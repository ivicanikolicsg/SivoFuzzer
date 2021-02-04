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
#include "branch_inference_misc.h"


//
//
// Inference based on probabilistic group testing
//
//

uint32_t larger_primes[NO_LARGER_PRIMES] = {10067, 22811, 30839, 41357, 50423, 71453, 80669, 91387, 102701, 104723};


// Bit vectors according to the group testing approach,
// i.e. combination of 0s followed by 1s of different length 
char** generate_flips(int ex2, int &no_vectors)
{
    no_vectors = 2 + 2*log(ex2)/log(2);
    char **F;

    F = (char **)malloc(sizeof(char *)* no_vectors );
    F[0] = (char *)malloc( ex2 );
    memset( F[0], 0, ex2);
    F[1] = (char *)malloc( ex2 );
    memset( F[1], 1, ex2);

    int vpos = 2;
    int sep = int(ex2/2);
    while (sep > 0){
        for (int j=0; j<2; j++){

            F[vpos] = (char *)malloc(ex2);
            memset(F[vpos], 0, ex2);

            int pos = 0;
            int sw = j;
            while( pos < ex2 ){

                int t = sep;
                while (t > 0){
                    if (pos >= ex2) break;
                    if (sw) F[vpos][pos] = 1;
                    pos += 1;
                    t -=1;
                }
                sw = 1 - sw;
            }
            vpos ++;
        }
        sep = int(sep/ 2);
    }

    return F;
}

// use this as a random permute (because it is faster)
uint32_t permute( int byte_pos, uint32_t larger_prime, int lenx )
{
    return (byte_pos * larger_prime ) % lenx;
}


