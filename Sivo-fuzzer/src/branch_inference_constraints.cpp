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
#include "branches.h"
#include "fork_servers.h"
#include "executions.h"


//
//
// Infer branch constraints that can be inverted with the system solver.
// For this, need to find the direct copies of bytes, and determine the types of the branch conditions
//
//

void  value_constraints_inference(
        vector <branch_address_pair> &targets1, 
        vector <branch_address_pair> &targets2, 
        string original_input)
{


    // read the original input
    if( !prepare_tmp_input( original_input )) return;
    int no_input_bytes;
    unsigned char* X = read_binary_file(TMP_INPUT, no_input_bytes);
    if ( NULL == X){
        printf( KERR "Cannot read the original input file:%s\n" KNRM, original_input.c_str());
        return;
    }
    if( 0 == no_input_bytes )return;

    byte *Y = (byte *)malloc( no_input_bytes);


    int TRIALS = 3;
    int star = (mtr.randInt() % 100);
    map < branch_address_pair , uint32_t > max_seen_dep;
    for(int t=1; t<TRIALS; t++){

        if( 0 == t )
            memcpy(Y, X, no_input_bytes);
        else{
            uint8_t val = t+ 100 + star;
            for( int i=0; i<no_input_bytes; i++){
                Y[i] = val;
                val++;
                if(val == 0) val++;
            }
        }

        write_bytes_to_binary_file(TMP_INPUT, Y, no_input_bytes);
        run_one_fork( RUN_FORCER );

        int ii = 0;
        map <int,int> counters;
        while (ii < store_branches_tot  
        &&     branches_trace[ii]  == store_branches_trace[ii])
        {

            if( BRANCH_IS_UNIMPORTANT(branches_operations[ii]) ) {
                ii++;
                continue;
            }

            // if it is cond if, then the inference comes from branch_val1, branch_val2
            if( BRANCH_IS_IF(branches_operations[ii]) && !USED_BRANCH_VALS12( branches_operations[ii]) ){
                ii++;
                continue;                
            }


            auto zz = counters.find( branches_trace[ii] );
            if( zz == counters.end() )
                counters.insert( { branches_trace[ii], 0} );
            else
                zz -> second ++;
            zz = counters.find( branches_trace[ii] ) ;   

            branch_address_pair ba = get_ba( zz->first , zz->second ) ;

            if( branch_get_cannot_infer_constraints( ba ) ){ 
                ii++;
                continue;
            }

            if( !branch_good_for_iteration( ba ) ){
                ii++;
                continue;
            }


            uint32_t value = branches_value1[ii];
            uint32_t consz = branches_value2[ii];
            uint8_t compare_type = UNPACK_BRANCH_COMPARE_TYPE(branches_operations[ii] );
            uint8_t compare_oper = UNPACK_BRANCH_COMPARE_OPER(branches_operations[ii] );

            // if switch, they are different
            if( BRANCH_IS_SWITCH(branches_operations[ii]) ){
                value = branches_taken[ii];
                consz = 0;
                compare_type = 111;
                compare_oper = 111;
            }


            ii++;
            
            uint32_t ocon;

            vector<uint32_t> valuez;
            vector<uint32_t> conszz;
            vector<uint32_t> ori_cons;

            valuez.push_back( value );
            conszz.push_back( consz );
            ori_cons.push_back( consz );

            valuez.push_back( consz );
            conszz.push_back( value );
            ori_cons.push_back( value );

            
            for (int j=0; j<2; j++) {
                    value = valuez[j];
                    consz = conszz[j];
                    ocon  = ori_cons[j];

                    if(PRINT_DEBUG_INFER_CONSTRAINTS) {
                        printf("Address: %5d : %08x %4d : %ld :   %8x %8x\n", ii, ba.first, ba.second, 
                        get_dependency( ba ).size(), 
                        value, consz);
                    }

                    // find correct vars
                    vector <int> dep = get_dependency( ba );
                    if( dep.size() == 0) continue;

                    vector <uint8_t> vals;
                    for( int j=0; j<4; j++)
                        vals.push_back( (value >> ((3-j)*8)) & 0xff  );

                    vector <uint8_t> conz;
                    for( int j=0; j<4; j++)
                        conz.push_back( (consz >> ((3-j)*8)) & 0xff  );


                    if(PRINT_DEBUG_INFER_CONSTRAINTS) printf(" dep vars: ");
                    map <int,int> true_deps;
                    vector <int> new_dep;
                    bool used_vals[4] = {false,false,false,false};
                    for( int i=0; i<dep.size(); i++){
                        for( int j=3; j>=0; j--)
                            if( !used_vals[j] && dep[i] < no_input_bytes &&  vals[j] == Y[dep[i]] ){
                                used_vals[j] = true;
                                true_deps.insert( {dep[i],j});
                                new_dep.push_back( dep[i] );
                            }
                        if(PRINT_DEBUG_INFER_CONSTRAINTS) printf(" ( %d %x ) ", dep[i], Y[dep[i]] );
                    }
                    if(PRINT_DEBUG_INFER_CONSTRAINTS) {
                        printf("\n");
                        printf(" values : ");
                        for( int i=0; i<vals.size(); i++)
                            printf(" %x ", vals[i] );
                        printf("\n");

                    for( auto it=true_deps.begin(); it!= true_deps.end(); it++)
                        printf("dep: %d %d\n", it->first, it->second);
                    }

                    // this is likely wrong; with some prob try to get the fourth byte
                    if( 3 == true_deps.size() && (mtr.randInt() % 13) ){
                        set<uint8_t> sdep;
                        bool nf = true;
                        if(PRINT_DEBUG_INFER_CONSTRAINTS) printf("SET: ");
                        for( auto it=true_deps.begin(); it!=true_deps.end(); it++){
                            if(PRINT_DEBUG_INFER_CONSTRAINTS) printf("%x ", Y[it->first] );
                            sdep.insert( Y[it->first] );
                        }
                        set<uint8_t> svals;
                        for( int i=0; i<4; i++) svals.insert ( vals[i] );
                        if(PRINT_DEBUG_INFER_CONSTRAINTS) printf("\n");
                        for( int i=0; i<vals.size() && nf; i++)
                            if( sdep.find( vals[i] ) == sdep.end() ){
                                if(PRINT_DEBUG_INFER_CONSTRAINTS) printf("TARGET: %x   %d %d \n", vals[i] , max( 0, dep[0]-20), min(dep[0] + 20, no_input_bytes) );
                                for( int j= max( 0, dep[0]-20); j < min(dep[0] + 20, no_input_bytes); j++ ){
                                    if(PRINT_DEBUG_INFER_CONSTRAINTS) printf("(%x)", Y[j]);
                                    if( true_deps.find( j ) == true_deps.end() && 
                                        vals[i] == Y[j] ){
                                            true_deps.insert( {dep[i], j});
                                            nf = false;
                                            break;
                                        }
                                }
                                if(PRINT_DEBUG_INFER_CONSTRAINTS) printf("\n");
                            }
                        if(PRINT_DEBUG_INFER_CONSTRAINTS) {  
                            for( auto it=true_deps.begin(); it!=true_deps.end(); it++)
                                printf("newtrudep: %d %d\n", it->first, it->second); 
                        }
                    }

                    // for now only deal with one byte dependencies
                    vector <uint32_t> constraints;
                    if( true_deps.size() > 0 ){

                        constraints.push_back( compare_oper );
                        constraints.push_back( compare_oper );   
                        constraints.push_back( compare_type );
                        constraints.push_back( compare_type );  
                        constraints.push_back( ocon );   
                    }

                    if( max_seen_dep.find(ba) == max_seen_dep.end() )
                        max_seen_dep.insert( {ba, 0} );
                    auto zz= max_seen_dep.find( ba );
                    if( true_deps.size() > zz->second ){
                        if(PRINT_DEBUG_INFER_CONSTRAINTS) printf("ADDING!\n");
                        zz->second = true_deps.size();
                        branch_add_constraints( ba, true_deps, constraints, j) ;
                    }
                    else{
                        if(PRINT_DEBUG_INFER_CONSTRAINTS) printf("Will not add: %ld %d\n", true_deps.size(), zz->second ); 
                    }

            }
        }

    }

    free( X );    
    free( Y );


}

