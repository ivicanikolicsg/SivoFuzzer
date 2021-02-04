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


#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <chrono>
#include "MersenneTwister.h"
#include "typez.h"
#include "defs.h"

using namespace std;

extern MTRand mtr;

extern uint8_t *aflshowmap;
extern uint8_t showmap[1<<16];


extern float MIN_TIME_BETWEEN_INDEX_REDO; 
extern float TIME_BETWEEN_INDEX_CHANGE; 
extern float tot_index_change_time;
extern int   REINIT_STAGES;

extern float TIME_BETWEEN_MAX_LEN_INCR;  
extern float MIN_PERCT_NEW_TC;



extern bool RE_INIT_APPLY          ;
extern bool RE_INIT_USE            ;
extern int  RE_INIT_REPEATS        ;
extern int  RE_CURR_REPEATS        ;
extern int  RE_INIT_TIME_SEC       ;
extern int  prev_reinit_solution_id;
extern int  prev_real_solution_id;



extern vector<char *> target_comm;  /* target launch command */


extern bool coverage_type;

extern float                    Gamma; 
extern vector < node_mab * >    all_mabs; 
extern int                      discount_times;
extern int                      discount_times_cov;
extern int                      discount_times_other;


extern uint32_t SAMPLE_RANDOMLY_UNTIL_ROUND;
extern bool     SAMPLE_RANDOMLY_THIS_ROUND;
extern bool     SAMPLE_RANDOMLY_THIS_ROUND_NO_UPDATE;
extern bool     SAMPLE_NOT_RANDOMLY;

extern uint32_t tot_vanilla_runs;
extern uint32_t tot_dataflow_runs;


extern double   coverage_update_time;
extern uint64_t coverage_update_total;
extern uint64_t coverage_update_total_bb;
extern uint64_t trace_lengths;

extern int      fuzzer_type;
extern int      fuzzer_current_main;
extern int      fuzzer_current_sub;
extern double   fuzzer_init_time;

extern bool     processing_crash;
extern bool     processing_timeout_max;
extern bool     processing_temp_timeout;
extern uint8_t  last_crashed;

extern uint64_t hash_showmap;

extern double   frac_time_tc_sample;
extern double   MAX_FRAC_TIME_TC_SAMPLE;
extern uint64_t tc_sampling_used[20];

extern double   FUZZER_COVERAGE_RATIO;
extern float    found_new_cov;


extern int FORKSRV_FD[2];
extern uint64_t execution_types[2];
extern int USE_NO_FORK_SERVER;

extern int MAP_COVERAGE_INDEX;


extern float    write_speed ;
extern uint64_t write_length;
extern uint64_t tot_writes_to_file;

extern vector <int> unmutable;
extern bool unmutable_inferred;


extern vector <branch_address_pair> branches_with_dependency; 
extern VM branch_addresses;

extern uint8_t ectb[256];
extern uint32_t cur_iter;
extern int glob_found_different;


extern Time start_fuzzing_time;

extern int MAB_WINDOW_SIZE;


extern int TC_SAMPLE_WAIT_ITER_BEFORE_RESAMPLE_COUNT;
extern int TC_SAMPLE_MIN_PERCT_USE_TIME;

extern int chosen_testcase_method;
extern int used_sampling_method;

extern byte *differences[MAX_NO_DIFF];


extern int PRINT_DEBUG_SYSTEM;
extern int PRINT_DEBUG_INFER_CONSTRAINTS;

extern int  FUZZ_COPYREMOVE_CHECK_CHOOSE_LEN;

extern int   THRESH_FOR_OVERSAMPLE;
extern float MIN_TIME_BETWEEN_MINIMIZATION_TC;
extern float MIN_TIME_BETWEEN_MINIMIZATION_TC_T; 
extern float tot_min_tc_time;

extern int MAX_STORE_BRANCHES;
extern int MAX_START_ANALYZE_BRANCHES;

extern int solution_id;
extern int hang_id;

