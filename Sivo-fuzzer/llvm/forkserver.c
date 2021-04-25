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
   american fuzzy lop - LLVM instrumentation bootstrap
   ---------------------------------------------------

   Written by Laszlo Szekeres <lszekeres@google.com> and
              Michal Zalewski <lcamtuf@google.com>

   LLVM integration design comes from Laszlo Szekeres.

   Copyright 2015, 2016 Google Inc. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   This code is the rewrite of afl-as.h's main_payload.

*/


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "config_llvm.h"


uint32_t __branch_trace_init [MAX_BRANCHES_INSTRUMENTED];
uint32_t __branch_taken_init [MAX_BRANCHES_INSTRUMENTED];
uint32_t __branch_values1_init[MAX_BRANCHES_INSTRUMENTED];
uint32_t __branch_values2_init[MAX_BRANCHES_INSTRUMENTED];
double __branch_valuesdouble_init[MAX_BRANCHES_INSTRUMENTED];
uint32_t __branch_operations_init[MAX_BRANCHES_INSTRUMENTED];
uint32_t __new_branch_trace_init[1];
uint32_t __branch_instrument_init [8];

uint32_t *__branch_trace =__branch_trace_init;
uint32_t *__branch_taken =__branch_taken_init;
uint32_t *__branch_values1 = __branch_values1_init;
uint32_t *__branch_values2 = __branch_values2_init;
double   *__branch_valuesdouble = __branch_valuesdouble_init;
uint32_t *__branch_operations = __branch_operations_init;
uint32_t *__new_branch_index = __new_branch_trace_init;
uint32_t *__branch_instrument=__branch_instrument_init;

uint8_t  __afl_area_initial[ MAP_SIZE ];
uint8_t *__afl_area_ptr = __afl_area_initial;
__thread uint32_t __afl_prev_loc;




#define SHM_OR_MALLOC(EN_VAR, _CHARNAME, _var, _type, _size) \
  if (getenv(EN_VAR)) {                         \
    uint32_t shm_id = atoi(getenv(EN_VAR));     \
    _var = shmat(shm_id, NULL, 0);              \
    if (_var == (void *)-1) _exit(1);           \
  }                                             \
  else{                                         \
    _var = (_type *)malloc( sizeof(_type) * _size );  \
  }



static void set_shared_memory() {


  SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_TRACE, "branchtrace", __branch_trace, uint32_t, MAX_BRANCHES_INSTRUMENTED );
  SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_TAKEN, "branchttaken", __branch_taken, uint32_t, MAX_BRANCHES_INSTRUMENTED );
  SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_VALUE1, "branchvalue1", __branch_values1, uint32_t, MAX_BRANCHES_INSTRUMENTED );
  SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_VALUE2, "branchvalue2", __branch_values2, uint32_t, MAX_BRANCHES_INSTRUMENTED );
  SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_DOUBLE, "branchvaluedouble", __branch_valuesdouble, double, MAX_BRANCHES_INSTRUMENTED );
  SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_OPERTIONS, "branchoperations", __branch_operations, uint32_t, MAX_BRANCHES_INSTRUMENTED );  
  SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_INDEX, "branchindex", __new_branch_index, uint32_t, 1 );
  SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_INSTRUMENT, "branchinstrument", __branch_instrument, uint32_t, 8 );
  SHM_OR_MALLOC(SHM_ENV_VAR_AFL_MAP, "aflmap", __afl_area_ptr, uint8_t, MAP_SIZE );

  __new_branch_index[0]   = 0;
  __branch_instrument[0]  = 0;
  __branch_instrument[1]  = 2;

  __branch_instrument[2]  = 0xffffffff;
  __branch_instrument[3]  = 0;
  __branch_instrument[4]  = 0;
  __branch_instrument[5]  = 0;

}


static void start_forkserver()
{
    static uint8_t tmp[4];
    int child_pid;
    int server_id = 0; 
    int stdin_tty, ans;

    /* test if program is running as part of fork server */
    if (getenv(ENV_VAR_MY_SERVER_ID))
        server_id = atoi(getenv(ENV_VAR_MY_SERVER_ID));

    int ACTFORKSRV_FD = FORKSRV_FD + 2*server_id;
    if (write(ACTFORKSRV_FD + 1, tmp, 4) != 4)
        return;

    /* test once and for all if stdin is tty backed or file backed */
    stdin_tty = isatty(STDIN_FILENO);

    /* fork client main loop */
    while (1) {
        uint32_t was_killed;
        int status;

        /* server died (maybe) */
        if (read(ACTFORKSRV_FD, &was_killed, 4) != 4)
            _exit(1);

        /* spwan child from pre-main() checkpoint */
        child_pid = fork();
        if (child_pid < 0)
            _exit(1);

        /* child process cleanup */
        if (!child_pid) {
            /* if stdin redirected from file, rewind cursor to start *
             * if pipe, lseek will fail but that's ok                */
            if (!stdin_tty)
                ans = lseek(STDIN_FILENO, 0, SEEK_SET);

            /* reset shared map */
            __afl_prev_loc = 0;
            memset(__afl_area_ptr, 0, MAP_SIZE );


            /* terminate child communication to forkserver */
            close(ACTFORKSRV_FD);
            close(ACTFORKSRV_FD + 1);
       
            /* break out of fork client loop */
            return;
        }

        /* fork client must wait for child and report to server */
        if (write(ACTFORKSRV_FD + 1, &child_pid, 4) != 4)
            _exit(1);
        if (waitpid(child_pid, &status, 0) < 0)
            _exit(1);
        if (write(ACTFORKSRV_FD + 1, &status, 4) != 4)
            _exit(1);
    }
}



__attribute__((constructor(0))) void __afl_auto_init(void) 
{

  set_shared_memory();
  start_forkserver();
}


