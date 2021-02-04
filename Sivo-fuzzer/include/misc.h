/*
Copyright (c) 2021, Ivica Nikolic <cube444@gmail.com> and Radu Mantu <radu.mantu@upb.ro>

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

#ifndef MISC_H
#define MISC_H

#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <fstream>
#include <vector>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
#include "config.h"

using namespace std;


void simple_exec( string scmd );



bool copy_binary_file( string source, string dest);
bool dir_exists( char* dir );
int file_size(const char* filename);
bool file_exists (const std::string& name);
uint64_t hash_file(string filename);
unsigned char* read_binary_file(string filename, int &bytes_read);
void write_bytes_to_binary_file(string filename, unsigned char* data, int dsize);
void write_bytes_to_binary_file_non_permanent(string filename, unsigned char *data, int dsize);
uint64_t * read_binary_file_uint64_t(string filename, int &qword_read);
string get_current_dir();
string get_only_filename_from_filepath( string filepath);
bool prepare_tmp_input(string original_input);


void init_random();
int discrete_distribution_sample( vector <float> &v );
unsigned char one_rand_byte();
uint8_t one_nonzero_uniformly_random_byte();
uint8_t one_nonzero_rand_nibble();
uint8_t adv_mutate_byte(uint8_t x);



int hamming_weight64(uint64_t x);
string inst_no_to_str(uint32_t no, int t);
uint32_t extract_32bit_val_from_double( double v);
float mmin( float a, float b);
float mmax( float a, float b);
bool jumping_jindices( int count);
void reinit_jumping_jindices();




#endif
