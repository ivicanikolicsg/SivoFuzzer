/*
Copyright (c) 2021, Ivica Nikolic <cube444@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// parts of the code below is based on AFL's code
/*
   american fuzzy lop - LLVM-mode instrumentation pass
   ---------------------------------------------------

   Written by Laszlo Szekeres <lszekeres@google.com> and
              Michal Zalewski <lcamtuf@google.com>

   LLVM integration design comes from Laszlo Szekeres. C bits copied-and-pasted
   from afl-as.c are Michal's fault.

   Copyright 2015, 2016 Google Inc. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   This library is plugged into LLVM when invoking clang through afl-clang-fast.
   It tells the compiler to add code roughly equivalent to the bits discussed
   in ../afl-as.h.

 */

#include "config_llvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <map>
#include <set>
#include <algorithm>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
//#include "llvm/Support/raw_ostream.h"


using namespace llvm;
using namespace std;

namespace {

  class CollectPass : public ModulePass {

    private:

    void instrument_one_coverage( 
      Module &M, IRBuilder<> &IRB2, BasicBlock &BB, Instruction *last_instruction, uint32_t use_address, 
      GlobalVariable *AFLMapPtr,
      GlobalVariable *AFLPrevLoc,
      GlobalVariable *ExtraParams
    );

    uint32_t inst_branches = 0;


    public:

      static char ID;
      CollectPass() : ModulePass(ID) { }
      bool runOnModule(Module &M) override;

  };
}


char CollectPass::ID = 0;


void CollectPass::instrument_one_coverage( 
  Module &M, IRBuilder<> &IRB, BasicBlock &BB, Instruction *last_instruction,  uint32_t use_address,
  GlobalVariable *AFLMapPtr,
  GlobalVariable *AFLPrevLoc,
  GlobalVariable *ExtraParams )
{

      LLVMContext &C          = M.getContext();
      IntegerType *Int8Ty     = IntegerType::getInt8Ty(C);
      IntegerType *Int16Ty    = IntegerType::getInt16Ty(C);
      IntegerType *Int32Ty    = IntegerType::getInt32Ty(C);

      // this fixes the random numbers in all passes as long as use_address is the same for the BB
      srand( use_address );

      // add code before last_instruction
      IRB.SetInsertPoint(last_instruction);

      // Params
      LoadInst *ParamsPtr   = IRB.CreateLoad( ExtraParams );

      // Load masks for labels (which are either 0 or 0xffffffff )
      Value *MaskIdx;
      MaskIdx               = IRB.CreateGEP(  ParamsPtr, ConstantInt::get( Int32Ty, 2 ) );
      Value *MaskMap1       = IRB.CreateZExt( IRB.CreateLoad(MaskIdx) , IRB.getInt32Ty());
      MaskIdx               = IRB.CreateGEP(  ParamsPtr, ConstantInt::get( Int32Ty, 3 ) );
      Value *MaskMap2       = IRB.CreateZExt( IRB.CreateLoad(MaskIdx) , IRB.getInt32Ty());
      MaskIdx               = IRB.CreateGEP(  ParamsPtr, ConstantInt::get( Int32Ty, 4 ) );
      Value *MaskMap3       = IRB.CreateZExt( IRB.CreateLoad(MaskIdx) , IRB.getInt32Ty());
      MaskIdx               = IRB.CreateGEP(  ParamsPtr, ConstantInt::get( Int32Ty, 5 ) );
      Value *MaskMap4       = IRB.CreateZExt( IRB.CreateLoad(MaskIdx) , IRB.getInt32Ty());


      // Load prev_loc map, word 
      LoadInst *PrevLoc = IRB.CreateLoad(AFLPrevLoc);
      PrevLoc->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      
      Value *PrevLocCasted = IRB.CreateZExt(PrevLoc, IRB.getInt32Ty());

      // Load SHM pointer 
      LoadInst *MapPtr = IRB.CreateLoad(AFLMapPtr);
      MapPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      

      ConstantInt *CurLocMap1 = ConstantInt::get(Int32Ty, random() % MAP_SIZE );
      ConstantInt *CurLocMap2 = ConstantInt::get(Int32Ty, random() % MAP_SIZE );
      ConstantInt *CurLocMap3 = ConstantInt::get(Int32Ty, random() % MAP_SIZE );
      ConstantInt *CurLocMap4 = ConstantInt::get(Int32Ty, random() % MAP_SIZE );

      // create the actual label (as M1&L1 + ... + M4&L4 )
      Value *V1         = IRB.CreateAnd( MaskMap1, CurLocMap1 );
      Value *V2         = IRB.CreateAnd( MaskMap2, CurLocMap2 );
      Value *V3         = IRB.CreateAnd( MaskMap3, CurLocMap3 );
      Value *V4         = IRB.CreateAnd( MaskMap4, CurLocMap4 );
      Value *MultiPlex  = IRB.CreateXor( IRB.CreateXor(V1,V2), IRB.CreateXor(V3,V4) );
      Value *MapPtrIdx1 = IRB.CreateGEP(MapPtr, IRB.CreateXor(PrevLocCasted, MultiPlex));
      
      // increase counter (but do not go over 128, thus stopping overflow)
      LoadInst *Counter1 = IRB.CreateLoad(MapPtrIdx1);
      Counter1->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));           
      Value *bit1281 = IRB.CreateLShr( IRB.CreateAnd( ConstantInt::get(Int8Ty, 128), Counter1), 
                      ConstantInt::get(Int8Ty, 7 ) );
      Value *Incr1 = IRB.CreateAdd( Counter1, IRB.CreateSub( ConstantInt::get(Int8Ty, 1) , bit1281 ) ); 

      IRB.CreateStore(Incr1, MapPtrIdx1)->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

      // store prev loc map
      IRB.CreateStore( IRB.CreateLShr( MultiPlex, ConstantInt::get(Int16Ty,  1 ) )  , AFLPrevLoc) ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      
}



