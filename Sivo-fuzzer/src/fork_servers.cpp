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

// parts of the code below is based on AFL's code
/*
   american fuzzy lop - fuzzer code
   --------------------------------

   Written and maintained by Michal Zalewski <lcamtuf@google.com>

   Forkserver design by Jann Horn <jannhorn@googlemail.com>

   Copyright 2013, 2014, 2015, 2016, 2017 Google Inc. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   This is the real deal: the program takes an instrumented binary and
   attempts a variety of basic fuzzing tricks, paying close attention to
   how they affect the execution path.

 */

         
#include "fork_servers.h"    
#include "misc.h"
#include <chrono>
#include <vector>
#include "executions.h"
#include "config.h" 
#include "solutions.h"


using namespace std;

double      tot_fuzz_time                   = 0;
uint64_t    tot_fork_executions             = 0;
uint64_t    tot_norm_executions             = 0;
uint64_t    tot_branches_tried_to_invert    = 0;

static int forksrv_pid[2],
        fsrv_ctl_fd[2],   
        fsrv_st_fd[2],
        dev_urandom_fd = -1,       
        dev_null_fd = -1,
        out_fd[2];
           
uint8_t child_timed_out, stop_soon, last_crashed;
int status;
uint64_t timeouts=0;
bool last_is_timeout_max;
int cpu_core_count;
static int cpu_aff = -1; 

char *bin_path1 = NULL;
char *bin_path2 = NULL;
char *args1[256] = { NULL, NULL, NULL};
char *args2[256] = { NULL, NULL, NULL};
pid_t child_pid[2] = {0,0};
bool child_killed = false;
int latest_server_id = 0;
static int32_t fd_ctl[2];
static int32_t fd_log[2];

static int shm_id;                    
uint32_t *branches_trace  ;
uint32_t *branches_taken  ;
uint32_t *__new_branch_index;
uint32_t *branches_instrument;
uint32_t *branches_value1  ;
uint32_t *branches_value2  ;
double   *branches_valuedouble  ;
uint32_t *branches_operations  ;
uint32_t *store_branches_trace  ;
uint32_t *store_branches_taken  ;
uint32_t *store_branches_opera  ;
int store_branches_tot;

static uint64_t res_runs = 0;
static uint64_t res_timo = 0;
bool processing_temp_timeout = false;
#define QTS 100
static uint8_t queue_timeouts[QTS];
static uint32_t times_used_temp_timeout = 0;
static uint32_t times_successfull_used_temp_timeout = 0;

vector <double> time_to_execute[2];

uint8_t *aflshowmap;


static void handle_stop_sig(int sig) {

  stop_soon = 1; 

  if ( child_pid[latest_server_id] > 0)     kill(child_pid[latest_server_id],   SIGKILL);
  if ( forksrv_pid[latest_server_id] > 0)   kill(forksrv_pid[latest_server_id], SIGKILL);

}

static void handle_timeout(int sig) 
{

    if (child_pid[latest_server_id] > 0) {

        child_timed_out = 1; 
        kill(child_pid[latest_server_id], SIGKILL);

    } else if (child_pid[latest_server_id] == -1 && forksrv_pid[latest_server_id] > 0) {

        child_timed_out = 1; 
        kill(forksrv_pid[latest_server_id], SIGKILL);
    }
}

void setup_signal_handlers(void) 
{
  struct sigaction sa;
  sa.sa_handler   = NULL;
  sa.sa_flags     = SA_RESTART;
  sa.sa_sigaction = NULL;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = handle_stop_sig;
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sa.sa_handler = handle_timeout;
  sigaction(SIGALRM, &sa, NULL);

}

void setup_fork_servers()
{
    dev_null_fd = open("/dev/null", O_RDWR);
    if (dev_null_fd == -1) {
        printf("[ERROR] could not open /dev/null (%d)\n", errno);
        exit(-1);
    }
    memset( queue_timeouts, 0, QTS );
}



