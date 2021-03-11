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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "config_llvm.h"


uint32_t __branch_trace_init [MAX_BRANCHES_INSTRUMENTED];
uint32_t __branch_taken_init [MAX_BRANCHES_INSTRUMENTED];
uint32_t __branch_values1_init[MAX_BRANCHES_INSTRUMENTED];
uint32_t __branch_values2_init[MAX_BRANCHES_INSTRUMENTED];
double   __branch_valuesdouble_init[MAX_BRANCHES_INSTRUMENTED];
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
  if (getenv(EN_VAR)) {                                      \
    uint32_t shm_id = atoi(getenv(EN_VAR));                  \
    _var = (_type *) shmat(shm_id, NULL, 0);                 \
    if (_var == (void *)-1) _exit(1);                        \
  }                                                          \
  else{                                                      \
    _var = (_type *)malloc( sizeof(_type) * _size );         \
  }

/* (non-)persistent mode response to fuzzer wake-up call *
 * depends on ENV_VAR_PERSISTENT                         */
int (*wakeup_handler)(int stdin_tty, char *input_path, int actforksrv_fd);

/* API wrapper - to be called in-process */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
// {
//     return 0;
// }


/* set_shared_memory - initializes shared memory environment
 */
static void set_shared_memory() {
    SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_TRACE, "branchtrace",
        __branch_trace, uint32_t, MAX_BRANCHES_INSTRUMENTED );
    SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_TAKEN, "branchttaken",
        __branch_taken, uint32_t, MAX_BRANCHES_INSTRUMENTED );
    SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_VALUE1, "branchvalue1",
        __branch_values1, uint32_t, MAX_BRANCHES_INSTRUMENTED );
    SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_VALUE2, "branchvalue2",
        __branch_values2, uint32_t, MAX_BRANCHES_INSTRUMENTED );
    SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_DOUBLE, "branchvaluedouble",
        __branch_valuesdouble, double, MAX_BRANCHES_INSTRUMENTED );
    SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_OPERTIONS, "branchoperations",
        __branch_operations, uint32_t, MAX_BRANCHES_INSTRUMENTED );  
    SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_INDEX, "branchindex",
        __new_branch_index, uint32_t, 1 );
    SHM_OR_MALLOC(SHM_ENV_VAR_BRANCH_INSTRUMENT, "branchinstrument",
        __branch_instrument, uint32_t, 8 );
    SHM_OR_MALLOC(SHM_ENV_VAR_AFL_MAP, "aflmap",
        __afl_area_ptr, uint8_t, MAP_SIZE );

    __new_branch_index[0]  = 0;
    __branch_instrument[0] = 0;
    __branch_instrument[1] = 2;

    __branch_instrument[2] = 0xffffffff;
    __branch_instrument[3] = 0;
    __branch_instrument[4] = 0;
    __branch_instrument[5] = 0;
}

/* termination_handler - SIGTERM handler
 *  @signum : signal number
 *
 * When in persistent mode, the executing process must not be killed on
 * timeout. So in stead of SIGKILL, we will receive the SIGTERM maskable
 * interrupt.
 */
void termination_handler(int signum)
{
    /* TODO: dark magic to revert from wrapper early      *
     * NOTE: remember to close any rogue file descriptors */
}

/* handle_fork - non-persistent mode response to fuzzer wake-up call
 *  @stdin_tty     : 0 if file backed; !0 if tty backed
 *  @input_path    : NULL (unused)
 *  @actforksrv_fd : active forkserver file descriptor
 *
 *  @return: !0 (child)  to break out of main()'s instrumentation
 *            0 (parent) to remain inside the instrumented loop
 */
int handle_fork(int stdin_tty, char *input_path, int actforksrv_fd)
{ 
    int child_pid, status, ans;

    /* spawn child */
    child_pid = fork();
    if (child_pid < 0) {
        printf("[ERROR] Unable to fork (%d)\n", errno);
        _exit(1);
    }

    /* child: clean up inherited state and ask to break pre-main() loop */
    if (!child_pid) {
        /* if stdin redirected from file, rewind cursor to start */
        if (!stdin_tty) {
            ans = lseek(STDIN_FILENO, 0, SEEK_SET);
            if (ans == -1) {
                printf("[ERROR] Unable to rewind stdin (%d)\n", errno);
                _exit(1);
            }
        }

        /* reset shared map */
        __afl_prev_loc = 0;
        memset(__afl_area_ptr, 0, MAP_SIZE );

        /* terminate child communication to forkserver */
        close(actforksrv_fd);
        close(actforksrv_fd + 1);
    
        /* break out of pre-main() loop */
        return 1;
    }   

    /* parent: wait for child and report to server */
    if (write(actforksrv_fd + 1, &child_pid, 4) != 4)
        _exit(1);
    if (waitpid(child_pid, &status, 0) < 0)
        _exit(1);
    if (write(actforksrv_fd + 1, &status, 4) != 4)
        _exit(1);

    /* parent doesn't get to be free */
    return 0;
}

