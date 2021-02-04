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

#include <set>
#include <fstream>
#include "branches.h"



void count_true_coverage( uint32_t cttc[2] );
string create_id(string temp_input, bool external_tc, int sol_type, string add_info );
void solution_add_found_types( int CURRENT_SOLUTION_TYPE  );
void check_and_add_solution( string tmp_input_file, int CURRENT_SOLUTION_TYPE, string add_info);
void create_lookup_for_sol_id_to_string();
string get_solution_name( int stype  );
void process_crash( int status );
uint32_t count_current_important_tc();
void store_timeout_testcase();
void process_timeout(   int type_of_run,
                        uint64_t &res_runs, uint64_t &res_timo, 
                        int QTS, uint8_t queue_timeouts[], 
                        uint32_t &times_used_temp_timeout, uint32_t &times_successfull_used_temp_timeout  );