void setup_shmem()
{

        
#define SHM_( _ENV, _var, _varstr, _type, _siz )                                        \
    shm_id = shmget(IPC_PRIVATE, (_siz)*sizeof(_type), IPC_CREAT | IPC_EXCL | 0600);    \
    if (shm_id == -1) {                                                                 \
        printf("[ERROR] could not shmget (%d)\n", errno);                               \
        exit(-1);                                                                       \
    }                                                                                   \
    printf("Shm_id " _varstr " : %d\n", shm_id);                                        \
    sprintf( shm_str, "%d", shm_id );                                                   \
    setenv(_ENV, (const char*)shm_str, 1);                                              \
    _var = (_type *)shmat(shm_id, NULL, 0);                                             
   

    char *shm_str = (char * ) malloc( 100 );
   
    SHM_( SHM_ENV_VAR_AFL_MAP,          aflshowmap,             "aflshowmap",           uint8_t,    (1<<16) );
    SHM_( SHM_ENV_VAR_BRANCH_TRACE,     branches_trace,         "branches_trace",       uint32_t,   MAX_BRANCHES_INSTRUMENTED );
    SHM_( SHM_ENV_VAR_BRANCH_TAKEN,     branches_taken,         "branches_taken",       uint32_t,   MAX_BRANCHES_INSTRUMENTED );
    SHM_( SHM_ENV_VAR_BRANCH_VALUE1,    branches_value1,        "branches_value1",      uint32_t,   MAX_BRANCHES_INSTRUMENTED );
    SHM_( SHM_ENV_VAR_BRANCH_VALUE2,    branches_value2,        "branches_value2",      uint32_t,   MAX_BRANCHES_INSTRUMENTED );
    SHM_( SHM_ENV_VAR_BRANCH_DOUBLE,    branches_valuedouble,   "branches_valuedouble", double,     MAX_BRANCHES_INSTRUMENTED );
    SHM_( SHM_ENV_VAR_BRANCH_OPERTIONS, branches_operations,    "branches_operations",  uint32_t,   MAX_BRANCHES_INSTRUMENTED );
    SHM_( SHM_ENV_VAR_BRANCH_INDEX,     __new_branch_index,     "__new_branch_index",   uint32_t,   1 );
    SHM_( SHM_ENV_VAR_BRANCH_INSTRUMENT,branches_instrument,    "branches_instrument",  uint32_t,   8 );


    free(shm_str);


}


/*
 * init_one_server - initializes each forkserver individually
 *      @bin_path      : path to binary (relative or absolute)
 *      @args          : arguments passed to binary (w/ binary name)
 *      @server_id     : 0 or 1, deciding functionality
 *      @replace_stdin : target binary takes input directly from stdin
 *      @tmp_input     : if @replace_stdin is true, redirect this to stdin
 *
 *      @return        :
 */
