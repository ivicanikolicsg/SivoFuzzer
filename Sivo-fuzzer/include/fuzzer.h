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

#ifndef FUZZER_H
#define FUZZER_H

#include <fstream>
#include <sstream> 
#include <algorithm>
#include <random>
#include <map>
#include <string.h>
#include <stdio.h>       
#include <stdint.h>         
#include <fcntl.h>          
#include <unistd.h>         
#include <time.h>
#include <chrono>
#include <iterator>
#include <numeric>


#include "branches.h"
#include "nodes.h"
#include "solutions.h"
#include "solving_constraints.h"
#include "solving_constraints.h"
#include "multi_arm_banditos.h"
#include "executions.h"
#include "fork_servers.h"
#include "config.h"
#include "typez.h"


extern uint64_t tot_branches_tried_to_invert;

extern map < int, uint32_t > byte_to_branches;
extern map <uint32_t, uint32_t> traceId_to_branch;
extern map < branch_address_pair, uint32_t> bAddr_to_traceId;
extern set <int> set_all_dep_bytes;

extern vector < targetNode > branch_candidates;
extern vector < pair<branch_address_pair,int> > ordered_suggested;


extern map< branch_address_pair, int > dep_newly_found_bas;



int trace_index_to_branch_address_index(int trace_ind);
void finalize_solution(    map <int,int> one_solution );

void reinit_mab_fuzzers();
void prepare_fuzzers();
void fuzzer( bool vanilla_fuzzer,   
    testcase_file *one,
    int NO_ITERATIONS, 
    int NO_EXECS_BLOCK,
    float min_running_time,
    Time iteration_start_time  
 );

void init_fuzzer_copyremove( int init_no_execs, byte *init_x, int init_lenx , int itern, Time begin_time );
int  exec_block_fuzzer_copyremove( float min_running_time );
void fini_fuzzer_copyremove( );
bool can_run_fuzzer_copyremove( );


void init_fuzzer_bytes_mutation(  int init_no_execs, byte *init_x, int init_lenx, bool real_dum_fuz, string roriginal_input, uint64_t otrace_hash, Time begin_time  );
int  exec_block_bytes_mutation( float min_running_time );            
void fini_fuzzer_bytes_mutation();         
bool can_run_bytes_mutation( );


void init_fuzzer_mingle(   int init_no_execs, byte *init_x, int init_lenx, bool real_dum_fuz, Time begin_time  );
int  exec_block_fuzzer_mingle( float min_running_time );
void fini_fuzzer_mingle();
bool can_run_fuzzer_mingle( );


void init_fuzzer_system(   int init_no_execs, byte *init_x, int init_lenx , string roriginal_input, Time begin_time  );
int  exec_block_system( float min_running_time );
void fini_fuzzer_system();
bool can_run_system( );


void init_fuzzer_branches(  int init_no_execs, byte *init_x, int init_lenx, Time begin_time  );
int  exec_block_branches( float min_running_time );             
void fini_fuzzer_branches();
bool can_run_branches( );


void init_fuzzer_combiner(   int init_no_execs, byte *init_x, int init_lenx, float max_node_score_time, Time begin_time  );
int  exec_block_combiner( float min_running_time );             
void fini_fuzzer_combiner();
bool can_run_combiner( );


void init_fuzzer_branch_ga(  int init_no_execs, byte *init_x, int init_lenx, Time begin_time  );
int  exec_branch_ga( float min_running_time );
void fini_fuzzer_branch_ga( );
bool can_run_branch_ga( );



void discount_freqs(float dgamma, int dis_cou);



#define COMP_W( freqs, w , aux, b) \
    aux = 0;    \
    for( int i=0; i<256; i++)   \
            aux += freqs[i];    \
    w.clear(); \
    for( int i=0; i<256; i++) \
        w.push_back(    aux > 0 ? (float)freqs[i] / aux : 0 );   \
    w[0] = 0;  \
    for( int i=0; i<256; i++) \
        if( freqs_chars[i] > freqs_chars_avg && w[i] > freqs_chars[i] ) w[i] = (w[i] - freqs_chars[i]) / freqs_chars[i]; \
        else w[i] = 0;  \
    aux = 0; \
    for( int i=0; i<256; i++) if( w[i] > aux ) aux = w[i]; \
    if( aux > 0 ) \
        for( int i=0; i<256; i++)   \
            w[i] = w[i] / aux * 10;            \
    b = false;\
    for( int i=0; i<256; i++){   \
        if( w[i] > 0 )  \
            w[i] = pow( 2, w[i] )  ;\
        b |= w[i] > 0; \
    } 





#endif