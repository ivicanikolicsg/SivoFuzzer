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

#ifndef MULTIARMBAND_H
#define MULTIARMBAND_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <vector>
#include <set>
#include <math.h>
#include "config.h"

using namespace std;



int mab_sample( vector < node_mab > &x, int number, bool print_me, vector <bool> &use );
void mab_update( vector < node_mab > &x, int index, float add_cover, float timtot );
void mab_recompute( vector < node_mab > &x );
node_mab mab_init();
node_mab mab_init( float different_gamma );
node_mab mab_init_value( float a, float b);
node_mab mab_init_best_value(   vector < node_mab > &x   );
node_mab mab_init_allow_small( int all_sm );
void mab_discount_add( vector < node_mab > &x, int *discount_times_addr );
void mab_discount_all_mabs();
int mab_get_best_index(  vector < node_mab > &x  );
node_mab mab_init_best_value(   vector < node_mab > &x   );


#endif