bool init_one_server(char *bin_path,
                     char *args[],
                     int  server_id,
                     bool replace_stdin,
                     char *tmp_input)
{
    static struct itimerval it;
    int st_pipe[2], ctl_pipe[2];
    int status;
    int rlen;
    int ans, fd;

    printf("Starting fork server  %d\n", server_id);

    if (pipe(st_pipe) || pipe(ctl_pipe))
        printf("pipe() failed\n");

    char se_id[10];
    snprintf(se_id, sizeof(se_id), "%d", server_id); 
    setenv(ENV_VAR_MY_SERVER_ID, se_id, 1);

    printf("Preparing to fork...\n");

    forksrv_pid[server_id] = fork();

    if (forksrv_pid[server_id] < 0)
        printf("fork() failed\n");

    printf("forksrv_pid[server_id] = %d\n", forksrv_pid[server_id]); 

    if (!forksrv_pid[server_id]) {
        printf("Executable: %s :  ", bin_path);
        int ii =0 ;
        while( args[ii] != NULL)
            printf("%s ", args[ii++] );
        printf("\n");

        setsid();

        // set to false to show the output
        if(true)
        {
            dup2(dev_null_fd, STDOUT_FILENO);
            dup2(dev_null_fd, STDERR_FILENO);
        }
        
        /* Set up control and status pipes, close the unneeded original fds. */
        if (dup2(ctl_pipe[0], FORKSRV_FD[server_id]) < 0)
            printf("dup2() failed\n");
        if (dup2(st_pipe[1], FORKSRV_FD[server_id] + 1) < 0)
            printf("dup2() failed\n");

        close(ctl_pipe[0]);
        close(ctl_pipe[1]);
        close(st_pipe[0]);
        close(st_pipe[1]);
        close(dev_null_fd);

        /* redirect tmp_input to stdin                *
         * forkserver must rewind cursort at each run */
        if (replace_stdin) {
            printf("Redirecting stdin input from file\n");

            /* open input file */
            fd = open(tmp_input, O_RDONLY|O_CREAT, 0666);
            if (fd == -1) {
                printf("ERROR: could not open tmp_input (%d)\n", errno);
                exit(-1);
            }

            /* replace stdin with tmp_input fd (stdin closed silently) */
            ans = dup2(fd, STDIN_FILENO);
            if (ans == -1) {
                printf("ERROR: could not dup2 tmp_input (%d)\n", errno);
                exit(-1);
            }

            /* close the original fd, now we have it as 0 */
            ans = close(fd);
            if (ans == -1) {
                printf("ERROR: could not close old tmp_input (%d)\n", errno);
                exit(-1);
            }
        }

        printf("We are now executing target...\n");
        execv(bin_path, args);

        printf(KERR "Execv in fork failed. Exiting ...\n" KNRM );

        exit(0);
    }

    printf("Main fuzzer handling post-fork of forkserver\n");

    /* Close the unneeded endpoints. */
    close(ctl_pipe[0]);
    close(st_pipe[1]);

    fsrv_ctl_fd[server_id] = ctl_pipe[1];
    fsrv_st_fd[server_id]  = st_pipe[0];

    /* Wait for the fork server to come up, but don't wait too long. */
    it.it_value.tv_sec = int(TIMEOUT_EXECUTION);
    it.it_value.tv_usec = int(TIMEOUT_EXECUTION * 1000000) % 1000000;

    setitimer(ITIMER_REAL, &it, NULL);

    rlen = read(fsrv_st_fd[server_id], &status, 4);

    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = 0;

    setitimer(ITIMER_REAL, &it, NULL);

    printf("Main fuzzer read %d bytes from forkserver: %08x\n", rlen, status);
    
    if (rlen == 4) {
        printf("Fork server is up.\n");
        return true;
    }

    if (child_timed_out)
        printf("Timeout while initializing fork server (adjusting -t may help)\n");

    if (waitpid(forksrv_pid[server_id], &status, 0) <= 0)
        printf("waitpid() failed\n");

    printf("Fork server crashed with signal %d\n", WTERMSIG(status));

    return false;

}


