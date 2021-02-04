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

#ifndef TESTCASES_H
#define TESTCASES_H


#include <string>
#include "misc.h"

using namespace std;


bool add_testcase(string filename);
int get_no_testcases();
testcase_file *full_get_testcase(int type);
void update_testcases_candidates();
int testcase_get_passed_dataflow(bool once);
int testcase_get_passed_vanilla(bool once);
int testcase_get_passed_any(bool once);
int testcase_times_passed_dataflow(bool is_it_min);
int testcase_times_passed_vanilla(bool is_it_min);
void testcase_get_testcases_( vector < pair < string, vector<float> > > &tcc );
uint32_t testcases_get_num_of_stats();
string testcase_get_filename(string testcase);
void    init_testcase_stuff();
void    testcases_update_sampling_method( float new_findings, float new_findings_cov, double time_spent);
void    testcases_update_testcase_stats( string tf, uint64_t forkexecs, double ttim, uint32_t found_tc, uint32_t found_tc_cov, uint32_t timeouts, uint32_t tc_on_timeouts   );
string  testcase_get_sampling_name( int meth);
void remove_redundant_testcases(    map < string , testcase_file * > &min_testcases, 
                                    bool &has_changed, 
                                    map < int, pair< testcase_file * , float >  > &shtc );
void testcases_choose_class();
void setup_reinit( bool );
void redo_with_different_index();



#endif