// probabilistic group testing for inference
void one_pass_inference_complete(bool forced_inference,
    set <branch_address_pair> &target_branches,
    int no_input_bytes, byte *X, byte *Y,
    int no_vectors, char **F, 
    int no_compressed, uint64_t **cF, 
    uint64_t *cb_init,
    uint32_t pass_id,
    uint32_t larger_prime,
    map < branch_address_pair, set<int> > &joint_dependencies, 
    set<int> &set_unmutable
)
{
    
    uint64_t *cb        = (uint64_t *)malloc( sizeof(uint64_t) * no_compressed );  
    map <branch_address_pair,vector<vector<uint32_t>> > z ;

    int per_vec = 1;
    for(int jj=0; jj<no_vectors; jj++){

        for( int pv = 0; pv < per_vec; pv++){

                printf(" ( %d %d ) ",jj, pv); fflush(stdout);

                memcpy(Y,X, no_input_bytes);
                for( int i=0; i<no_input_bytes; i++)
                    if (  F[jj][i] ){
                        int index = permute( i, larger_prime, no_input_bytes) ;
                        if( set_unmutable.find( index ) == set_unmutable.end() )
                            Y[ index ] = X[ index ] ^ differences[pv][ index ];
                    }

                write_bytes_to_binary_file(TMP_INPUT, Y, no_input_bytes);
                if( forced_inference )  run_one_fork( RUN_FORCER );
                else                    run_one_fork( RUN_GETTER );


                if( forced_inference && store_branches_tot > __new_branch_index[0]){
                    printf( KERR "[missed %d/%d]" KNRM, __new_branch_index[0], store_branches_tot );
                }


                int ii = 0;
                map <int,int> counters;
                while ( 
                            forced_inference && 
                                ii < store_branches_tot &&  branches_trace[ii]  == store_branches_trace[ii]
                        
                        || !forced_inference && 
                                ii < __new_branch_index[0] 
                    )
                {


                    auto zz = counters.find( branches_trace[ii] );
                    if( zz == counters.end() ) counters.insert( { branches_trace[ii], 0} );
                    else zz -> second ++;
                    zz = counters.find( branches_trace[ii] ) ;  

                    branch_address_pair ba = get_ba( zz->first , zz->second ) ;

                    if( target_branches.find( ba ) == target_branches.end() ){
                        ii++;
                        continue;
                    }

                    if(     forced_inference && BRANCH_IS_UNIMPORTANT( store_branches_opera[ii] ) ){
                        ii++;
                        continue;
                    }

                

                    uint32_t val =  compute_val(ii);
                    int taken = branches_taken[ii] ;


                    if ( z.find(ba) == z.end() )
                        z.insert( {ba,vector<vector<uint32_t>>() });
                    auto it = z.find( ba );
                    vector <uint32_t> el;
                    uint32_t code = (jj<<16) ^ pv;
                    el.push_back( code );
                    el.push_back( val );
                    el.push_back( taken );
                    it->second.push_back( el );

                    if( it->second.size() > per_vec*no_vectors){
                        printf("Impossible size for branch: %08x %4d\n", ba.first, ba.second );
                        exit(0);
                    }

                
                    ii++;
                }

            if( 0 == jj) break;
        }
    }
    printf("\n");
   

    printf( KINFO "#Candidates: %ld\n" KNRM, z.size() );


    int cand_more_than_1 = 0;
    int cand_founds = 0;

    int tot_reported_no_primary = 0;            
    for(auto it=z.begin(); it!=z.end(); it++){

        branch_address_pair ba = it->first;
        if ( it->second.size() == 0 || it->second[0][0] != 0){
            tot_reported_no_primary ++;
            if( PRINT_DEPENDENCY_INFERENCE_INFO && tot_reported_no_primary < 10)
                printf("No primary input for %08x %4d\n", ba.first, ba.second );
            continue;
        }



        // do not check ones that do not have more than 0 different value
        set <int> v;
        for( int j=0; j<it->second.size(); j++)
            v.insert( it->second[j][1] );
        if( v.size() <= 1) continue;

        cand_more_than_1 ++;


        bool no_more_1 = false;
        map < pair<int,int>, bool > same12;
        map < pair<int,int>, bool > same2;
        for( int ind1=0; ind1< no_vectors; ind1++)
            for( int ind2=ind1+1; ind2 < no_vectors; ind2++){
                same12.insert ( {make_pair(ind1,ind2), true} );
                same2.insert  ( {make_pair(ind1,ind2), true} );
            }


 
        // compare between each other
        int ind1 = 0;
        while( ind1 < it->second.size() )
        {
            int jj1 = it->second[ind1][0] >> 16;
            int pv1 = it->second[ind1][0] & 0xffff;
            int primary_ind1 = ind1;
            int ind2 = ind1 + 1;
            while ( ind2 < it->second.size() && jj1 == ( it->second[ind2][0] >> 16 )  ) 
                ind2 ++;
            if( ind2 >= it->second.size() ) break;

            while( 1 ){
                
                ind1 = primary_ind1;
                jj1 = it->second[ind1][0] >> 16;

                int jj2 = it->second[ind2][0] >> 16;
                int pv2 = it->second[ind2][0] & 0xffff;
                bool found_different12 = false;
                bool found_different2 = false;

                while ( 1 ){
                    if( pv1 == pv2 ){
                        if( it->second[ind1][1] ^ it->second[ind1][2] ^ 
                            it->second[ind2][1] ^ it->second[ind2][2]  )
                            found_different12 = true;
                        if( it->second[ind1][2] ^ it->second[ind2][2]  )
                            found_different2 = true;
                        ind1++;
                        ind2++;
                    }
                    else if( pv1 < pv2 ) ind1++ ;
                    else if( pv1 > pv2 ) ind2++ ;

                    if( ind1 >= it->second.size() || ind2 >= it->second.size() ) break;
                    if( jj1 != (it->second[ind1][0] >> 16) || jj2 != (it->second[ind2][0] >> 16))
                        break;
                }
                if( found_different12 )
                    same12.find( make_pair(jj1,jj2) )->second = false;
                if( found_different2 )
                    same2.find( make_pair(jj1,jj2) )->second = false;


                if( ind1 >= it->second.size() ) break;

                while( ind2 < it->second.size() && jj2 == (it->second[ind2][0] >> 16))
                    ind2 ++;

                if( ind2 >= it->second.size() ) {
                    while(ind1 < it->second.size() && jj1 == (it->second[ind1][0] >> 16 ) )
                        ind1 ++;
                    break;                
                }
            }
        }


        memcpy( cb, cb_init, sizeof(uint64_t) * no_compressed );


        no_more_1 = false;
        bool found_1;
        uint64_t cfound_1;
        for( auto it=same12.begin(); it!= same12.end(); it++){

            if( ! it->second ) continue;

            //
            // do not compare two none-zero vectors
            //
            if( it->first.first != 0) continue;

            cfound_1 = 0;
            for( int j=0; j<no_compressed; j++)
            if( cb[j] )
            {
                uint64_t t = cF[ it->first.first][j] ^ cF[ it->first.second][j] ;
                cb[j] |= t;
                cb[j] ^= t; 
                cfound_1 |= cb[j] ;
            }
 
            if( !cfound_1 ){
                no_more_1 = true;
                break;
            }
        }


        
        int sumb = 0;
        for( int i=0; i<no_compressed; i++)
            sumb += hamming_weight64( cb[i] );

        if( sumb > 0 ){
            if( 0 == pass_id || 
                0 <  pass_id && sumb <= 4 && joint_dependencies.find( ba ) == joint_dependencies.end() 
            ){
                set <int> current_branch_dependencies;
                if( sumb < MAX_DEPENDENCIES_INTERMEDIATE_STORE_PER_BRANCH ){
                    for( int j=0; j<no_compressed; j++)
                        if( cb[j] )
                            for( int i=0; i<64; i++)
                                if( 64*j + i < no_input_bytes && ( (cb[j] >> (63 - i)) & 1) ){
                                    int index = permute( 64*j + i, larger_prime, no_input_bytes );
                                    if( set_unmutable.find( index ) == set_unmutable.end() )
                                        current_branch_dependencies.insert( index );
                                }
                }
                else{
                    // randomly jump over the indices to select the important among the set of all 
                    uint32_t alar_prime = larger_primes[ mtr.randInt() % NO_LARGER_PRIMES ];
                    for( int jj=0; jj<no_compressed; jj++){
                        int j = (alar_prime * jj ) % no_compressed;
                        if( cb[j] ){
                            for( int i=0; i<64; i++)
                                if( 64*j + i < no_input_bytes && ( (cb[j] >> (63 - i)) & 1) ){
                                    int index = permute( 64*j + i, larger_prime, no_input_bytes );
                                    if( set_unmutable.find( index ) == set_unmutable.end() ){
                                        current_branch_dependencies.insert( index );
                                        if( current_branch_dependencies.size() >= MAX_DEPENDENCIES_INTERMEDIATE_STORE_PER_BRANCH )
                                            break;
                                    }
                                }
                        }
                        if( current_branch_dependencies.size() >= MAX_DEPENDENCIES_INTERMEDIATE_STORE_PER_BRANCH )
                            break;
                    }

                }

                joint_dependencies.insert( {ba, current_branch_dependencies} );
            }
            else{

                auto zz = joint_dependencies.find( ba );
                if( zz != joint_dependencies.end() ){
                    
                    set<int> found;
                    for( int j=0; j<no_compressed; j++)
                        if( cb[j] )
                            for( int i=0; i<64; i++)
                                if( 64*j + i < no_input_bytes && ( (cb[j] >> (63 - i)) & 1) ){
                                    int index = permute( 64*j + i, larger_prime, no_input_bytes );
                                    if( zz->second.find( index ) != zz->second.end() )
                                        found.insert( index );
                                }

                    zz->second = found;
                    if( zz->second.size() == 0)
                        joint_dependencies.erase( zz );
                }
            }
        }

    }

    free( cb );

}