int run_one_fork(  int type_of_run  ) 
{

    int server_id = !!type_of_run;

    latest_server_id                = server_id;
    static uint32_t prev_timed_out  = 0;
    child_timed_out                 = 0;
    last_crashed                    = 0;
    last_is_timeout_max             = false;        
    int res;


    execution_types[ server_id ] ++;

    START_TIMER(begin_time)

    // set map coverage index ( 0,1,2, or 3)
    // helps to avoid collisions in AFL map
    branches_instrument[2] = 0; 
    branches_instrument[3] = 0; 
    branches_instrument[4] = 0; 
    branches_instrument[5] = 0; 
    branches_instrument[2+MAP_COVERAGE_INDEX] = 0xffffffff; 



    // set instrumentation bit & load prev trace (if forced exec)
    if( RUN_FORCER ==  type_of_run ){
        branches_instrument[0] = EXECUTION_INSTRUMENT_BRANCHES;
        load_from_stored_branch_trace();
    }
    else
        branches_instrument[0] = EXECUTION_DO_NOT_INSTRUMENT_BRANCHES;

    // set 0 to the last array element to see if __new_branch_index overflows
    branches_trace[ MAX_BRANCHES_INSTRUMENTED - 1 ] = 0;
    
    
    // set trace index
    __new_branch_index[0] = 0;

    // set path length (modulo) -- this is relic 
    branches_instrument[1] = 0; 


    if( USE_NO_FORK_SERVER){

        child_pid[server_id] = fork();
        if (child_pid[server_id] < 0){
            printf("fork() failed");
            exit(1);
        }

        if (!child_pid[server_id]) {

            printf("\n\n ############## NO FORK ##############\n\n");

            setsid();

            dup2(dev_null_fd, 1);
            dup2(dev_null_fd, 2);                
            close(dev_null_fd);
            close(dev_urandom_fd);

            if( 0 == server_id )
                execv(bin_path1, args1);
            else
                execv(bin_path2, args2);

        }


    }
    else{


        if ((res = write(fsrv_ctl_fd[server_id], &prev_timed_out, 4)) != 4) {
            if (stop_soon) return 0;
            printf("Unable to request new process from fork server (OOM?)\n"); fflush(stdout);
            exit(1);
        }

        if ((res = read(fsrv_st_fd[server_id], &(child_pid[server_id]), 4)) != 4) {
            if (stop_soon) return 0;
            printf("Unable to request new process from fork server (OOM?)\n"); fflush(stdout);
            exit(1);
        }

        if (child_pid[server_id] <= 0){ 
            printf("Fork server is misbehaving (OOM?)\n"); fflush(stdout);
            exit(1);
        }

    }


    static struct itimerval it;
    float FINAL_TIMEOUT = mmin( MAX_TIMEOUT_EXECUTION, 
                                TIMEOUT_EXECUTION * (processing_temp_timeout ? FACTOR_INCREASE_TIMEOUT : 1.0 ) );
    it.it_value.tv_sec  = int( FINAL_TIMEOUT );
    it.it_value.tv_usec = int( FINAL_TIMEOUT * 1000000) % 1000000;

    setitimer(ITIMER_REAL, &it, NULL);
        

    if ( USE_NO_FORK_SERVER ){

        tot_norm_executions ++;

        if (waitpid(child_pid[server_id], &status, 0) <= 0){ 
            printf("waitpid() failed");
            exit(1);
        }
    }
    else {

        tot_fork_executions++;

        if ((res = read(fsrv_st_fd[server_id], &status, 4)) != 4) {
            if (stop_soon) return 0;
            printf("Unable to communicate with fork server (OOM?)\n"); fflush(stdout);
            exit(2);
        }
    }



    if (!WIFSTOPPED(status)) child_pid[server_id] = 0;

    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = 0;

    setitimer(ITIMER_REAL, &it, NULL);

    // there was overflow in the instruction counter
    if( 0 != branches_trace[ MAX_BRANCHES_INSTRUMENTED - 1 ] ){
        __new_branch_index[0] = MAX_BRANCHES_INSTRUMENTED - 1;
        printf( KINFO "Overflow of instruction counter \n" KNRM );  fflush(stdout);
    }

    // store time of execution
    double one_time= TIME_DIFFERENCE_SECONDS(begin_time);
    tot_fuzz_time += one_time;
    time_to_execute[server_id].push_back( one_time );
    if( time_to_execute[server_id].size() > 1000 )  // keep vector size small
        time_to_execute[server_id].erase( time_to_execute[server_id].begin() );




    //
    //
    // Crash processing
    // 
    //

    // if processing crash, the run_one_fork was called from process_crash, 
    // so just return 
    if( processing_crash ){
        processing_crash = false;
        //return WIFSIGNALED(status) && !stop_soon && !child_timed_out;
        return false;
    }


    // if crash occured, and it it not forced execution or timeout, 
    // then process the crash
    if (WIFSIGNALED(status) && !stop_soon && !child_timed_out && !processing_temp_timeout   
        && ( RUN_TRACER == type_of_run || RUN_GETTER == type_of_run )
    ){
        process_crash( WTERMSIG(status) );
        return !child_timed_out;
    }






    //
    //
    // Timeout (aka hangs) processing
    // 
    //

    
    // if processing timeout normal( i.e. incrase timeout to see if you can execute ), 
    // then report if the new execution was successfull
    if( processing_temp_timeout ){
        printf( KINFO "Prolonged exec successfull: %d\n" KNRM,  !child_timed_out ); fflush(stdout);
        times_successfull_used_temp_timeout += !child_timed_out;

        if( FINAL_TIMEOUT / MAX_TIMEOUT_EXECUTION >= 0.9 )
            last_is_timeout_max = true;

    }

    res_runs ++; 
    timeouts += !!child_timed_out;

    queue_timeouts[ res_runs % int(FACTOR_INCREASE_TIMEOUT) ] = 0;
    bool prev_processing_temp_timeout = processing_temp_timeout;
    processing_temp_timeout = false;
    bool already_proc_time = false;


    // if processing timeout_max (i.e. given timeout close to the max )
    if( processing_timeout_max || FINAL_TIMEOUT / MAX_TIMEOUT_EXECUTION >= 0.99 ){

        already_proc_time   = true;

        // if timeouted then store it
        if( child_timed_out )
            store_timeout_testcase();
        
        // if called from process_timeout then return 
        if( processing_timeout_max){
            processing_timeout_max = false;
            return false;
        }
    }




    if( child_timed_out && !already_proc_time ){

        // Adjust timeout time if needed. 
        // If the ratio of timeouts exceeds MAX_TIMEOUT_RATIO
        // then increase the timeout by 50% as long as it does not exceed MAX_TIMEOUT_INCREASE_EXEC
        if ( TIMEOUT_EXECUTION < MAX_TIMEOUT_INCREASE_EXEC ){
            res_timo ++;
            if( res_timo > 10 && (float)res_timo/res_runs > MAX_TIMEOUT_RATIO ){
                res_timo = res_runs = 0;
                TIMEOUT_EXECUTION *= 1.5;
                if( TIMEOUT_EXECUTION > MAX_TIMEOUT_INCREASE_EXEC )
                    TIMEOUT_EXECUTION = MAX_TIMEOUT_INCREASE_EXEC;
                printf( KINFO "[%8d] Increase timeout to %.3f \n" KNRM, glob_iter, TIMEOUT_EXECUTION );  fflush(stdout);
            }
        }
        
        // process timeout (if not currently processing)
        if( !prev_processing_temp_timeout )
            process_timeout( type_of_run, res_runs, res_timo, QTS, queue_timeouts, 
                        times_successfull_used_temp_timeout, times_used_temp_timeout  );

    }

    

    prev_timed_out = child_timed_out;

    return !child_timed_out;


}


