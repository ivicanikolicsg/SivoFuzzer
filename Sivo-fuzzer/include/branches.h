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

#ifndef BRANCHES_H
#define BRANCHES_H

#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "misc.h"


branch_address_type branch_insert_empty_branch(branch_address_pair b);
void branch_clear_all_branches();
unsigned long branch_current_size();
branch_address_pair get_ba(  uint32_t ad_bas, uint32_t ad_cou  );
void import_branches(VM &all_branch_addresses, int iteration_no);
void print_all_branches();
bool branch_exist(branch_address_pair bp);
bool branch_is_solved(branch_address_pair bp);
void branch_replace_dependency_fast(branch_address_pair bp, vector<int> found_vars);
void branch_add_dependency(branch_address_pair bp, vector<int> found_vars, bool slow);
vector<int> get_dependency( branch_address_pair bp);
set<uint32_t> branch_get_edges(  branch_address_pair bp);
void branch_insert_solution( branch_address_pair bp, vector<int> varz, vector<int> sols);
vector<int> get_previous_solution( branch_address_pair bp, vector<int> varz );
void branch_add_edge( branch_address_pair bp, uint32_t next_branch_address);
void print_branches_with_dependency( vector <branch_address_pair> branches_with_dependency  );
int branch_get_solving_candidates( VM branch_addresses );
int branch_get_number_of_not_solved(bool with_failed);
void branch_add_constraints(branch_address_pair bp, map <int,int> true_deps, vector <uint32_t> constraints, bool inverted);
bool branch_has_constraints(branch_address_pair bp );
map <int,int> branch_get_true_dep( branch_address_pair bp);
vector <uint32_t> branch_get_constraints( branch_address_pair bp);
bool branch_get_inverted( branch_address_pair bp );
bool branch_get_cannot_infer_constraints( branch_address_pair bp);
bool branch_is_one_byte_constraint( branch_address_pair bp );
bool branch_is_multi_byte_constraint( branch_address_pair bp );
bool branch_is_solved_constraint( branch_address_pair bp );
void branch_set_solved_constraint( branch_address_pair bp, bool set_solved );
void branch_insert_solved_constraints( branch_address_pair bp , set<uint8_t> byte_values[2] );
void branch_print_solved_constraint( branch_address_pair bp  );
set<uint8_t> branch_get_solved_constraints( branch_address_pair bp , int value );
set<uint8_t> branch_get_rand_byte_values( branch_address_pair bp , int truth_value );
void branch_set_rand_byte_values( branch_address_pair bp , int truth_value, set<uint8_t> byte_values  );
vector <int> branch_get_correct_dependency( branch_address_pair bp );
void branch_set_constraint_interval( branch_address_pair bp, vector < pair<uint64_t,uint64_t> > interval ) ;
vector < pair<uint64_t,uint64_t> > branch_get_constraint_interval( branch_address_pair bp );
bool branch_is_constraint_independent( branch_address_pair bp );
bool branch_is_constraint_multibyte_dependent( branch_address_pair bp );
void branch_print_info( branch_address_pair ba , int code );
bool branch_get_used_detailed_dependency_inference( branch_address_pair ba  );
void branch_set_used_detailed_dependency_inference( branch_address_pair ba, bool val  );
void branch_random_sieve();
bool branch_good_for_iteration(  branch_address_pair ba );
void branch_remove_all_dependencies();
int branch_get_found_at_iteration( branch_address_pair bp ) ;
int branch_get_total_number();
int branch_get_times_fuzzed(  branch_address_pair ba   );
void branch_increase_times_fuzzed( branch_address_pair ba  );
int branch_get_branch_type(  branch_address_pair ba   );
int branch_get_max_out_edges(  branch_address_pair ba   );


#endif