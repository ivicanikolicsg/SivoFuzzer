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


#include "branch_inference.h"
#include "fork_servers.h"
#include "executions.h"
#include "branch_inference_slow.h"
#include "branch_inference_misc.h"


//
// 
// Inference based on mutating bytes one by one
//
//


int branch_inference_target_bytes( string original_input, bool forced_inference, 
    vector<int>& byte_positions, float time_max_slow_inference,
    set<int> &set_bytes_passed, set<int> &set_bytes_significant)
{

    START_TIMER(begin_time);

    if( 0 == byte_positions.size() ) return 0;


    set <branch_address_pair> target_branches;
    for( int i=0; i< branch_addresses.size(); i++){
        branch_address_pair ba = get_ba( branch_addresses[i][0],branch_addresses[i][1] );
        if( branch_exist(ba) && branch_good_for_iteration(ba) )
            target_branches.insert( ba );
    }

    if( PRINT_DEPENDENCY_INFERENCE_INFO )
        printf("#target branches: %ld\n", target_branches.size() );


    // read the original input
    if( !prepare_tmp_input( original_input )) return 0;
    int no_input_bytes;
    unsigned char* X = read_binary_file(TMP_INPUT, no_input_bytes);
    if ( NULL == X){
        printf( KERR "Cannot read the original input file:%s\n" KNRM, original_input.c_str());
        return 0 ;
    }
    if( 0 == no_input_bytes )return 0;
    unsigned char* Y = (unsigned char *)malloc(no_input_bytes);

    

    // recreate the vector of active positions, with
    // 1) add -1 at beginning (to denote the original file)
    // 2) concatenate the target bytes 
    vector <int> active_byte_positions;
    active_byte_positions.push_back( -1 );

    // randomize positions
    // instead of using built-in random permutation which is too slow for larger vectors
    // use faster solution, based on modulo multiplication by prime, 
    // i.e. (P*x + B) % N, where P is prime
    uint32_t larger_prime = larger_primes[ mtr.randInt() % NO_LARGER_PRIMES ]; 
    uint32_t offset = mtr.randInt() % byte_positions.size();
    uint32_t findex = 0;
    for( int i=0; i<byte_positions.size(); i++){
            active_byte_positions.push_back( byte_positions[ (offset + larger_prime * findex) % byte_positions.size() ] );
            findex ++;
    }


    map <branch_address_pair, pair<set<uint32_t>,set<uint32_t>> > z ;
    map <branch_address_pair,pair<set<uint32_t>,set<uint32_t>> > deps ;

    int finished_checked = -1;
    for(int kk=0; kk<active_byte_positions.size(); kk++)
    {

        if( PRINT_DEPENDENCY_INFERENCE_INFO )
            printf("%d ",active_byte_positions[kk]); fflush(stdout);


        if( time_max_slow_inference > 0 && 
            TIME_DIFFERENCE_SECONDS(begin_time) > time_max_slow_inference ){
            printf(KINFO "Timeout on branch inference \n" KNRM );
            break;
        }

        finished_checked ++;


        int bpos = active_byte_positions[kk];

        if( bpos >= 0 ) set_bytes_passed.insert( bpos ) ;

        memcpy(Y , X , no_input_bytes);
        if( bpos >= 0 ){
            if( bpos >= no_input_bytes ) break;
            Y[bpos] ^= differences[0][bpos] ;
        }

        write_bytes_to_binary_file(TMP_INPUT, Y, no_input_bytes);

        if( forced_inference )
            run_one_fork( RUN_FORCER );
        else
            run_one_fork( RUN_GETTER );

        if( forced_inference && store_branches_tot > __new_branch_index[0])
            if( PRINT_DEPENDENCY_INFERENCE_INFO )
                printf( KERR "[missed]" KNRM );


        int ii = 0;
        map <int,int> counters;
        while (forced_inference  && ii < store_branches_tot && branches_trace[ii] == store_branches_trace[ii]
        ||     !forced_inference && ii < __new_branch_index[0]) {
            auto zz = counters.find( branches_trace[ii] );
            if( zz == counters.end() ) counters.insert( { branches_trace[ii], 0} );
            else  zz -> second ++;
            zz = counters.find( branches_trace[ii] ) ;   

            branch_address_pair ba = get_ba( zz->first , zz->second ) ;
            if( target_branches.find(ba) == target_branches.end() ){
                ii++;
                continue;
            }


            // check if it exist in the previous map
            if(     forced_inference && BRANCH_IS_UNIMPORTANT( store_branches_opera[ii] )
            ){
                ii++;
                continue;
            }

            uint32_t val =  compute_val(ii);        
            int taken = branches_taken[ii];

            if ( z.find(ba) == z.end() ){
                z.insert( {ba,make_pair(set<uint32_t> (),set<uint32_t> ()) });
            } 
            auto it = z.find( ba );

            // branch value
            if( it->second.first.find(taken) == it->second.first.end() ){
                if( 0 == kk )
                    it->second.first.insert( taken );

                if( it->second.first.size() && kk > 0 ){
                    if ( deps.find( ba ) == deps.end()  )
                        deps.insert( {ba,make_pair(set<uint32_t>(),set<uint32_t>()) });
                    auto i2 = deps.find( ba );
                    i2->second.first.insert( bpos  );
                }           
            }

            // icmp operators
            if( it->second.second.find(val) == it->second.second.end() ){
                if( 0 == kk )
                    it->second.second.insert( val );

                if( it->second.second.size() && kk > 0 ){
                    if ( deps.find( ba ) == deps.end()  )
                        deps.insert( {ba,make_pair(set<uint32_t>(),set<uint32_t>()) });
                    auto i2 = deps.find( ba );
                    i2->second.second.insert( bpos  );
                }           
            }

            ii++; 
        }
    }
    if( PRINT_DEPENDENCY_INFERENCE_INFO )
        printf("\n");

    free( X );
    free( Y );

    float max_perct_filled = 0.75;

    if(PRINT_DEBUG_INFERRED_BYTES)
        printf("Found slow dependencies:\n");

    for(auto it=deps.begin(); it != deps.end(); it++){

        branch_address_pair  ba = it->first;

        if(PRINT_DEBUG_INFERRED_BYTES)
            printf("%08x %4d :  %2ld %2ld : ", it->first.first, it->first.second, it->second.first.size(), it->second.second.size() );

        set<uint32_t> final_deps;

        // if not forced execution then both branch taken and values must be important
        if( !forced_inference ){
            set_union(it->second.first.begin(),it->second.first.end(),
                             it->second.second.begin(),it->second.second.end(),
                            inserter(final_deps,final_deps.begin()));
        }
        // if forced, then take the values set only if it is size is not too large
        if( forced_inference){
            if( byte_positions.size() > 10 && 
                it->second.second.size()  < byte_positions.size() * max_perct_filled 
             )
                set_union(it->second.first.begin(),it->second.first.end(),
                             it->second.second.begin(),it->second.second.end(),
                            inserter(final_deps,final_deps.begin()));
            else{
                // randomly choose one or neither and stick to it
                switch( mtr.randInt() % 3){
                    case 0: final_deps = it->second.first; break;
                    case 1: final_deps = it->second.second; break;
                    default: break;
                }
            }
        }

        vector <int> d;
        for( auto qq=final_deps.begin(); qq != final_deps.end(); qq++){ 
            d.push_back( *qq );
            set_bytes_significant.insert( *qq );
        }
        branch_add_dependency( it->first, d, true );
        if( find(branches_with_dependency.begin(), branches_with_dependency.end(), ba ) == branches_with_dependency.end() ) {
            branches_with_dependency.push_back(it->first);
        }

        if(PRINT_DEBUG_INFERRED_BYTES){
            for( auto qq=final_deps.begin(); qq != final_deps.end(); qq++)
                    printf("%d ", *qq);
            printf("\n");
        }
    
    }

    return set_bytes_passed.size();

}


