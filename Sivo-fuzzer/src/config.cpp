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

#include "config.h"


//
// 
// Params
//
//

float   MIN_TIME_BETWEEN_INDEX_REDO      = 20 * 60 ;  
float   TIME_BETWEEN_INDEX_CHANGE; 
float   tot_index_change_time             = 0;
int     REINIT_STAGES                     = 0;
bool    RE_INIT_APPLY                      = true;
bool    RE_INIT_USE                        = true;
int     RE_INIT_REPEATS                    = 0;//8;
int     RE_CURR_REPEATS                    = 0;
int     RE_INIT_TIME_SEC                   = 15 * 60;
int     prev_reinit_solution_id            = 100000;
int     prev_real_solution_id              = 0;

float   TIME_BETWEEN_MAX_LEN_INCR           = 20 * 60 ;  
float   MIN_PERCT_NEW_TC                    = 1.02;

bool    USE_VANILLA_FUZZERS                 = true;
bool    USE_DATAFLOW_FUZZERS                = true;
bool    USE_IMPROVED_INFERENCE              = true;
bool    USE_LEARNING                        = true;

 
int     MAX_STORE_BRANCHES                              = 500000 ; 
int     MAX_DEPENDENCIES_PER_BRANCH                     = 8;        
int     MAX_DEPENDENCIES_INTERMEDIATE_STORE_PER_BRANCH  = 512; 
int     MAX_SOLUTIONS_PER_BRANCH_ADDRESS                = 100;
int     MAX_START_ANALYZE_BRANCHES                      = 100000; 


int     MAX_TESTCASE_SIZE       =   500000;
int     MAX_LENGTH_PROPOSE      =     1000;
int     MAX_LENGTH_TOTAL        =  1000000;
Time    time_change_max_length;
int     last_length_tc          = 0;

int MAX_PROCESS_TESTCASES_PER_RUN = 10000;


// force expansion of testcases to minimal size (filled with 0s )
bool    EXPAND_TESTCASE_SIZE        = false;
int     MIN_EXPANDED_TESTCASE_SIZE  = 1000;

// store solution (for fuzzer mingle) if it does not have more than MAX_ONE_SOLUTION_SIZE bytes
// store at most MAX_STORE_NUMBER_SOLUTIONS solutions
int     MAX_ONE_SOLUTION_SIZE       =   100;        
int     MAX_STORE_NUMBER_SOLUTIONS  = 50000;

// minimization parameters
int     MIN_SIZE_TO_MINIMIZE                = 10000;
float   MIN_RATIO_TO_MINIMIZE               = 0.75;
float   MAX_FRAC_SOL_TO_TIME_TO_MINIMIZE    = 0.1;

// used when computing score of branches (far fetched, most likely useless)
int RANDOM_WALK_LENGTH = 10;


// do not use fork server
int     USE_NO_FORK_SERVER = false;

// do not take into account edge multiplicity in the showmap
bool    ONLY_EDGE_COVERAGE          = false;

// how much of fuzzer calls should be based exclusivly on success based in line to ONLY_EDGE_COVERAGE
double FUZZER_COVERAGE_RATIO        = 0.10 ;

// timeouts
float TIMEOUT_FUZZER_ROUND          = 200;
float TIMEOUT_SWITCH_FUZZ           = 1.5;

float MAX_TIMEOUT_RATIO             = 0.05 ; 

float TIMEOUT_EXECUTION             = 2.0; 
float STORE_TIMEOUT_EXECUTION;
float MAX_TIMEOUT_INCREASE_EXEC     = 0.2;  
float MAX_TIMEOUT_EXECUTION         = 2.0; 
float MAX_EXTERNAL_TC_TIMEOUT_EXEC  = 2.0;  

float FACTOR_INCREASE_TIMEOUT       = 50.0; 


int TC_SAMPLE_WAIT_ITER_BEFORE_RESAMPLE_COUNT   = 3;
int TC_SAMPLE_MIN_PERCT_USE_TIME                = 1; 