void  branch_inference_fast(string original_input,  int iter_no, bool forced_inference, 
    vector<int> unmutable, float max_time )
{

    int tot_slow_inferences = 0;
    int tot_reported_no_primary = 0;
    int cand_more_than_1 = 0;
    int cand_founds = 0;


    set <branch_address_pair> target_branches;
    for( int i=0; i< branch_addresses.size(); i++){
        branch_address_pair ba = get_ba( branch_addresses[i][0],branch_addresses[i][1] );
        if( branch_exist(ba) && branch_good_for_iteration(ba) )
            target_branches.insert( ba );
    }

    if( PRINT_DEPENDENCY_INFERENCE_INFO ){
        printf("-------------------------------------------------------------------\n");
        printf("----------- Complete byte dependency inference \n");
        printf("----------- #branches   %ld  :   #targets    %ld  :  forced  %d \n", 
            branch_addresses.size(), target_branches.size() , forced_inference);
        printf("-------------------------------------------------------------------\n");
    }


    // read the original input
    if( !prepare_tmp_input( original_input )) return;
    int no_input_bytes;
    unsigned char* X = read_binary_file(TMP_INPUT, no_input_bytes);
    if ( NULL == X){
        printf( KERR "Cannot read the original input file:%s\n" KNRM, original_input.c_str());
        return;
    }
    if( 0 == no_input_bytes )return;
    unsigned char* Y = (unsigned char *)malloc(no_input_bytes);


    // get the exp 2 length
    int exponent = (int(log(no_input_bytes)/log(2)) < log(no_input_bytes)/log(2))?1:0 ;
    exponent += int(log(no_input_bytes)/log(2));
    int ex_len = 1<<exponent;
    
    // generate all required inversion vectors 
    int no_vectors;
    char**  F = generate_flips(ex_len, no_vectors);


    set<int> set_unmutable;
    for( int i=0; i<unmutable.size(); i++)
        set_unmutable.insert( unmutable[i] );


    printf("Unmutable size:  %ld \n", unmutable.size() );


    int no_compressed = ceil (no_input_bytes/64.0 );
    uint64_t **cF;
    cF = (uint64_t **)malloc(sizeof(uint64_t *) * no_vectors );
    for( int i=0; i<no_vectors; i++)
        cF[i] = (uint64_t *)malloc( sizeof(uint64_t) * no_compressed);

    for( int i=0; i<no_vectors; i++)
        for( int j=0; j<no_compressed; j++)
            cF[i][j] = 0;

    for( int i=0; i<no_vectors; i++){
        for( int k = 0; k<no_input_bytes; k++){
            if( F[i][k] )
                cF[i][ k/64 ] ^=  ((uint64_t)1) << (63-(k % 64));
        }
    }




    uint64_t *cb_init   = (uint64_t *)malloc( sizeof(uint64_t) * no_compressed );
    for( int j=0; j<no_compressed; j++)
        cb_init[j] = uint64_t(-1);
    if( no_input_bytes/64.0 != no_input_bytes/64 )
        cb_init[no_compressed-1] ^= ( ((uint64_t)1) << (64 - ( no_input_bytes % 64) ) )   - 1; 
    for( int i=0; i<unmutable.size(); i++)
        cb_init[ unmutable[i]/64 ] ^=  ((uint64_t)1) << (63 - (unmutable[i] % 64) );



    map < branch_address_pair, set<int> > joint_dependencies;
    set < int > chosen_primes;
    int32_t larger_prime;

    START_TIMER( begin_time );
    
    for( int pass_id=0; pass_id<3; pass_id++){

        if( pass_id >= NO_LARGER_PRIMES ){
            printf("cannot choose that many primes: %d %d\n", pass_id, NO_LARGER_PRIMES );
            exit(2);
        }

        if( 0 == pass_id )
            larger_prime = 1;
        else{
            int pind;
            do {
                pind = mtr.randInt() % NO_LARGER_PRIMES;
            }while ( chosen_primes.find( larger_primes[pind]) != chosen_primes.end() );
            chosen_primes.insert( larger_primes[pind] );
            larger_prime = larger_primes[pind];       
        }
 
        // fix the unmutable
        for( int j=0; j<no_compressed; j++)
            cb_init[j] = uint64_t(-1);
        if( no_input_bytes/64.0 != no_input_bytes/64 )
            cb_init[no_compressed-1] ^= ( ((uint64_t)1) << (64 - ( no_input_bytes % 64) ) )   - 1; 
        for( int j=0; j<no_compressed; j++)
                for( int i=0; i<64; i++){
                    int index = permute( 64*j + i, larger_prime, no_input_bytes );
                    if( set_unmutable.find( index ) != set_unmutable.end() )
                        cb_init[j] ^= ((uint64_t)1) << (63 - i);
                }
        

        // run one pass
        one_pass_inference_complete( forced_inference,
                    target_branches,
                    no_input_bytes, X, Y,
                    no_vectors, F, 
                    no_compressed, cF,  
                    cb_init,
                    pass_id,
                    larger_prime,
                    joint_dependencies,
                    set_unmutable
        );

        printf("Joint size: %ld \n", joint_dependencies.size() );

        // check if second pass is needed
        int more_than_max = 0;
        for( auto it= joint_dependencies.begin(); it != joint_dependencies.end(); it++){
            more_than_max += it->second.size() > MAX_DEPENDENCIES_PER_BRANCH;
        }
        printf( KINFO "Branches with more than 10 depenendent bytes: %d\n" KNRM, more_than_max);
        if( more_than_max <= 10 
            && !( more_than_max > 0 && joint_dependencies.size() > 0 && (float)more_than_max/joint_dependencies.size() > 0.25 ) 
        ) break;

        if( TIME_DIFFERENCE_SECONDS( begin_time ) > max_time )
            break;
    }


    for( auto it= joint_dependencies.begin(); it != joint_dependencies.end(); it++){
    
        auto set_dep_size = it->second.size() ;

        if( set_dep_size ) 
            cand_more_than_1 ++;

        if( set_dep_size > 0 && set_dep_size <= MAX_DEPENDENCIES_PER_BRANCH ){
            vector <int> found_vars;
            for( auto zz = it->second.begin(); zz != it->second.end(); zz++)
                found_vars.push_back( *zz );

            auto ba = it->first;
            if( find(branches_with_dependency.begin(), branches_with_dependency.end(), ba ) == branches_with_dependency.end() ) {
                branches_with_dependency.push_back(it->first);
            }

            if( found_vars.size() > 0){
                branch_add_dependency( ba, found_vars, false);
                cand_founds ++; 

            }

        }
        else if( set_dep_size > 0 && set_dep_size > MAX_DEPENDENCIES_PER_BRANCH )
        {
            vector <int> found_all_vars;
            for( auto zz = it->second.begin(); zz != it->second.end(); zz++)
                found_all_vars.push_back( *zz );

            vector <int> found_vars;
            while( found_vars.size() < MAX_DEPENDENCIES_PER_BRANCH){
                int chosen_index = mtr.randInt() % found_all_vars.size() ;
                found_vars.push_back ( found_all_vars[ chosen_index ] );
                found_all_vars.erase( found_all_vars.begin() + chosen_index );
            }

            auto ba = it->first;
            branch_add_dependency( ba, found_vars, false);
    
        }

    }

   
    printf( KINFO "#Candid > 1   : %d \n" KNRM, cand_more_than_1 );
    printf( KINFO "#Candid founds: %d \n" KNRM, cand_founds );


    free( X );
    free( Y );

    for( int i=0; i<no_vectors; i++)
        free ( F[i] );
    free( F );

    for( int i=0; i<no_vectors; i++)
        free( cF[i] );
    free( cF );   

    free( cb_init );    


    
}