int branch_inference_try_all_bytes( string original_input,  int original_input_size, 
                float time_max_slow_inference,
                set <int> &set_passed, set<int> &set_important
                )
{
    vector <int> byte_positions;
    for( int i=0; i<original_input_size; i++)
        byte_positions.push_back( i );


    return  branch_inference_target_bytes( original_input, 0, byte_positions, time_max_slow_inference, 
                    set_passed, set_important );
}


int  infer_depedency_of_randomly_chosen_chunk_of_bytes( string original_input, int original_input_size, bool forced_inference, 
    int CHUNK_SIZE, float time_max_slow_inference)
{

    int no_chunks = original_input_size / CHUNK_SIZE;
    if( mtr.randInt() % 2) no_chunks = min(no_chunks, 4);
    int chosen_chunk = (no_chunks> 0 ) ? (mtr.randInt() % no_chunks) : 0 ;

    if( PRINT_DEPENDENCY_INFERENCE_INFO )
        printf( KINFO "Chosen chunk: %d out of %d    \n" KNRM , chosen_chunk, no_chunks );

    vector <int> byte_positions;
    for( int jj = 0; jj<CHUNK_SIZE; jj++){
        if( chosen_chunk * CHUNK_SIZE + jj >= original_input_size ) break;
        byte_positions.push_back( chosen_chunk * CHUNK_SIZE + jj  );
    }

    if( byte_positions.size() > 0){
        set <int > s0, s1;
        return 
            branch_inference_target_bytes(original_input,  forced_inference, byte_positions, time_max_slow_inference, s0, s1);
    }


    return 0;

}


