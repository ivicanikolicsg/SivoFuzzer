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

#include "fuzzer.h"


int MAX_MINGLE_NUMBER = 5;

static byte *x;
static int lenx;
static int no_execs;
static Time mbegin_time;
static bool vanilla_fuzzer;
static byte *y;

vector < node_mab  > mab_mingler_number;
vector < node_mab  > mab_mingler_targets;
vector < bool > uses;

map <int,int> one_solution;
set <int> already_chosen;


void init_fuzzer_mingle( int init_no_execs, byte *init_x, int init_lenx, bool real_dum_fuz, Time begin_time   )
{
    x           = init_x;
    lenx        = init_lenx;
    no_execs    = init_no_execs;
    vanilla_fuzzer = real_dum_fuz;
    mbegin_time = begin_time;

    y = (byte *)malloc(lenx + 1);

    mab_recompute( mab_mingler_number );
    mab_recompute( mab_mingler_targets );


    while( mab_mingler_targets.size() < all_found_solutions.size() ){        
        mab_mingler_targets.push_back( mab_init_best_value(mab_mingler_targets) );
    }
    uses.clear();
    for( int i=0; i<mab_mingler_targets.size(); i++)
        uses.push_back(true);


    uint32_t vs=0;
    for( auto i=0; i< all_found_solutions.size(); i++)
        vs += all_found_solutions[i].size();
}


bool can_run_fuzzer_mingle()
{
    return tc_found_solutions.size() > 0 || all_found_solutions.size() > 0;
}



int exec_block_fuzzer_mingle( float min_running_time  )
{
    int found_new = 0;
    vector <bool> multiplier;

    fuzzer_current_main =  vanilla_fuzzer ? SOL_TYPE_MINGLER_DUMB : SOL_TYPE_MINGLER_SMART;

    if( tc_found_solutions.size() == 0 && all_found_solutions.size() == 0){ 
        printf( KINFO "No solutions to mingle\n" KNRM);
        return found_new;
    }


    for( int e=0; e<no_execs; e++){

        if( TIME_DIFFERENCE_SECONDS(mbegin_time) >= min_running_time ) break; 


        memcpy( y, x, lenx);
        one_solution.clear();
        already_chosen.clear();
                
        START_TIMER( begin_time_mingle );

        // choose number of mingled
        int mingle_number = mab_sample( mab_mingler_number, MAX_MINGLE_NUMBER, false, multiplier);
        int how_many_to_choose = 1 + mingle_number;

        // chose type of mingled (current testcase = 0 vs all= 1)
        // type:0 - mingle only the testcases found from the current testcases
        // type:1 - mingle all 
        int type_local_global = mtr.randInt() % 3;
        if( 0 == type_local_global && tc_found_solutions.size() == 0 )
            type_local_global = 1;
        
        if(PRINT_DEBUG_FUZZER_MINGLE){
            printf("\t Mingler : type %d  size  %4d  %ld :", !!type_local_global, how_many_to_choose, mab_mingler_targets.size() ); fflush(stdout);
        }
        
        // offset can shift the previous positions
        int offset = 0;
        if( type_local_global && ! (mtr.randInt() % 5) ) offset = -lenx/2 +  lenx % mtr.randInt(); 

        set <int > sampled;
        for( int j=0; j<how_many_to_choose; j++){
            
            int index;
            if( 0 == type_local_global )
                index = mtr.randInt() % tc_found_solutions.size() ;
            else{
                index = mab_sample( mab_mingler_targets, mab_mingler_targets.size(), false, uses );
                if( index < 0 ){
                    uses.clear();
                    for( int i=0; i<mab_mingler_targets.size(); i++)
                        uses.push_back(true);
                    index = mab_sample( mab_mingler_targets, mab_mingler_targets.size(), false, uses );
                    if( index < 0 ) break;
                }
                sampled.insert( index );
                uses[index] = false;
            }
            
            if(PRINT_DEBUG_FUZZER_MINGLE){
                printf("%4d ", index); fflush(stdout);
            }


            for( auto it =  (type_local_global ? all_found_solutions[index].begin() : tc_found_solutions[index].begin() ); 
                      it != (type_local_global ? all_found_solutions[index].end()   : tc_found_solutions[index].end()); 
                      it++)
            {
                            if( it->first + offset >= 0 && 
                                it->first + offset < lenx && 
                                one_solution.find( it->first) == one_solution.end() )
                            {
                                    y[ it->first + offset ] = it->second;
                                    one_solution.insert( {it->first+ offset, it->second } );
                            }
            }
        }
        if(PRINT_DEBUG_FUZZER_MINGLE)
            printf("\n");
 
        write_bytes_to_binary_file(TMP_INPUT, y, lenx);

        int good_exec = run_one_fork( RUN_TRACER );
        int found_one_new = 0;
        if( good_exec && execution_edge_coverage_update() ){

            check_and_add_solution( TMP_INPUT, SOL_TYPE_MINGLER , "" );
            finalize_solution( one_solution );
        
            found_new ++;
            found_new_cov += 2==glob_found_different;
            found_one_new = 1;
        }

                
        auto timf = TIME_DIFFERENCE_SECONDS( begin_time_mingle );
        mab_update( mab_mingler_number, mingle_number, found_one_new, timf );

        if( type_local_global && sampled.size() > 0 ){
            for( auto it= sampled.begin(); it != sampled.end(); it++)
                mab_update( mab_mingler_targets, *it, found_one_new, 1 );
        }

    }


    return found_new;

}


void fini_fuzzer_mingle()
{
    free( y );

}
