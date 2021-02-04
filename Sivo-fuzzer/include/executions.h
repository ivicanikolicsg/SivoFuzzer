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

#ifndef EXECUTIONS_H
#define EXECUTIONS_H

#include <fstream>
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
#include <string.h>
#include <stdint.h>
#include <fstream>
#include "config.h"
#include "misc.h"
#include "typez.h"


int get_coverage_entries();
int get_coverage_entries_all();
void get_coverage_entries_separate(int s[8]);
float get_coverage_byte_perct();
void create_lookup_for_compressed_bit();
void init_coverage();
void fini_coverage();
void store_branch_trace();
void load_from_stored_branch_trace();
bool execution_edge_coverage_update();
void execution_increase_edge_counters();
int execution_get_divergent_index( );
void execution_all_coverages_produce_and_save_to_file( string file_edgecoverage, string file_nodecoverage, string file_afl, uint32_t &no_ifs_switches, uint32_t &trace_length );
void execution_afl_coverages_produce_and_save_to_file(string file_afl);
void get_edge_coverage_from_file( string filename, vector < vector<int>> &coverage );
void get_node_coverage_from_file( string filename, vector < vector<int>> &coverage );
vector <vector<uint32_t>>  execution_get_branches( );
void update_node_count();
uint32_t node_to_testcases_size();
void fill_node_coverage_counts_with_inserts( string filename );
void fill_node_to_nodes( string filename );
bool execution_consistent_minimized_edge_coverage( uint8_t initial_showmap[1<<16], bool only_update );
void execution_get_minimized_edge_coverage( uint8_t initial_showmap[1<<16] );
void populate_node_to_max_nodes(VM &all_branch_addresses);
uint32_t compute_val(int ii);
bool execution_showmap_timeout_new();


#endif