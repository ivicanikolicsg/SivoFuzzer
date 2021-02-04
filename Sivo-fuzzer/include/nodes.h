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

#ifndef NODES_H
#define NODES_H


#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "misc.h"
#include "nodes.h"





node_type nodes_init_empty_node();
void nodes_add_new_node( uint32_t node );
bool nodes_exist(uint32_t node);
bool nodes_is_solved(uint32_t node);
void nodes_add_number_of_edges(uint32_t node, int new_cur_num_edges, int new_max_num_edges);
void nodes_add_outcoming_node(uint32_t node, uint32_t outcome_node);
set<uint32_t> nodes_get_outcome_nodes(  uint32_t node );
uint32_t nodes_get_current_size();
uint32_t nodes_get_number_of_important_unsolved( int branch_type );
uint32_t nodes_get_number_of_important( int branch_type );
uint32_t nodes_get_number_of_present_in_testcases();
uint32_t nodes_get_number_all_outcoming_nodes();
uint32_t nodes_get_num_cur_or_max(int branch_type , int cur_or_max);
int nodes_get_number_outcome_nodes(  uint32_t node );
int nodes_get_max_out_edges( uint32_t node );
void nodes_add_type(uint32_t node, int type_of_branch);
void nodes_increment_testcase_count(uint32_t node);
void nodes_decrement_testcases_count(uint32_t node );
void nodes_clear_all_testcase_count();
void fill_nodes_from_edgecoverage_file( string filename );
int nodes_get_testcase_count( uint32_t node );
void nodes_fill_ifs( string bp );
void nodes_fill_switch_cases( string binary_path );
void nodes_add_switch_case(uint32_t node, uint32_t sw_case);
void nodes_add_solved_value( uint32_t node, uint32_t sw_value );
int nodes_get_number_solved(  uint32_t node );
int nodes_get_number_max(  uint32_t node );
uint32_t nodes_get_number_all_solved();
int nodes_get_type_of_branch( uint32_t node );
uint32_t nodes_choose_random_unsolved_switch( uint32_t node );
int  nodes_not_solved( uint32_t node, uint32_t val);
void nodes_fill_cfg( string bp );
int random_walk_on_cfg( uint32_t start_node, uint32_t steps);
void do_random_walk_on_all_nodes( int steps);
int nodes_get_random_walk_score(uint32_t node);



#endif