bool CollectPass::runOnModule(Module &M) {


  LLVMContext &C = M.getContext();

  IntegerType *Int32Ty  = IntegerType::getInt32Ty(C);
  IntegerType *Int8Ty   = IntegerType::getInt8Ty(C);

  IRBuilder<> IRB2( C );

  GlobalVariable *AFLMapPtr =
      new GlobalVariable(M, PointerType::get(Int8Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__afl_area_ptr");

  GlobalVariable *AFLPrevLoc = new GlobalVariable(
      M, Int32Ty, false, GlobalValue::ExternalLinkage, 0, "__afl_prev_loc",
      0, GlobalVariable::GeneralDynamicTLSModel, 0, false);


  GlobalVariable *ExtraParams =
      new GlobalVariable(M, PointerType::get(Int32Ty, 0), false,
                          GlobalValue::ExternalLinkage, 0, "__branch_instrument");



  // create random 32-bit id for the basic block
  auto fn = M.getName	();
  uint32_t start_addr_rand = 0;
  for( uint32_t i=0; i<fn.size(); i++){
    start_addr_rand = 3*(start_addr_rand << 5 ^ start_addr_rand >> 27);
    start_addr_rand += fn[i];
  }


  for (auto &F : M)
    for (auto &BB : F) {

      inst_branches++;

      Instruction *term_inst = (dyn_cast<Instruction>(BB.getTerminator ()));
      if( term_inst  == nullptr ) continue;

      instrument_one_coverage( M, IRB2, BB, term_inst, start_addr_rand +  inst_branches, 
          AFLMapPtr, AFLPrevLoc, ExtraParams ) ;              
    }

  
  printf("[Coverage pass for  %25s  ] : Addr %8x  \n", M.getName().data(), start_addr_rand );


  return true;

}


static void registerCollectPassPass(const PassManagerBuilder &, legacy::PassManagerBase &PM) {

  PM.add(new CollectPass());

}


static RegisterStandardPasses RegisterCollectPassPass(
    PassManagerBuilder::EP_OptimizerLast, registerCollectPassPass);

static RegisterStandardPasses RegisterCollectPassPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerCollectPassPass);



