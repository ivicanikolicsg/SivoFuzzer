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
static Time mbegin_time;
static byte *y;
static string original_input;

map< branch_address_pair, int > dep_newly_found_bas;
vector< node_mab > mab_choice_solver;

static map < uint32_t , vector<int> > address_to_current_indices;



void init_fuzzer_system( int init_no_execs, byte *init_x, int init_lenx, string roriginal_input, Time begin_time  )
{

    x_ri        = init_x;
    lenx        = init_lenx;
    no_execs    = init_no_execs;
    mbegin_time = begin_time;

    y = (byte *)malloc(lenx + 1);

    original_input = roriginal_input;
    
    mab_recompute( mab_choice_solver );

    
    address_to_current_indices.clear();
    for( int i=0; i< branch_candidates.size(); i++){

        uint32_t address_only = branch_candidates[i].target_branch_address.first;

        if ( address_to_current_indices.find( address_only ) == address_to_current_indices.end()  )
            address_to_current_indices.insert( { address_only, vector<int>() });
        address_to_current_indices.find( address_only )->second.push_back( i );

    }
}

void fini_fuzzer_system()
{
    free( y );

}



bool can_run_system( )
{
    return branch_candidates.size()  > 0;
}


int exec_block_system( float min_running_time  )
{
    int found_new = 0;
    set <int > curr_byte_pos;
    map <int,int> one_solution;
    bool is_solved = false;
    vector <double> multiplier;

        
    START_TIMER( begin_time_solver );
 
    // choose one
    if( branch_candidates.size() == 0) return 0;

    uint32_t cand_index = mtr.randInt() % address_to_current_indices.size();
    auto it_chosen_address = address_to_current_indices.begin();
    for( ; cand_index > 0 ; cand_index--, it_chosen_address++);       
    auto cand_only_address = it_chosen_address->first;
    vector <int> potential_indices = it_chosen_address->second;
    int final_cand_index = potential_indices[ mtr.randInt() % potential_indices.size() ];



    if( PRINT_DEBUG_FUZZER_SYSTEM){
        printf("potential NOT exist: %d \n", address_to_current_indices.find( cand_only_address ) == address_to_current_indices.end());
        printf("CAND: %d   %08x : ", cand_index, cand_only_address );
    }

    auto one_candidate = branch_candidates[ final_cand_index ];
    branch_address_pair target_branch_address   = one_candidate.target_branch_address;
    int target_index                            = one_candidate.target_index;
    int index_in_suggested                      = one_candidate.index_in_suggested;

    auto tmp_deps = branch_get_correct_dependency( target_branch_address);

    vector <bool> use_all;
    int solver_choice = mab_sample( mab_choice_solver, 2, false, use_all);
        
    fuzzer_current_main =  SOL_TYPE_SOLVER_A + solver_choice;

    int branch_type =  branch_addresses[ordered_suggested[index_in_suggested].second][3]; 
    // Compose the system that consists of all dependent vars of the branches
    set <branch_address_pair> dependent_branches;
    set <int> set_dep;
    for( int i=0; i<tmp_deps.size(); i++) set_dep.insert( tmp_deps[i] );
    if( 1 == solver_choice ){
        for( int j=0; j<=index_in_suggested; j++){
            auto tmp_depsZ = branch_get_correct_dependency( ordered_suggested[j].first  );
            for( int i=0; i<tmp_depsZ.size();i++)
                if( set_dep.find( tmp_depsZ[i] ) != set_dep.end() )
                    dependent_branches.insert( ordered_suggested[j].first );
        }
    }
    else 
        dependent_branches.insert( ordered_suggested[index_in_suggested].first ) ;

    vector < pair<branch_address_pair,int> > path;
    vector < uint32_t > path_index;
    for( int j=0; j<index_in_suggested; j++){

        if( dependent_branches.find( ordered_suggested[j].first ) == dependent_branches.end() ) continue;
        
        path.push_back( { ordered_suggested[j].first, 
            branch_addresses[ordered_suggested[j].second][3] == 2 ?
            1 : branch_addresses[ordered_suggested[j].second][2]  } ); 
        path_index.push_back ( ordered_suggested[j].second );
    }
    path.push_back( { ordered_suggested[index_in_suggested].first, 
        branch_addresses[ordered_suggested[index_in_suggested].second][3] == 2 ?
        0 : 1- ( branch_addresses[ordered_suggested[index_in_suggested].second][2] ) } ); 
    path_index.push_back(ordered_suggested[index_in_suggested].second );


    if( PRINT_DEBUG_FUZZER_SYSTEM){
        printf(KINFO "\t* Inverting branch %d: length %d : %08x %4d:  " KNRM, 
            target_index, lenx, branch_addresses[target_index][0], branch_addresses[target_index][1] );
        for( auto it=tmp_deps.begin(); it!=tmp_deps.end(); it++)
            printf("%d ", *it );
        printf("\n");
        printf("Path size  %ld   dependent set  %ld\n", path.size(), dependent_branches.size() );
    }

    int PPRINT_FDEBUG = 1;
    int times_cannot = 0;
    branch_address_pair last_failed_branch_address;
    set <branch_address_pair> branches_chosen_randomely;
    for(int current_trials = 0; current_trials < no_execs; current_trials++){

        if( TIME_DIFFERENCE_SECONDS(mbegin_time) >= min_running_time ) break; 


        memcpy( y, x_ri, lenx );

        int no_solved_constraints = 0;
        bool used_random = false;
        bool used_rand_interval = false;
        bool has_random_bytes = true;

        branches_chosen_randomely.clear();


        one_solution.clear();
        if( 1 == solver_choice){
            bool can_sample = sample_solution( path, y, lenx, no_solved_constraints, set_dep, used_random, used_rand_interval, has_random_bytes, branches_chosen_randomely, last_failed_branch_address, one_solution );
            if( !can_sample && !used_random ){
                if( PRINT_DEBUG_FUZZER_SYSTEM ) 
                    printf("Cannot solve/sample the  system \n");

                times_cannot++ ;
                break;
            }
        }
        else if ( 0 == solver_choice ){
            bool can_sample = sample_solution( path, y, lenx, no_solved_constraints, set_dep, used_random, used_rand_interval, has_random_bytes, branches_chosen_randomely, last_failed_branch_address, one_solution );
            if( !can_sample && !used_random ){
                if( PRINT_DEBUG_FUZZER_SYSTEM ) 
                    printf("Cannot solve/sample the system \n");
                break;
            }
        }
        write_bytes_to_binary_file(TMP_INPUT, y, lenx);


        int next_edge = 1;
        int good_exec = run_one_fork( RUN_GETTER );

        if ( !good_exec ) break;

        int trace_id  = execution_get_divergent_index( );
        int branch_id = trace_index_to_branch_address_index( trace_id );

        if( branch_id < 0){
            branch_id = target_index;
            next_edge = 0;
        }
        else if( branch_id > target_index){
            branch_id = target_index;
            next_edge = 0;
        }
        branch_address_pair branch_address= get_ba( branch_addresses[branch_id][0], branch_addresses[branch_id][1]  );
        last_failed_branch_address = branch_address;

        if( PRINT_DEBUG_FUZZER_SYSTEM ){
            printf("\tSystem %d :%5d:%5d:%x:%d   ", solver_choice, trace_id, branch_id, next_edge, lenx );
            printf("CBA: %08x %4d   iso %d:  :  \n", branch_address.first, branch_address.second,  branch_is_solved(branch_address) );
            fflush(stdout);
        }
        if( branch_id < 0 || branch_id >= branch_addresses.size()){
            printf( KERR "Something is wrong with branch_id:%d %ld\n" KNRM, branch_id, branch_addresses.size());
            break;
        }


        if( good_exec && execution_edge_coverage_update() )
        {
            check_and_add_solution( TMP_INPUT, SOL_TYPE_SOLVER_A + solver_choice, "" );
            finalize_solution( one_solution );

            found_new ++;
            found_new_cov += 2==glob_found_different;

        }

        
        if( next_edge > 0 && branch_id == target_index  ){

            if( PRINT_DEBUG_FUZZER_SYSTEM ) {
                printf("solution:\n");
                for(auto zz = one_solution.begin(); zz!= one_solution.end(); zz++)
                    printf("%d  ->   %d\n", zz->first, zz->second );
                printf("\n");
                printf( KBLU "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ SOLVED \n" KNRM );
            }

            is_solved = true;
            break;
        }


        // Add wrong single byte guess 
        // Branch that failed is the only that will add wrong byte guess
        if( used_random ){
            for( int i=0; i<path.size(); i++)
                if( path[i].first ==  branch_address ){
                    add_wrong_byte_guess( path[i].first, path[i].second, y, lenx  );
                    break;
                }
        }


        // If all of a sudden the system depends on previously unknown branch, 
        // add it to the system
        if( dependent_branches.find(branch_address) == dependent_branches.end() && 
            branch_id < target_index   ){

                if( PRINT_DEBUG_FUZZER_SYSTEM ) {
                    printf( KINFO "New branch: ");
                    branch_print_info( branch_address, 0 );
                    printf( "\n" KNRM );
                }
    
            auto tmp_depsZ = branch_get_correct_dependency( branch_address  );
            bool found_cand = false;

            // In mode 1, add the branch if it depends on some common vars with the final branch
            // In mode 2, simply add the vars of the new branch
            if( 0 == solver_choice){

                
                if( tmp_depsZ.size() == 0){

                    // add the address to later process it
                    if( dep_newly_found_bas.find ( branch_address ) == dep_newly_found_bas.end() )
                        dep_newly_found_bas.insert( {branch_address,0} );
                    auto it = dep_newly_found_bas.find( branch_address );
                    it->second ++;

                    if( it->second > 4  ){


                        set <int> found;
                        branch_address_pair ba = branch_address;
                        branch_insert_empty_branch( branch_address );   

                        tmp_depsZ = branch_get_correct_dependency( branch_address  );

                        if( PRINT_DEBUG_FUZZER_SYSTEM ) {
                            printf("renew: ");
                            for( int z=0; z<tmp_depsZ.size(); z++) printf("%d ", tmp_depsZ[z]);
                            printf("\n");
                        }

                    }
                }

                for( int i=0; i<tmp_depsZ.size(); i++)
                    set_dep.insert( tmp_depsZ[i] );
                    
            }

            for( int i=0; i<tmp_depsZ.size(); i++)
                if( set_dep.find( tmp_depsZ[i]) != set_dep.end()  ){

                    // ignore switch
                    if( 2 == branch_addresses[branch_id][3]) continue;

                    if( PRINT_DEBUG_FUZZER_SYSTEM ) 
                        printf( KMAG "Inserting %08x %4d \n" KNRM , branch_address.first, branch_address.second );
                    

                    path.insert( path.begin() + path.size()-1,  { branch_address,  ( branch_addresses[branch_id][2] ) } ); 
                    path_index.insert(path_index.begin() + path.size() -1 , branch_id );

                    dependent_branches.insert( branch_address );
                    found_cand = true;
                    break;
                }
            
            if( !found_cand ){
                if( PRINT_DEBUG_FUZZER_SYSTEM ) 
                    printf( "Did NOT add the branch\n");
                break;
            }
            else{
                if( PRINT_DEBUG_FUZZER_SYSTEM ) 
                    printf( "ADDed the branch\n");
                continue;
            }
        }


        // If they are all constraints (did not use randomness to solve any branch), 
        // and did not solve properly the system
        // then stop constraints solving 
        if( !used_random && (!used_rand_interval || !has_random_bytes) &&   
            (branch_id < target_index  && next_edge || branch_id == target_index && !next_edge) ){

                    if( PRINT_DEBUG_FUZZER_SYSTEM ){ 
                        printf( KERR "All solvable constraints, but the solution is incorrect: %d %d %d \n" KNRM,
                        used_random , used_rand_interval , has_random_bytes  );
                    }                   
            break;
        }
    }


    // for solver type
    auto timf = TIME_DIFFERENCE_SECONDS( begin_time_solver );
    mab_update( mab_choice_solver, solver_choice, found_new, timf );


    return found_new;
}


