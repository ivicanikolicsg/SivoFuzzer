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


static byte *x_ri;
static int lenx;
static int no_execs;
static byte *y;
static Time mbegin_time;

static vector<float> cand_probs;

void init_fuzzer_branches(  int init_no_execs, byte *init_x, int init_lenx, Time begin_time  )
{
    x_ri        = init_x;
    lenx        = init_lenx;
    no_execs    = init_no_execs;
    mbegin_time = begin_time;

    y = (byte *)malloc(lenx + 1);

    cand_probs.clear();
    for( int i=0; i< branch_candidates.size(); i++)
        cand_probs.push_back( branch_candidates[i].score   );

}

void fini_fuzzer_branches()
{
    free( y );
}


bool can_run_branches()
{
    return branch_candidates.size() > 0 ;
}


int exec_block_branches( float min_running_time )
{
    int found_new = 0;
    map <int,int> one_solution;

    // choose one
    if( branch_candidates.size() == 0){ 
        printf(KINFO "No candidates for block fuzzer\n" KNRM );   
        return 0;
    }

    fuzzer_current_main = SOL_TYPE_BRANCH_PRIMARY;


    for(int current_trials = 0; current_trials < no_execs; current_trials++){

        if( TIME_DIFFERENCE_SECONDS(mbegin_time) >= min_running_time ) break; 

        int cand_index;
        if( mtr.randInt() % 3)
            cand_index = discrete_distribution_sample( cand_probs );
        else
            cand_index = mtr.randInt() % cand_probs.size();

        cand_probs[cand_index] /= 2.0;

        branch_increase_times_fuzzed( branch_candidates[cand_index].target_branch_address );


        auto one_candidate = branch_candidates[ cand_index ];
        branch_address_pair target_branch_address   = one_candidate.target_branch_address;
        int target_index                            = one_candidate.target_index;
        int index_in_suggested                      = one_candidate.index_in_suggested;

        // find frequencies 
        auto tmp_deps = branch_get_correct_dependency( target_branch_address );
        if( tmp_deps.size() == 0){
            if(PRINT_DEBUG_FUZZER_BRANCH)
                printf("Empty dep set\n");
            continue;
        }


        vector <float> ofreqs;
        vector <int> obyte_vals;
        bool found_non_zero_freq = false;
        for( int i=0; i<tmp_deps.size(); i++){
            int byte_val = tmp_deps[i];
            obyte_vals.push_back( byte_val );
            auto it = byte_to_branches.find( byte_val );
            if( it == byte_to_branches.end() ) 
                ofreqs.push_back( 0 );
            else
                ofreqs.push_back( float(it->second) );
            if( it->second > 0) found_non_zero_freq = true;
        }
        if( !found_non_zero_freq){
            if(PRINT_DEBUG_FUZZER_BRANCH)   
                printf("Cannot find non-zero byte_to_branch\n");
            return 0;
        }



        memcpy( y, x_ri, lenx );
        one_solution.clear();

        auto freqs = ofreqs;
        auto byte_vals = obyte_vals;

        // number of chosen bytes according to trunc Zipf distribution (up to 4)
        vector <float> ncv;
        for( int i=0; i< min(4, int(tmp_deps.size()) ); i++ )
            ncv.push_back( 1.0 / pow(1 + i, 2 ));
        int number_of_chosen_bytes = 1 + discrete_distribution_sample( ncv );

        bool choose_uniformly_at_random = mtr.randInt() % 2;
        for( int k=0; k<number_of_chosen_bytes; k++){
            if( freqs.size() == 0) break;
            int chosen_byte_ind, chosen_byte_val;
            int tries = 100;
            do{
                if( choose_uniformly_at_random )
                    chosen_byte_ind = mtr.randInt() % freqs.size() ;
                else
                    chosen_byte_ind = discrete_distribution_sample( freqs );
                chosen_byte_val = byte_vals[ chosen_byte_ind ];
                tries --;
            }while ( chosen_byte_val >= lenx && tries > 0 );

            if( tries <= 0 ) break;

            freqs.erase( freqs.begin() + chosen_byte_ind );
            byte_vals.erase( byte_vals.begin() + chosen_byte_ind );

            if( chosen_byte_val >= lenx ) continue;

            y[chosen_byte_val] = adv_mutate_byte( x_ri[chosen_byte_val] );
            one_solution.insert( {chosen_byte_val, y[chosen_byte_val]});

        }

        if( PRINT_DEBUG_FUZZER_BRANCH ){
            printf("\t Fuzz branch %08x %4d  : #dep  %2ld  :  bytes ",  
                target_branch_address.first, target_branch_address.second, tmp_deps.size() );
            for( auto it=one_solution.begin(); it!= one_solution.end(); it++)
                printf("%d ", it->first);
            printf("\n");
        }


        write_bytes_to_binary_file( TMP_INPUT, y, lenx);
        int good_exec = run_one_fork( RUN_TRACER );

        bool found_one_for_switch = false;
        if( good_exec && execution_edge_coverage_update() ){

            check_and_add_solution( TMP_INPUT, SOL_TYPE_BRANCH_PRIMARY , "" );
            finalize_solution( one_solution );

            found_new ++;
            found_new_cov += 2==glob_found_different;
            found_one_for_switch = true;
        }

        // if switch and fast execution, immeidately fuzz the bytes
        if( found_one_for_switch &&         
            branch_get_branch_type(target_branch_address) == 2 && 
            tmp_deps.size() > 0
            ){

                memcpy(y,x_ri,lenx);

                START_TIMER(tm)

                for( int byte_val = 0; byte_val < 256; byte_val++)
                {
                    if( TIME_DIFFERENCE_SECONDS(tm) > TIMEOUT_SWITCH_FUZZ ) break;

                    // if single byte, try all possible values (unless timeout)
                    if( 1 == tmp_deps.size() )
                        y[ byte_vals[0] ] = byte_val;
                    // otherwise mutate all dependent bytes
                    else{
                        memcpy(y,x_ri,lenx);
                        for( int j=0; j<tmp_deps.size(); j++)
                            if( tmp_deps[j] < lenx )
                                y[ tmp_deps[j] ] = one_rand_byte();
                    }

                
                    write_bytes_to_binary_file(TMP_INPUT, y, lenx);
                    int good_exec = run_one_fork( RUN_TRACER );
                    if( good_exec && execution_edge_coverage_update() ){
                        check_and_add_solution( TMP_INPUT, SOL_TYPE_BRANCH_SECOND , "" );
                        finalize_solution( one_solution );

                        found_new ++;
                        found_new_cov += 2==glob_found_different;
                    }
                }
            }
    }


    return found_new;
}


