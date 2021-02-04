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

#include <fstream>
#include <sstream> 
#include <algorithm>
#include <random>
#include <string.h>
#include <stdio.h>       
#include <stdint.h>         
#include <fcntl.h>          
#include <unistd.h>         
#include <time.h>
#include <chrono>

#include "misc.h"
#include "branches.h"
#include "testcases.h"



void constraint_system( string original_input, VM branch_addresses, vector <branch_address_pair> probs );
void solve_one_constraint( branch_address_pair ba );
bool sample_multibyte( branch_address_pair ba, bool truth_value,  map < uint32_t, set<uint8_t> > &bytes_vals );




bool sample_solution(   vector < pair<branch_address_pair,int> > path, byte *x, int lenx, 
        int &solved_constraints, set <int> touch_only_vars, 
        bool &used_random, bool &used_rand_interval, bool &has_random_bytes, 
        set <branch_address_pair> &branches_chosen_randomely,  
        branch_address_pair last_failed,
        map <int,int> &one_solution
 );

 void add_wrong_byte_guess( branch_address_pair ba, int truth_value, byte *x, int lenx   );
