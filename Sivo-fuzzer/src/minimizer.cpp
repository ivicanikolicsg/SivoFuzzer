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

#include "minimizer.h"
#include "testcases.h"
#include "misc.h"
#include "fork_servers.h"
#include "executions.h"
#include <string>

using namespace std;

// 
// Minimize seeds, i.e. try to reduce their length.
// With logarithmic search we try to identify where the break point is 
// i.e. the new seed no longer provides the same coverage.
//

void minimize_testcase(string filepath)
{

    if( tot_sol_add_time/ TIME_DIFFERENCE_SECONDS(start_fuzzing_time) > MAX_FRAC_SOL_TO_TIME_TO_MINIMIZE) 
        return;
    
    int fsize = file_size(filepath.c_str());
    uint8_t initial_minimized_showmap[1<<16];

    if( fsize == 0) return;


    uint32_t * st_branches_trace = (uint32_t *)malloc( MAX_BRANCHES_INSTRUMENTED * sizeof(uint32_t) )  ;
    uint32_t * st_branches_taken = (uint32_t *)malloc( MAX_BRANCHES_INSTRUMENTED * sizeof(uint32_t) )  ;
    uint32_t st_branches_tot = 0;
        
    execution_get_minimized_edge_coverage( initial_minimized_showmap );
    execution_consistent_minimized_edge_coverage( initial_minimized_showmap, true );


    byte *x = read_binary_file( filepath , fsize );
    byte *y = (byte *) malloc( fsize );
    write_bytes_to_binary_file(TMP_INPUT, x, fsize);
    
    int good_exec = run_one_fork( RUN_GETTER );
    while( st_branches_tot < __new_branch_index[0] ){
        st_branches_trace[st_branches_tot] = branches_trace[st_branches_tot];
        st_branches_taken[st_branches_tot] = branches_taken[st_branches_tot];
        st_branches_tot++;
    }



    uint32_t l = 0;
    uint32_t r = fsize;
    while( l+1 < r)
    {
        int m = l + (r-l)/2 ; 
        memcpy( y, x, m);
        write_bytes_to_binary_file(TMP_INPUT, y, m );
        int good_exec = run_one_fork( RUN_GETTER );
        bool same = true;

        // if execution was bad or the traces do not have the same length, then definitely different
        if( !good_exec || __new_branch_index[0] != st_branches_tot ) same = false;
        else{
            for( int i=0; i<st_branches_tot; i++)
                // if types of trace operations/branch values are inconsistent, then definitely different
                if( st_branches_trace[i] != branches_trace[i] || 
                    st_branches_taken[i] != branches_taken[i] ){
                        same = false;
                        break;
                    }

            // if they produce different coverage, then they are different
            if( same ){
                run_one_fork( RUN_TRACER ); 
                if( ! execution_consistent_minimized_edge_coverage( initial_minimized_showmap, false ) )
                    same = false;
            }
        }

        if( same ) r = m;
        else l = m;

    }

    if( fsize  >= MIN_SIZE_TO_MINIMIZE && 
        (float)r / fsize < MIN_RATIO_TO_MINIMIZE 
    )
    {
        printf( KINFO "Minimized testcase size from/to : %5d -> %5d \n" KNRM , fsize, r );
        write_bytes_to_binary_file_non_permanent(filepath, y, r);
    }
    
    free( st_branches_trace );
    free( st_branches_taken );
    free( y );
    free( x );


}