extern bool last_is_timeout_max;
extern int solutions_on_timeout;

extern uint64_t timeouts;


extern string FOLDER_AUX;
extern string OUTPUT_FOLDER;
extern string FUZZER_OUTPUT_FOLDER_NAME;

extern string BINARY;
extern string TMP_INPUT;
extern int fd_tmp_input;
extern vector <string> params_before;
extern vector <string> params_after;


extern int MAX_TESTCASE_SIZE;
extern int MAX_LENGTH_PROPOSE;
extern int MAX_LENGTH_TOTAL;
extern Time time_change_max_length;
extern int MAX_PROCESS_TESTCASES_PER_RUN;
extern int last_length_tc;


extern bool EXPAND_TESTCASE_SIZE;
extern int  MIN_EXPANDED_TESTCASE_SIZE;

extern int MAX_ONE_SOLUTION_SIZE;
extern int MAX_STORE_NUMBER_SOLUTIONS;

extern int    MIN_SIZE_TO_MINIMIZE;
extern float  MIN_RATIO_TO_MINIMIZE;

extern int RANDOM_WALK_LENGTH;

extern int MAX_SOLUTIONS_PER_BRANCH_ADDRESS;
extern int MAX_DEPENDENCIES_PER_BRANCH;
extern int MAX_DEPENDENCIES_INTERMEDIATE_STORE_PER_BRANCH;


extern float MAX_FRAC_SOL_TO_TIME_TO_MINIMIZE;


extern uint32_t glob_iter;

extern int crash_id;
extern int crash_uniq;


extern map < int , int >            solution_types;
extern map < int , set<uint32_t> >  solution_types_iter;
extern map < int , int >            solution_first;

extern bool last_exec_timed_out;


extern vector < map<int,int> > all_found_solutions;
extern vector < map<int,int> > tc_found_solutions;

extern double   counts_chars[256];
extern double   freqs_chars[256];
extern double   freqs_chars_avg;

extern bool ONLY_EDGE_COVERAGE;

extern bool STORE_CRASHES;
extern bool STORE_TIMEOUTS;

extern float TIMEOUT_FUZZER_ROUND;
extern float MAX_TIMEOUT_RATIO;
extern float TIMEOUT_EXECUTION;
extern float STORE_TIMEOUT_EXECUTION;
extern float MAX_TIMEOUT_EXECUTION; 
extern float MAX_TIMEOUT_INCREASE_EXEC;
extern float MAX_EXTERNAL_TC_TIMEOUT_EXEC;
extern float TIMEOUT_SWITCH_FUZZ;
extern float FACTOR_INCREASE_TIMEOUT;


extern int NO_ITERATIONS;
extern int NO_EXECS_BLOCK;

extern float    fuzzer_individual_times[10];
extern uint32_t fuzzer_individual_called[10];

extern double tot_fuzz_time;
extern double tot_sol_add_time;


extern int PRINT_FUZZER_ITERATIONS;
extern int PRINT_FUZZER_BASIC_INFOS;
extern int PRINT_FOUND_EDGES;
extern int PRINT_FOUND_TESTCASES;
extern int PRINT_ADDED_TESTCASES;
extern int PRINT_TESTCASES_INFO;
extern int PRINT_DEPENDENCY_INFERENCE_INFO;
extern int PRINT_DEBUG_FUZZER_BYTES;
extern int PRINT_DEBUG_FUZZER_MINGLE;
extern int PRINT_DEBUG_FUZZER_SYSTEM;
extern int PRINT_DEBUG_FUZZER_BRANCH;
extern int PRINT_DEBUG_MAB;
extern int PRINT_DEBUG_INFERRED_BYTES;

extern testcase_file *current_testcase;
extern map < string , interim_testcase > itest;

extern map < int, string > sol_type_id_to_str; 

extern bool USE_VANILLA_FUZZERS;
extern bool USE_DATAFLOW_FUZZERS;
extern bool USE_IMPROVED_INFERENCE;
extern bool USE_LEARNING;




#endif