//  one fuzzer call params. not fixed, changed later by the code
int NO_ITERATIONS       = 1024; 
int NO_EXECS_BLOCK      =    4; 

// store testcases that lead to crashes / hangs
bool STORE_CRASHES      = true;
bool STORE_TIMEOUTS     = true;

// logging
int PRINT_DEBUG_SYSTEM              = 0;
int PRINT_DEBUG_INFER_CONSTRAINTS   = 0;
int PRINT_FUZZER_ITERATIONS         = 0;
int PRINT_FUZZER_BASIC_INFOS        = 1;
int PRINT_FOUND_EDGES               = 0;
int PRINT_FOUND_TESTCASES           = 0;
int PRINT_ADDED_TESTCASES           = 0;
int PRINT_TESTCASES_INFO            = 0;
int PRINT_DEPENDENCY_INFERENCE_INFO = 0;
int PRINT_DEBUG_FUZZER_BYTES        = 0;
int PRINT_DEBUG_FUZZER_MINGLE       = 0;
int PRINT_DEBUG_FUZZER_SYSTEM       = 0;
int PRINT_DEBUG_FUZZER_BRANCH       = 0;
int PRINT_DEBUG_MAB                 = 0;
int PRINT_DEBUG_INFERRED_BYTES      = 0;



// length fuzzer 
int  FUZZ_COPYREMOVE_CHECK_CHOOSE_LEN       = 5000;

// Minimize TC
int   THRESH_FOR_OVERSAMPLE                 = 10;
float MIN_TIME_BETWEEN_MINIMIZATION_TC      = 300; 
float MIN_TIME_BETWEEN_MINIMIZATION_TC_T    = 300; 


// MAB
float       Gamma                           = 0.99; 
int         MAB_WINDOW_SIZE                 = 100;
uint32_t    SAMPLE_RANDOMLY_UNTIL_ROUND     = 20;



//
//
// Global vars
//
// 

string FOLDER_AUX;      /* initialized in main()  */
string OUTPUT_FOLDER;   /* initialized in main()  */
string FUZZER_OUTPUT_FOLDER_NAME = "outputs";

vector<char *> target_comm;     /* holds target binary launch command */

int FORKSRV_FD[] = { 198, 200};
uint64_t execution_types[2] = {0,0};

int MAP_COVERAGE_INDEX = 0;

string TMP_INPUT;
int fd_tmp_input;

double      coverage_update_time       = 0;
uint64_t    coverage_update_total      = 0;
uint64_t    coverage_update_total_bb   = 0;
uint64_t    trace_lengths              = 0;
double      fuzzer_init_time           = 0;
float       tot_min_tc_time            = 0;

testcase_file *current_testcase = NULL;
map < string , interim_testcase > itest;


uint64_t    hash_showmap;
float       found_new_cov;
bool        last_exec_timed_out;
uint32_t    glob_iter;

int         crash_id                   = 0;
int         crash_uniq                 = 0;

bool        processing_crash           = false;
bool        processing_timeout_max     = false;

int         fuzzer_type;
int         fuzzer_current_main;
int         fuzzer_current_sub;

uint64_t    tc_sampling_used[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// Multi Armed bandits 
bool        SAMPLE_RANDOMLY_THIS_ROUND              = false;
bool        SAMPLE_RANDOMLY_THIS_ROUND_NO_UPDATE    = false;
bool        SAMPLE_NOT_RANDOMLY                     = false;


// discounting stuff
vector < node_mab * >       all_mabs; 
int                         discount_times              = 0;
int                         discount_times_cov          = 0;
int                         discount_times_other        = 0;



double   counts_chars[256];
double   freqs_chars[256];
double   freqs_chars_avg;


int chosen_testcase_method;
int used_sampling_method;
bool coverage_type;


vector < map<int,int> > all_found_solutions;
vector < map<int,int> > tc_found_solutions;

map < int, string > sol_type_id_to_str; 


