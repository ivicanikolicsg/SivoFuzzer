
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
#include "llvm/Support/raw_ostream.h"

 #include "llvm/Transforms/Scalar/ADCE.h"
 #include "llvm/ADT/DenseMap.h"
 #include "llvm/ADT/DepthFirstIterator.h"
 #include "llvm/ADT/GraphTraits.h"
 #include "llvm/ADT/MapVector.h"
 #include "llvm/ADT/PostOrderIterator.h"
 #include "llvm/ADT/SetVector.h"
 #include "llvm/ADT/SmallPtrSet.h"
 #include "llvm/ADT/SmallVector.h"
 #include "llvm/ADT/Statistic.h"
 #include "llvm/Analysis/GlobalsModRef.h"
 #include "llvm/Analysis/IteratedDominanceFrontier.h"
 #include "llvm/Analysis/PostDominators.h"
 #include "llvm/IR/BasicBlock.h"
 #include "llvm/IR/CFG.h"
 #include "llvm/IR/DebugInfoMetadata.h"
 #include "llvm/IR/DebugLoc.h"
 #include "llvm/IR/Dominators.h"
 #include "llvm/IR/Function.h"
 #include "llvm/IR/IRBuilder.h"
 #include "llvm/IR/InstIterator.h"
 #include "llvm/IR/InstrTypes.h"
 #include "llvm/IR/Instruction.h"
 #include "llvm/IR/Instructions.h"
 #include "llvm/IR/IntrinsicInst.h"
 #include "llvm/IR/PassManager.h"
 #include "llvm/IR/Use.h"
 #include "llvm/IR/Value.h"
 #include "llvm/InitializePasses.h"
 #include "llvm/Pass.h"
 #include "llvm/ProfileData/InstrProf.h"
 #include "llvm/Support/Casting.h"
 #include "llvm/Support/CommandLine.h"
 #include "llvm/Support/Debug.h"
 #include "llvm/Support/raw_ostream.h"
 #include "llvm/Transforms/Scalar.h"


using namespace llvm;
using namespace std;

namespace {

  class EnforcePass : public ModulePass {


    private:

      void instrument_one_trace(Module &M, IRBuilder<> &IRB, BasicBlock &BB, Instruction *last_instruction, uint32_t use_address,
        GlobalVariable *BranchTracePtr, 
        GlobalVariable *BranchTakenPtr, 
        GlobalVariable *BranchValue1Ptr,
        GlobalVariable *BranchValue2Ptr,
        GlobalVariable *BranchValueDoublePtr,
        GlobalVariable *BranchOperationPtr,
        GlobalVariable *New_Branch,
        GlobalVariable *ExtraParams
      );

      void instrument_one_coverage( 
        Module &M, IRBuilder<> &IRB, BasicBlock &BB, Instruction *last_instruction, uint32_t use_address, 
        GlobalVariable *AFLMapPtr,
        GlobalVariable *AFLPrevLoc,
        GlobalVariable *ExtraParams      
      );


    uint32_t inst_branches = 0;
    uint32_t inst_real_branches = 0;
    uint32_t inst_if_type = 0;
    uint32_t inst_switch_type = 0;


    public:

      static char ID;
      EnforcePass() : ModulePass(ID) { }
      bool runOnModule(Module &M) override;

  };
}

  

char EnforcePass::ID = 0;


