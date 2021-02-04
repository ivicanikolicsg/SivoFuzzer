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


#ifndef DEFS_H
#define DEFS_H


#define MAX_BRANCHES_INSTRUMENTED ((1<<24) )   

#define RUN_TRACER 0
#define RUN_GETTER 1
#define RUN_FORCER 2

#define EXECUTION_INSTRUMENT_BRANCHES           (0xffffffff)
#define EXECUTION_DO_NOT_INSTRUMENT_BRANCHES    (0)

#define MAX_NO_DIFF 1


#define SHM_ENV_VAR_AFL_MAP               "__SHM_ID_AFL_MAP"
#define SHM_ENV_VAR_BRANCH_TRACE          "__SHM_ID_BRANCH_TRACE"
#define SHM_ENV_VAR_BRANCH_TAKEN          "__SHM_ID_BRANCH_TAKEN"
#define SHM_ENV_VAR_BRANCH_VALUE1         "__SHM_ID_BRANCH_VALUE1"
#define SHM_ENV_VAR_BRANCH_VALUE2         "__SHM_ID_BRANCH_VALUE2"
#define SHM_ENV_VAR_BRANCH_DOUBLE         "__SHM_ID_BRANCH_DOUBLE"
#define SHM_ENV_VAR_BRANCH_TAKEN_RES      "__SHM_ID_BRANCH_TAKEN_RES"
#define SHM_ENV_VAR_BRANCH_OPERTIONS      "__SHM_ID_BRANCH_OPERATIONS"
#define SHM_ENV_VAR_BRANCH_INDEX          "__SHM_ID_BRANCH_INDEX"
#define SHM_ENV_VAR_BRANCH_INSTRUMENT     "__SHM_ID_BRANCH_INSTRUMENT"
#define ENV_VAR_MY_SERVER_ID              "__MY_SERVER_ID"


#define START_TIMER(begin_time) \
      auto begin_time = std::chrono::high_resolution_clock::now();

#define GET_TIMER() \
    (std::chrono::high_resolution_clock::now())

#define TIME_DIFFERENCE_SECONDS(begin_time) \
    (std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - (begin_time) ).count()/1000000.0 )


#define UNPACK_BRANCH_TYPE(x)         ( ((x)>>16) & 0xff )
#define UNPACK_BRANCH_COMPARE_TYPE(x) ( ((x)>> 8) & 0xff )
#define UNPACK_BRANCH_COMPARE_OPER(x) ( ((x)>> 0) & 0xff )
#define BRANCH_TYPE_COND_BRANCH 1
#define BRANCH_TYPE_SWITCH 2
#define BRANCH_TYPE_NOBRANCH 0
#define IS_ICMP 0x33
#define IS_FCMP 0x34
#define UNPACK_SWITCH_NUM_SUC(x)         ( ((x)>>0) & 0xf )
#define UNPACK_SWITCH_NUM_CAS(x)         ( ((x)>>8) & 0xf )
#define USED_BRANCH_VAL1(x)   (((x)>>25) & 1)
#define USED_BRANCH_VAL2(x)   (((x)>>26) & 1)
#define USED_BRANCH_DOUBLE(x)   (((x)>>27) & 1)
#define USED_BRANCH_VAL_ANY(x)  (((x)>>25) & 0xff)
#define USED_BRANCH_VALS12(x) (((x)>>25) & 3)

#define BRANCH_IS_UNIMPORTANT(x) (UNPACK_BRANCH_TYPE(x) == BRANCH_TYPE_NOBRANCH)
#define BRANCH_IS_IMPORTANT(x) (UNPACK_BRANCH_TYPE(x) != BRANCH_TYPE_NOBRANCH)
#define BRANCH_IS_IF(x) (UNPACK_BRANCH_TYPE(x) == BRANCH_TYPE_COND_BRANCH)
#define BRANCH_IS_SWITCH(x) (UNPACK_BRANCH_TYPE(x) == BRANCH_TYPE_SWITCH)

#define OPERATION_IS_FCPM(x) ( (UNPACK_BRANCH_TYPE(x) == BRANCH_TYPE_COND_BRANCH)  && \
                           (UNPACK_BRANCH_COMPARE_TYPE(x) == IS_FCMP ) )

#define OPERATION_IS_SOME_CMP(x) \
  ( UNPACK_BRANCH_COMPARE_TYPE(x) == IS_FCMP  || UNPACK_BRANCH_COMPARE_TYPE(x) == IS_ICMP  )

#define LLVM_IF_PREFIX          "llvm-ifs-"
#define LLVM_SWITCH_PREFIX      "llvm-switches-"
#define LLVM_CFG_PREFIX         "llvm-cfg-"



#define SOL_TYPE_IMPORTED           0
#define SOL_TYPE_SOLVER_A           100
#define SOL_TYPE_SOLVER_B           101
#define SOL_TYPE_BRANCH_GA          250
#define SOL_TYPE_BRANCH_PRIMARY     310
#define SOL_TYPE_BRANCH_SECOND      320
#define SOL_TYPE_MUTATE_KNOWN       400
#define SOL_TYPE_MINGLER            500
#define SOL_TYPE_MINGLER_DUMB       501
#define SOL_TYPE_MINGLER_SMART      502
#define SOL_TYPE_MUTATE_RAND        600
#define SOL_TYPE_MUTATE_RAND1       601
#define SOL_TYPE_COPREM_RAND        700
#define SOL_TYPE_COPREM_REAL        701
#define SOL_TYPE_COPREM_RE_DIVS     702
#define SOL_TYPE_COPREM_RE_PREV     703
#define SOL_TYPE_COMBINER           800
#define SOL_TYPE_COMBINERSMART      801

#define SAMPLING_METHOD_UNSAMPLED       0
#define SAMPLING_METHOD_COUNT_VANILLA      1
#define SAMPLING_METHOD_SUCCESS_VANILLA    2
#define SAMPLING_METHOD_LENGTH_VANILLA     3
#define SAMPLING_METHOD_NEW_VANILLA        4
#define SAMPLING_METHOD_CRASH_VANILLA      5
#define SAMPLING_METHOD_RANDOM_VANILLA     6
#define SAMPLING_METHOD_COUNT_DATAFLOW     7
#define SAMPLING_METHOD_SUCCESS_DATAFLOW   8
#define SAMPLING_METHOD_LENGTH_DATAFLOW    9
#define SAMPLING_METHOD_NEW_DATAFLOW       10
#define SAMPLING_METHOD_CRASH_DATAFLOW     11
#define SAMPLING_METHOD_RANDOM_DATAFLOW    12

#define SUFFIX_COV  "+new"


#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KERR  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KINFO  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KSEC  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KIMP  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"




#endif