/* handle_wrapper - persistent mode response to fuzzer wake-up call
 *  @stdin_tty     : 0 if file backed; !0 if tty backed (unused)
 *  @input_path    : path to testcase file containing input
 *  @actforksrv_fd : active forkserver file descriptor
 *
 *  @return : 0 to remain inside the instrumented loop; never !0
 */
int handle_wrapper(int stdin_tty, char *input_path, int actforksrv_fd)
{
    /* avoid reallocating buffers as much as possible */
    static uint8_t *data = NULL;
    static size_t  size  = 0;

    struct stat    statbuf;
    ssize_t        rb;
    size_t         trb;
    int            fd, status, my_pid, ans;

    /* if stdin redirected from file, rewind cursor to start */
    if (!stdin_tty) {
        ans = lseek(STDIN_FILENO, 0, SEEK_SET);
        if (ans == -1) {
            printf("[ERROR] Unable to rewind stdin (%d)\n", errno);
            _exit(1);
        }
    }

    /* open input file */
    fd = open(input_path, O_RDONLY);
    if (fd == -1) {
        printf("[ERROR] Unable to open input: %s (%d)\n", input_path, errno);
        _exit(1);
    }

    /* get input file size */
    ans = fstat(fd, &statbuf);
    if (ans == -1) {
        printf("[ERROR] Unable to stat input file (%d)\n", errno);
        _exit(-1);
    }

    /* resize buffer if necessary */
    if (statbuf.st_size > size) {
        data = (uint8_t *) realloc(data, statbuf.st_size);
        if (!data) {
            printf("[ERROR] Unable to allocate memory (%d)\n", errno);
            _exit(-1);
        }
    }

    /* read data from disk to buffer                                     *
     * NOTE: consider possiblity that file may be a pipe, so read() may  *
     *       not be able to fetch statbuf.st_size bytes in one go; this  *
     *       should not be treated as an error                           */
    trb = 0;
    while (trb != statbuf.st_size) {
        rb = read(fd, data, statbuf.st_size - trb);
        if (rb == -1) {
            printf("[ERROR] Unable to read from input file (%d)\n", errno);
            exit(-1);
        }

        trb += rb;
    }

    /* cleanup */
    ans = close(fd);
    if (ans == -1) {
        printf("[ERROR] Unable to close input file (%d)\n", errno);
        _exit(-1);
    }

    /* report ourselves as the executing process */
    my_pid = getpid();
    if (write(actforksrv_fd + 1, &my_pid, 4) != 4) {
        printf("[ERROR] Unable to send first report (%d)\n", errno);
        _exit(1);
    }

    /* synchronously call fuzzed API element */
    LLVMFuzzerTestOneInput(data, statbuf.st_size);

    /* "child" finished normally */
    status = 0;
    if (write(actforksrv_fd + 1, &status, 4) != 4) {
        printf("[ERROR] Unable to send second report (%d)\n", errno);
        _exit(1);
    }

    return 0;
}

/* start_forkserver - pre-main() infinite loop
 *
 * invoked by constructor when the object is loaded in memory.
 * communicates with the fuzzer and starts instances to run new testcases.
 * ENV_VAR_PERSISTENT defines two modes of operation:
 *  - if  NULL : process is forked and the child exits this function and reaches
 *               main(). this process will wait for child to finish. fuzzer can
 *               kill child via SIGTERM on timeout
 *  - if !NULL : the env variable contains the path to the testcase; no child
 *               will be spawned. in stead, the fuzzing will take place using
 *               the LLVMFuzzerTestOneInput() wrapper over the tested API.
 */
static void start_forkserver()
{
    struct sigaction action;
    static uint8_t   tmp[4];
    char             *persistent_path = NULL;
    uint32_t         was_killed;
    int              server_id = 0; 
    int              stdin_tty, ans;

    /* test if program is running as part of fork server */
    if (getenv(ENV_VAR_MY_SERVER_ID))
        server_id = atoi(getenv(ENV_VAR_MY_SERVER_ID));

    int actforksrv_fd = FORKSRV_FD + 2*server_id;
    if (write(actforksrv_fd + 1, tmp, 4) != 4)
        return;

    /* test once and for all if stdin is tty backed or file backed */
    stdin_tty = isatty(STDIN_FILENO);

    /* fetch ENV_VAR_PERSISTENT and set wake-up call handler accordingly */
    persistent_path = getenv(ENV_VAR_PERSISTENT);
    wakeup_handler = persistent_path ? handle_wrapper : handle_fork;

    /* if in persistent mode, set up a SIGTERM handler */
    action.sa_handler = persistent_path ? termination_handler : SIG_DFL;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    ans = sigaction(SIGTERM, &action, NULL);
    if (ans == -1)
        _exit(1);

    /* fork client main loop */
    while (1) {
        /* server died (maybe) */
        if (read(actforksrv_fd, &was_killed, 4) != 4)
            _exit(1);

        /* handle the wake-up call */
        ans = wakeup_handler(stdin_tty, persistent_path, actforksrv_fd);
        if (ans)
            return;
    }
}


__attribute__((constructor(0))) void __afl_auto_init(void) 
{
  set_shared_memory();
  start_forkserver();
}


