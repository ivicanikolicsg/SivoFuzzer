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

#ifndef TYPEZ_H
#define TYPEZ_H

#include <string>
#include <set>
#include <vector>
#include <map>
#include <chrono>

using namespace std;

typedef pair<uint32_t,uint32_t> branch_address_pair;

typedef struct tbranches{
    vector <int> dependency;
    vector <int> dependency_slow;
    vector <int> dependency_fast;
    bool used_detailed_dependency_inference;
    map<uint64_t,vector<vector<int>>> solutions;
    set <uint32_t> edges;
    //
    int found_at_iter;
    int times_fuzzed;
    int branch_type;
    int max_out_edges;
    // constraints
    map <int,int> true_deps;
    vector <uint32_t> constraints;
    bool inverted;
    bool is_solved_constraint;
    bool cannot_infer_constraints;
    bool is_independent_constraint;
    bool is_multidependent_constraint;
    // multi dependent
    vector < pair<uint64_t,uint64_t> > interval ;     
    // rand trials
    bool fill_rand_byte_values;
    set<uint8_t> rand_byte_values[2];

}branch_address_type;





typedef struct{

    set <uint32_t> set_of_outcoming_nodes;

    set <uint32_t> set_of_cases;
    set <uint32_t> set_of_solved_cases;

    uint32_t cur_num_of_cases;
    uint32_t max_num_of_cases;

    uint32_t no_testcases_present;

    uint32_t cur_num_of_out_edges;
    uint32_t max_num_of_out_edges;

    int type_of_branch;
    uint32_t cmp_instruction_code;
    uint32_t cmp_compare_predicate;

    int random_walk_score;


}node_type;



typedef struct ttestcase{
    bool valid;
    int no_input_bytes;
    string testcase_filename; 
    string testcase_hash; 
    string branch_filename;
    int number_of_branches;
    int times_sampled_dataflow;
    int times_sampled_vanilla;
    int last_iter_sampled;
    int missing_solutions;
    int missing_solutions_next;
    int my_id;
    int coverage_score;
    string coverage_edges_filename;
    string coverage_nodes_filename;
    string coverage_afl_filename;
    uint64_t tot_execs;
    double tot_time;
    uint32_t found_tc;
    uint32_t found_tc_cov;
    uint32_t timetouts;
    uint32_t timetout_tc;
    uint32_t trace_length;
    uint64_t trace_hash;
    uint32_t depth;
    uint32_t path_length;
    bool     found_crash; 
    bool     is_new_cover;

}testcase_file;

typedef struct tinterim_testcase{
    string      testcase_filename; 
    uint32_t    depth;
    uint32_t    path_length;
    bool        is_new_cover;
    int         my_id;

}interim_testcase;


typedef vector<vector<uint32_t>> VM;
typedef unsigned char byte;

typedef struct {
    branch_address_pair target_branch_address;
    int target_index;
    int index_in_suggested;
    int index_in_trace;
    int age;
    float score;
} targetNode;


typedef struct{
    byte *active;
    byte *bytes;
    uint32_t lenx;
    float fitness;
    int tot;
}node_gs;




typedef struct{

    vector < vector < float > > window;

    float updates;
    float tot_updates;
    float last_update;

    float running_num;
    float running_denom;

    float discount_updates;
    float drunning_num;
    float drunning_denom;
    float dupdates;

    float new_num;
    float new_denom;
    float new_updates;

    float gamma;
    int   *discount_times;

    bool stop_disc;

    int  allow_small;

}node_mab;


typedef struct{
    double      tot_time;
    uint32_t    tot_calls;
}avg_time;

typedef std::chrono::time_point<std::chrono::high_resolution_clock> Time;

#endif