/*
 * init_fork_servers - composes correct arguemtns; wrapper for init_one_server()
 *  @tmp_input : std::string containing the name of the actual input file
 *  @return    : 0 if everything went ok (checks that return 1 missing for now)
 *
 * NOTE: potential memory leak due to malloc() and strdup() buffers not freed
 *       not a problem since this function is not called more than once
 *       keep in mind if this ever changes
 */
bool init_fork_servers(string tmp_input)
{
    size_t len;
    bool   replace_stdin = true;

    USE_NO_FORK_SERVER = true;     /* state until successful init */

    /* compose binary path variations for "-1" and "-2" *
     * can be relative                                  */
    len = strlen(target_comm[0]) + 3;
    bin_path1 = (char *) malloc(len);
    bin_path2 = (char *) malloc(len);

    snprintf(bin_path1, len, "%s-1", target_comm[0]);
    snprintf(bin_path2, len, "%s-2", target_comm[0]);

    /* identify strictly the base binary name          *
     * compose binay name variations for "-1" and "-2" */
    char *bin_name = strrchr(target_comm[0], '/');
    bin_name = (bin_name == NULL) ? target_comm[0] : bin_name + 1;

    len = strlen(bin_name) + 3;
    char *bin_name_1 = (char *) malloc(len);
    char *bin_name_2 = (char *) malloc(len);

    snprintf(bin_name_1, len, "%s-1", bin_name);
    snprintf(bin_name_2, len, "%s-2", bin_name);

    /* create arguments for each binary variation       *
     * NOTE: global args1, args2 will be reused at exec */
    args1[0] = bin_name_1;
    args2[0] = bin_name_2;

    for (size_t i=1; i<target_comm.size(); ++i) {
        if (!strncmp(target_comm[i], "@@", 3)) {
            args1[i] = strdup(tmp_input.c_str());
            args2[i] = strdup(tmp_input.c_str());
            
            replace_stdin = false;

            continue;
        }

        args1[i] = strdup(target_comm[i]);
        args2[i] = strdup(target_comm[i]);
    }

    /* initialize individual fork servers */
    if (!init_one_server(bin_path1, args1, 0, replace_stdin, strdup(tmp_input.c_str())))
        return false;
    if (!init_one_server(bin_path2, args2, 1, replace_stdin, strdup(tmp_input.c_str())))
        return false;

    USE_NO_FORK_SERVER  = false;
    last_crashed        = 0;

    return true;
}