void EnforcePass::instrument_one_trace( Module &M, IRBuilder<> &IRB, BasicBlock &BB, 
  Instruction *last_instruction, uint32_t use_address,
  GlobalVariable *BranchTracePtr, 
  GlobalVariable *BranchTakenPtr, 
  GlobalVariable *BranchValue1Ptr,
  GlobalVariable *BranchValue2Ptr,
  GlobalVariable *BranchValueDoublePtr,
  GlobalVariable *BranchOperationPtr,
  GlobalVariable *New_Branch,
  GlobalVariable *ExtraParams
 )
{
      LLVMContext &C = M.getContext();
      IntegerType *Int32Ty = IntegerType::getInt32Ty(C);


      // add code before last_instruction
      IRB.SetInsertPoint(last_instruction);


      ConstantInt *CurBranch = ConstantInt::get(Int32Ty, use_address  );

      // Current index of the trace 
      LoadInst *New_Branch_Ptr = IRB.CreateLoad(New_Branch);
      New_Branch_Ptr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));   

      
      Value *New_Branch_Index = IRB.CreateGEP(New_Branch_Ptr, ConstantInt::get(Int32Ty, 0));
      LoadInst *New_Branch_Value = IRB.CreateLoad(New_Branch_Index);
      New_Branch_Value->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      
      

      Value *New_Branch_Casted = IRB.CreateZExt(New_Branch_Value, IRB.getInt32Ty());
      Value *New_Branch_Incr = IRB.CreateAnd( 
            IRB.CreateAdd(New_Branch_Casted, ConstantInt::get(Int32Ty, 1))
            , ConstantInt::get(Int32Ty, MAX_BRANCHES_INSTRUMENTED - 1) );
      IRB.CreateStore(New_Branch_Incr, New_Branch_Index);//->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));


      // Branch trace : just store the branch ID
      LoadInst *IBranchTracePtr = IRB.CreateLoad(BranchTracePtr);
      IBranchTracePtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      

      Value *BranchTracePtrIdx = IRB.CreateGEP(IBranchTracePtr, New_Branch_Casted);
      IRB.CreateStore( CurBranch , BranchTracePtrIdx)->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));


      //
      // Conditional branch or switch
      //
      Value *BranchVal          =  ConstantInt::get(Int32Ty, 0);
      Value *Branch_value1      =  ConstantInt::get(Int32Ty, 0);
      Value *Branch_value2      =  ConstantInt::get(Int32Ty, 0);
      Value *Branch_value_float =  ConstantFP::get( Type::getDoubleTy(C) , 0);
       
      // Type of the last instruction: 0 - nothing, 1 - conditional branch, 2- switch
      uint32_t typeOfBranch = 0;
      uint32_t branch_oper  =  0;     
      bool branch_found = false;
      bool switch_found = false;
      auto *branch  = dyn_cast<BranchInst>(last_instruction);
      auto *switcc  = dyn_cast<SwitchInst>(last_instruction);

      int used_regs = 0;
      int used_regs_float = 0;
      uint32_t mask_used_branchval = 0;
      uint32_t mask_used_branch_value1 = 0;
      uint32_t mask_used_branch_value2 = 0;
      uint32_t mask_used_branch_double = 0;


      if( branch && branch->isConditional() ) {

        int cond_branch_cmp_instruction_type = 0;
        int cond_branch_cmp_instruction_predicate_type = 0;

        inst_if_type++;
        inst_real_branches++;

        branch_found = true;
        typeOfBranch = 1<<16;

        mask_used_branchval = 1<<24;
        BranchVal =  IRB.CreateZExt( branch->getCondition() , IRB.getInt32Ty());   

        // Find where (which instruction) the conditional branch condition (1-bit) is defined
        // Used later to capture the operands/variables that can influence the conditional branch condition  
        Instruction *instr_defines_cond_branch = (dyn_cast<Instruction>(last_instruction))->getPrevNode();
        bool found = false;
        while( instr_defines_cond_branch ){

          for (Use &use: instr_defines_cond_branch->uses()){
              User *user = use.getUser();
              unsigned n = use.getOperandNo();
              Instruction* userInst = dyn_cast<Instruction>(user);
              if( last_instruction == userInst && n == 0){
                found = true;
                break;
              }
          }
          if( found ) break;
          instr_defines_cond_branch = instr_defines_cond_branch->getPrevNode();
        }

        if( instr_defines_cond_branch ){

          // capture opcope of the instruction
          branch_oper    = instr_defines_cond_branch->getOpcode()<<8;   
          cond_branch_cmp_instruction_type = instr_defines_cond_branch->getOpcode();

          // if icmp, then capture the predicate too (icmp_sgt, icmp_ne, ...)
          CmpInst *cp = dyn_cast<CmpInst>(instr_defines_cond_branch);
          if( cp != 0){ 
            branch_oper ^= cp->getPredicate(); 
            cond_branch_cmp_instruction_predicate_type = cp->getPredicate();
          }


          // analyze all operands of the instruction that defines the conditional branch condition
          for( uint32_t j=0; j<instr_defines_cond_branch->getNumOperands(); j++)
          {
              auto op = instr_defines_cond_branch->getOperand(j);

              Value *cur_val;
              Value *cur_val_float;
              bool found_good_int = false;
              bool found_good_float = false;


              // If integer, then store 
              if( op->getType()->isIntegerTy() )  
              {
                  cur_val = op;
                  found_good_int = true;
              }
              // if pointer to integer, then dereference and store
              else if (op->getType()->isPointerTy() ){
                  auto *optype = cast<PointerType>(op->getType());
                  auto eltype = optype->getElementType ();
                
                  if( eltype == Type::getInt8Ty(C) || 
                    eltype == Type::getInt16Ty(C) || 
                    eltype == Type::getInt32Ty(C) || 
                    eltype == Type::getInt64Ty(C)  
                  ){
                      cur_val  = IRB.CreatePtrToInt(op, eltype);
                      found_good_int = true;
                  }
              }
              else if( op->getType()->isFloatTy() ||  op->getType()->isDoubleTy() ){
                      cur_val_float = IRB.CreateFPExt( op, IRB.getDoubleTy() ) ;
                      found_good_float = true;
              }



              if( found_good_int){

                  // extend to 64-bit and trunc to 32-bit 
                  // this makes sure that all values are 32-bit regardless of the starting bit size

                  cur_val = IRB.CreateZExt ( cur_val, IRB.getInt64Ty()) ;
                  cur_val = IRB.CreateTruncOrBitCast( cur_val, IRB.getInt32Ty() );

                  if( 0 == used_regs){
                    Branch_value1 = cur_val;
                    used_regs++;
                  }
                  else if( 1 == used_regs){
                    Branch_value2 = cur_val;
                    used_regs++;
                  }
                  else{
                    Branch_value2 = IRB.CreateAdd( Branch_value2, cur_val );
                  }
              }

              if( found_good_float){
                  
                  if( 0 == used_regs_float ){
                    Branch_value_float = cur_val_float;
                    used_regs_float++;
                  }
                  else{
                    Branch_value_float = IRB.CreateFAdd( Branch_value_float, cur_val_float);
                  }
              }
          }
        }

        if (getenv(ENV_VAR_BINARY_NAME) && getenv(ENV_VAR_CALL_PATH) ){
          char llvm_filename[1024];
          sprintf(llvm_filename, "%s/llvm-ifs-%s",  getenv(ENV_VAR_CALL_PATH),  getenv(ENV_VAR_BINARY_NAME) );
          FILE *f = fopen(llvm_filename,"a+");
          fprintf(f,"%x %x %x\n",use_address, cond_branch_cmp_instruction_type, cond_branch_cmp_instruction_predicate_type );
          fclose(f);
        }
      }

      else if ( switcc ) {

          inst_switch_type ++;
          inst_real_branches++;
          switch_found = true;

          typeOfBranch = 2<<16;

          int numsuc_max255 = switcc->getNumSuccessors() >= 256 ? 255 :  switcc->getNumSuccessors(); 
          int numcas_max255 = switcc->getNumCases()      >= 256 ? 255 :  switcc->getNumCases(); 
          branch_oper = numcas_max255 << 8 ^ numsuc_max255;

          mask_used_branchval = 1<<24;

          BranchVal = switcc->getCondition () ;  
          BranchVal = IRB.CreateZExt(switcc->getCondition (), IRB.getInt64Ty());
          BranchVal = IRB.CreateTruncOrBitCast( BranchVal, IRB.getInt32Ty() );


          // switch cases
          if (getenv(ENV_VAR_BINARY_NAME) && getenv(ENV_VAR_CALL_PATH) ){
            char llvm_filename[1024];
            sprintf(llvm_filename, "%s/llvm-switches-%s",  getenv(ENV_VAR_CALL_PATH),  getenv(ENV_VAR_BINARY_NAME) );
            FILE *f = fopen(llvm_filename,"a+");
            fprintf(f,"%x %x %x", use_address, switcc->getNumSuccessors(), switcc->getNumCases() );
            for (auto i = switcc->case_begin(); i != switcc->case_end(); ++i)
              fprintf(f," %lx", i.getCaseValue()->getZExtValue() );
            fprintf(f,"\n");
            fclose(f);
          }
      }
      
      

      // Branch value 1
      if( used_regs > 0){
        mask_used_branch_value1 = 1<<25;
        LoadInst *IBranchValue1Ptr = IRB.CreateLoad(BranchValue1Ptr);
        IBranchValue1Ptr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      
        
        Value *BranchValue1PtrIdx = IRB.CreateGEP(IBranchValue1Ptr, New_Branch_Casted);
        IRB.CreateStore(Branch_value1, BranchValue1PtrIdx)->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      }

      // Branch value 2
      if( used_regs > 1){
        mask_used_branch_value2 = 1<<26;
        LoadInst *IBranchValue2Ptr = IRB.CreateLoad(BranchValue2Ptr);
        IBranchValue2Ptr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      

        Value *BranchValue2PtrIdx = IRB.CreateGEP(IBranchValue2Ptr, New_Branch_Casted);
        IRB.CreateStore(Branch_value2, BranchValue2PtrIdx)->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      }

      if( used_regs_float > 0)
      {
        mask_used_branch_double = 1<<27;
        LoadInst *IBranchValueDoublePtr = IRB.CreateLoad(BranchValueDoublePtr);
        IBranchValueDoublePtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      

        Value *BranchValueDoublePtrIdx = IRB.CreateGEP(IBranchValueDoublePtr, New_Branch_Casted);
        IRB.CreateStore(Branch_value_float, BranchValueDoublePtrIdx)->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));  
      }

      // Branch operations
      uint32_t allmasks = branch_oper ^ typeOfBranch;
      allmasks ^= mask_used_branchval;
      allmasks ^= mask_used_branch_value1;
      allmasks ^= mask_used_branch_value2;
      allmasks ^= mask_used_branch_double;
      
      LoadInst *IBranchOperationPtr = IRB.CreateLoad(BranchOperationPtr);
      IBranchOperationPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      

      Value *BranchOperationPtrIdx = IRB.CreateGEP(IBranchOperationPtr, New_Branch_Casted);
      IRB.CreateStore(ConstantInt::get(Int32Ty, allmasks ), BranchOperationPtrIdx)->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));


      // Get the original branches taken (used later for forced exec if any)
      // only if dealing with if or switch
      LoadInst *IBranchTakenPtr = IRB.CreateLoad(BranchTakenPtr);
      IBranchTakenPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      

      Value *BranchTakenPtrIdx = IRB.CreateGEP(IBranchTakenPtr, New_Branch_Casted);
      LoadInst *OldBranchTaken = IRB.CreateLoad(BranchTakenPtrIdx);
      OldBranchTaken->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      

      // Store current branchval
      IRB.CreateStore(BranchVal, BranchTakenPtrIdx)->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));


      if( branch_found   || switch_found  )
      {

        // Multiplexer: 
        // if instrument branches is set (stored in  ExtraParams ) 
        // then instrument, i.e. set the condition to the previous
        // otherwise, set the condition to the original
        // All bits of ExtraParams should be set (it is wordwise multiplex)
        LoadInst *IBranchInstrumentPtr = IRB.CreateLoad(ExtraParams);
        IBranchInstrumentPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));      

        Value *BranchInstrumentIdx  = IRB.CreateGEP(IBranchInstrumentPtr, ConstantInt::get( Int32Ty, 0 ) );
        Value *InsB32 = IRB.CreateZExt( IRB.CreateLoad(BranchInstrumentIdx) , IRB.getInt32Ty());
        
        
        Value *Orig32 = BranchVal;
        
        Value *Forc32 = IRB.CreateZExt(OldBranchTaken, IRB.getInt32Ty());
        
        // multipex: X&A + !X&B = (X&A) ^ (X^1)&B 
        Value *Res =  IRB.CreateXor( 
            IRB.CreateAnd( InsB32 , Forc32 ) , 
            IRB.CreateAnd( IRB.CreateXor(InsB32, ConstantInt::get(Int32Ty, 0xffffffff)) , Orig32  )
        );

        if( branch_found)   branch->setCondition( IRB.CreateTruncOrBitCast( Res, IRB.getInt1Ty() ) );
        
        else{ //switch
          // adjust the bit size to match the previous size of the condition
          auto tRes = IRB.CreateZExt(Res, IRB.getInt64Ty());      
          tRes = IRB.CreateTruncOrBitCast( tRes, switcc->getCondition()->getType()  );
          switcc->setCondition( tRes );
        } 
      }

}



