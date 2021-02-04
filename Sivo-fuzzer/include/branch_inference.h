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


#include <string.h>
#include <math.h>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <random>
#include "misc.h"
#include "branches.h"



#define NO_LARGER_PRIMES 10
extern uint32_t larger_primes[NO_LARGER_PRIMES];



void branch_inference_complete( 
    string original_input, int iter_no, bool forced_inference, vector<int> unmutable );

void  branch_inference_target_bytes( 
    string original_input, bool forced_inference, vector<int>& byte_positions);

void  branch_inference_fast(string original_input,  int iter_no, bool forced_inference, 
    vector<int> unmutable, float max_time  );

void  value_constraints_inference(
        vector <branch_address_pair> &targets1, 
        vector <branch_address_pair> &targets2, 
        string original_input);