void fini_fork_servers()
{
    for (size_t i=0; i<256; ++i) {
        if (args1[i]) {
            free(args1[i]);
            args1[i] = NULL;
        }

        if (args2[i]) {
            free(args2[i]);
            args2[i] = NULL;
        }
    }

   free(bin_path1);
   free(bin_path2);
}



bool last_execution_timed_out()
{
    return child_timed_out;
}




void bind_to_free_cpu() 
{

  DIR* d;
  struct dirent* de;
  cpu_set_t c;

  uint8_t cpu_used[4096] = { 0 };
  uint32_t i;

  FILE* f = fopen("/proc/stat", "r");
  char tmp[1024];
  if (!f) return;
  while (fgets(tmp, sizeof(tmp), f))
    if (!strncmp(tmp, "cpu", 3) && isdigit(tmp[3])) cpu_core_count++;
  fclose(f);


  if (cpu_core_count < 2) return;

  d = opendir("/proc");

  if (!d) {

    printf("Unable to access /proc - can't scan for free CPU cores.\n");
    return;

  }

  printf("Checking CPU core loadout...\n");

  /* Introduce some jitter, in case multiple AFL tasks are doing the same
     thing at the same time... */

  usleep( (random() % 1000 ) * 250);

  /* Scan all /proc/<pid>/status entries, checking for Cpus_allowed_list.
     Flag all processes bound to a specific CPU using cpu_used[]. This will
     fail for some exotic binding setups, but is likely good enough in almost
     all real-world use cases. */

  while ((de = readdir(d))) {

    char* fn;
    FILE* f;
    char tmp[8192];
    uint8_t has_vmsize = 0;

    if (!isdigit(de->d_name[0])) continue;

    fn = (char *)malloc( 1024 );
    sprintf(fn, "/proc/%s/status", de->d_name);

    if (!(f = fopen(fn, "r"))) {
      free(fn);
      continue;
    }


    while (fgets(tmp, sizeof(tmp), f)) {

      uint32_t hval;

      /* Processes without VmSize are probably kernel tasks. */

      if (!strncmp(tmp, "VmSize:\t", 8)) has_vmsize = 1;

      if (!strncmp(tmp, "Cpus_allowed_list:\t", 19) &&
          !strchr(tmp, '-') && !strchr(tmp, ',') &&
          sscanf(tmp + 19, "%u", &hval) == 1 && hval < sizeof(cpu_used) &&
          has_vmsize) {

        cpu_used[hval] = 1;
        break;

      }

    }

    free(fn);
    fclose(f);

  }

  closedir(d);

  for (i = 0; i < cpu_core_count; i++) if (!cpu_used[i]) break;

  if (i == cpu_core_count) {
    printf("No more free CPU cores\n");
  }

  printf("Found a free CPU core, binding to #%u.\n", i);

  cpu_aff = i;

  CPU_ZERO(&c);
  CPU_SET(i, &c);

  if (sched_setaffinity(0, sizeof(c), &c)){
    printf("sched_setaffinity failed\n");
    exit(0);
    }

}
