/*
Copyright (c) 2021, Ivica Nikolic <cube444@gmail.com> and Radu Mantu <radu.mantu@upb.ro>

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


#ifndef FORK_SERVERS
#define FORK_SERVERS

#include <iostream>
#include <stdio.h>          
#include <stdint.h>         
#include <fcntl.h>          
#include <unistd.h>         
#include <signal.h>     
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <dirent.h>
#include <sched.h>
#include <ctype.h>
#include <errno.h>
#include "util.h"  
#include <string>
#include <vector>
#include "typez.h"

using namespace std;

extern uint32_t *branches_trace  ;
extern uint32_t *branches_taken  ;
extern uint32_t __branch_index;
extern uint32_t *__new_branch_index;
extern uint32_t *__branch_instrument;
extern uint32_t *branches_value1  ;
extern uint32_t *branches_value2  ;
extern double   *branches_valuedouble  ;
extern uint32_t *branches_operations  ;
extern uint32_t *store_branches_trace  ;
extern uint32_t *store_branches_taken  ;
extern uint32_t *store_branches_opera  ;
extern int store_branches_tot;
extern uint64_t tot_fork_executions;
extern uint64_t tot_norm_executions;
extern vector <double> time_to_execute[2];

vector < vector<int>  >  fork_get_branches( ) ;
void setup_signal_handlers(void) ;
void setup_shmem();
void setup_fork_servers();
bool init_fork_servers(string tmp_input);
void fini_fork_servers();
int run_one_fork( int type_of_run );
bool last_execution_timed_out();
void bind_to_free_cpu();

#endif