void EnforcePass::instrument_one_coverage( 
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




bool EnforcePass::runOnModule(Module &M) {


  LLVMContext &C = M.getContext();
  IntegerType *Int32Ty  = IntegerType::getInt32Ty(C);
  IntegerType *Int8Ty  = IntegerType::getInt8Ty(C);

  IRBuilder<> IRB( C );


  GlobalVariable *BranchTracePtr =
      new GlobalVariable(M, PointerType::get(Int32Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__branch_trace");

  GlobalVariable *BranchTakenPtr =
      new GlobalVariable(M, PointerType::get(Int32Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__branch_taken");

  GlobalVariable *BranchValue1Ptr =
      new GlobalVariable(M, PointerType::get(Int32Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__branch_values1");

  GlobalVariable *BranchValue2Ptr =
      new GlobalVariable(M, PointerType::get(Int32Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__branch_values2");

  GlobalVariable *BranchValueDoublePtr =
      new GlobalVariable(M, PointerType::get( Type::getDoubleTy(C) , 0), false,
                         GlobalValue::ExternalLinkage, 0, "__branch_valuesdouble");

  GlobalVariable *BranchOperationPtr =
      new GlobalVariable(M, PointerType::get(Int32Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__branch_operations");

  GlobalVariable *New_Branch =
      new GlobalVariable(M, PointerType::get(Int32Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__new_branch_index");


  GlobalVariable *ExtraParams =
      new GlobalVariable(M, PointerType::get(Int32Ty, 0), false,
                          GlobalValue::ExternalLinkage, 0, "__branch_instrument");


  GlobalVariable *AFLMapPtr =
      new GlobalVariable(M, PointerType::get(Int8Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__afl_area_ptr");


  GlobalVariable *AFLPrevLoc = new GlobalVariable(
      M, Int32Ty, false, GlobalValue::ExternalLinkage, 0, "__afl_prev_loc",
      0, GlobalVariable::GeneralDynamicTLSModel, 0, false);



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

      instrument_one_coverage( M, IRB, BB, term_inst, start_addr_rand +  inst_branches, 
        AFLMapPtr, AFLPrevLoc, ExtraParams ) ;              

      
      instrument_one_trace( M, IRB, BB, term_inst, start_addr_rand + inst_branches, 
        BranchTracePtr, BranchTakenPtr, BranchValue1Ptr, BranchValue2Ptr,   BranchValueDoublePtr,   
        BranchOperationPtr, New_Branch, ExtraParams );
      
    }
    printf("[Branches pass for  %25s  ] : Addr %8x  : Tot %4d   : real %4d : ifs %4d  :  switches %4d\n", 
    M.getName().data(), start_addr_rand, 
    inst_branches,  inst_real_branches, inst_if_type, inst_switch_type  );



  // CFG for the module
  map <BasicBlock *, uint32_t> map_BB_to_addr;
  inst_branches = 0;
  for (auto &F : M)
    for (auto &BB : F) {
        
        inst_branches++;
        map_BB_to_addr.insert( {&BB, start_addr_rand + inst_branches});
    }
  if (getenv(ENV_VAR_BINARY_NAME) && getenv(ENV_VAR_CALL_PATH) ){

      char llvm_filename[1024];
      sprintf(llvm_filename, "%s/llvm-cfg-%s",  getenv(ENV_VAR_CALL_PATH),  getenv(ENV_VAR_BINARY_NAME) );
      FILE *f = fopen(llvm_filename,"a+");

      for (auto &F : M)
        for (auto &BB : F) {

            auto block_addr = map_BB_to_addr.find( &BB )->second;
            set <uint32_t> nexts;
            TerminatorInst *terminator = BB.getTerminator();
            for (uint32_t i=0; i < terminator->getNumSuccessors(); i++) {
              BasicBlock *Succ = terminator->getSuccessor(i);
              auto it = map_BB_to_addr.find ( Succ );
              if( it != map_BB_to_addr.end() )  
                nexts.insert( it->second );
            }


            fprintf(f,"%x %lx ", block_addr, nexts.size() );
            for(auto it= nexts.begin(); it!= nexts.end(); it++)
              fprintf(f,"%x ", *it);
            fprintf(f,"\n");
        }

      fclose(f);
  
  }

  return true;

}


static void registerEnforcePassPass(const PassManagerBuilder &,
                            legacy::PassManagerBase &PM) {

  PM.add(new EnforcePass());

}


static RegisterStandardPasses RegisterEnforcePassPass(
    PassManagerBuilder::EP_OptimizerLast, registerEnforcePassPass);

static RegisterStandardPasses RegisterEnforcePassPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerEnforcePassPass);
