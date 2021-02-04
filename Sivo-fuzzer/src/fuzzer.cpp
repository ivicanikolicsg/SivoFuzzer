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

#include "branches.h"
#include "solutions.h"
#include "solving_constraints.h"
#include "branch_inference_slow.h"
#include "fuzzer.h"
#include "multi_arm_banditos.h"
#include "fork_servers.h"
#include "executions.h"
#include "nodes.h"


//
//
// Given seed, schedules fuzzers to run 
//
//


vector < node_mab > mab_choice_fuzzer_vanilla;
vector < node_mab > mab_choice_fuzzer_vanilla_cov;
vector < node_mab > mab_choice_fuzzer_dataflow;
vector < node_mab > mab_choice_fuzzer_dataflow_cov;

extern vector< node_mab > mab_choice_forced_exec;
extern vector< node_mab > mab_combiner_strategy;
extern vector< node_mab > mab_combiner_number;
extern vector< node_mab > mab_combiner_type;
extern vector< node_mab > mab_copyremove_mode;
extern vector< node_mab > mab_copyremove_lognumcutpaste;
extern vector< node_mab > mab_copyremove_loglength;
extern vector< node_mab > mab_bytesmutate_vanilla_ga_no_mutations;
extern vector< node_mab > mab_bytesmutate_vanilla_simple_or_ga;
extern vector< node_mab > mab_bytesmutate_vanilla_simple_weights_simple_or_weighted;
extern vector< node_mab > mab_bytesmutate_data_lognumber;
extern vector< node_mab > mab_bytesmutate_data_rand_or_count;
extern vector< node_mab > mab_mingler_number;
extern vector< node_mab > mab_choice_solver;

// for combiner and copy-remove 
extern double freqs_left[256], freqs_right[256];
extern double freqs_insert[256],freqs_put[256];

vector < targetNode > branch_candidates;
map < int, uint32_t > byte_to_branches;
vector < pair<branch_address_pair,int> > ordered_suggested;
set <int> set_all_dep_bytes;
set <uint32_t> current_nodes;
map <uint32_t, uint32_t> traceId_to_branch;
map < branch_address_pair, uint32_t> bAddr_to_traceId;


static float wavg_testcases_per_time = 0;
static float wavg_testcases_per_iter = 0;
static float wavg = 0.02;


static bool use_random_walk_score;
static int  use_unsolved_branches_score;

int trace_index_to_branch_address_index(int trace_ind)
{
    auto qq = traceId_to_branch.find( trace_ind );
    if (qq != traceId_to_branch.end() ) return qq->second;

    return -1;

}

void finalize_solution(    map <int,int> one_solution )
{
    if( one_solution.size() == 0) return;
    if( one_solution.size() > MAX_ONE_SOLUTION_SIZE) return;

    if( !last_execution_timed_out() ){
        tc_found_solutions.push_back( one_solution );
        all_found_solutions.push_back( one_solution );
        if( all_found_solutions.size() > MAX_STORE_NUMBER_SOLUTIONS )
            all_found_solutions.erase( all_found_solutions.begin() + 0 );
    }
}

// Initialize MABs for all fuzzers
void reinit_mab_fuzzers()
{
    
    // fuzzer
    mab_choice_fuzzer_vanilla.clear();
    mab_choice_fuzzer_vanilla_cov.clear();
    for( int i=0; i<4; i++){
        mab_choice_fuzzer_vanilla.push_back    ( mab_init(  ) );
        mab_choice_fuzzer_vanilla_cov.push_back( mab_init(  ) );
    }
    mab_choice_fuzzer_dataflow.clear();
    mab_choice_fuzzer_dataflow_cov.clear();
    for( int i=0; i<5; i++){
        mab_choice_fuzzer_dataflow.push_back    ( mab_init(  ) );
        mab_choice_fuzzer_dataflow_cov.push_back( mab_init(  ) );
    }

    // forced
    mab_choice_forced_exec.clear();
    for( int i=0; i<2; i++)
        mab_choice_forced_exec.push_back( mab_init() );
    

    // bytes mutation
    mab_bytesmutate_vanilla_ga_no_mutations.clear();
    mab_bytesmutate_vanilla_simple_or_ga.clear();
    mab_bytesmutate_vanilla_simple_weights_simple_or_weighted.clear();
    mab_bytesmutate_data_lognumber.clear();
    mab_bytesmutate_data_rand_or_count.clear();
    for( int i=0; i<8; i++) mab_bytesmutate_vanilla_ga_no_mutations.push_back( mab_init() );
    for( int i=0; i<2; i++) mab_bytesmutate_vanilla_simple_or_ga.push_back( mab_init() );
    for( int i=0; i<2; i++) mab_bytesmutate_vanilla_simple_weights_simple_or_weighted.push_back( mab_init() );
    for( int i=0; i<5; i++) mab_bytesmutate_data_lognumber.push_back(       mab_init() );
    for( int i=0; i<2; i++) mab_bytesmutate_data_rand_or_count.push_back(   mab_init() );



    //
    // copyremove fuzzer
    //
    mab_copyremove_mode.clear();
    mab_copyremove_lognumcutpaste.clear();
    mab_copyremove_loglength.clear();
    // mode
    for( int i=0; i<4; i++)                                 mab_copyremove_mode.push_back( mab_init() );
    // log2 of the number of removes/additions
    for( int i=0; i<8; i++)                                 mab_copyremove_lognumcutpaste.push_back( mab_init() );
    // log2 lengths of the added/remove chunks
    for( int i=0; i< log(MAX_TESTCASE_SIZE)/log(2)  ; i++)  mab_copyremove_loglength.push_back( mab_init() );
        

    // combiner
    mab_combiner_strategy.clear();
    mab_combiner_number.clear();
    mab_combiner_type.clear();
    for(int i=0; i<testcases_get_num_of_stats(); i++) mab_combiner_strategy.push_back( mab_init() );
    for(int i=0; i<8; i++) mab_combiner_number.push_back( mab_init() );
    for(int i=0; i<2; i++) mab_combiner_type.push_back( mab_init() );


    // mingler
    mab_mingler_number.clear();
    for(int i=0; i<20; i++) mab_mingler_number.push_back( mab_init() );
    

    // system solver
    mab_choice_solver.clear();
    for( int i=0; i<2; i++)
        mab_choice_solver.push_back( mab_init() );


    
    mab_discount_add( mab_choice_fuzzer_vanilla        , &discount_times   );
    mab_discount_add( mab_choice_fuzzer_vanilla_cov    , &discount_times_cov   );
    mab_discount_add( mab_choice_fuzzer_dataflow       , &discount_times   );
    mab_discount_add( mab_choice_fuzzer_dataflow_cov   , &discount_times_cov  );
    mab_discount_add( mab_combiner_strategy         , &discount_times   );
    mab_discount_add( mab_combiner_number           , &discount_times   );
    mab_discount_add( mab_combiner_type             , &discount_times   );
    mab_discount_add( mab_copyremove_mode               , &discount_times   );
    mab_discount_add( mab_copyremove_lognumcutpaste     , &discount_times   );
    mab_discount_add( mab_copyremove_loglength          , &discount_times   );
    mab_discount_add( mab_bytesmutate_vanilla_ga_no_mutations   , &discount_times   );
    mab_discount_add( mab_bytesmutate_vanilla_simple_or_ga      , &discount_times   );
    mab_discount_add( mab_bytesmutate_vanilla_simple_weights_simple_or_weighted , &discount_times   );
    mab_discount_add( mab_bytesmutate_data_lognumber            , &discount_times   );
    mab_discount_add( mab_bytesmutate_data_rand_or_count        , &discount_times   );
    mab_discount_add( mab_mingler_number                        , &discount_times   );
    mab_discount_add( mab_choice_solver                     , &discount_times  );

}


// prepares fuzzers 
void prepare_fuzzers()
{
    // counters for the learning in copyremove and combiner fuzzers
    memset(freqs_left,      0, sizeof(double) * 256 );
    memset(freqs_right,     0, sizeof(double) * 256 );
    memset(freqs_insert,    0, sizeof(double) * 256 );
    memset(freqs_put,       0, sizeof(double) * 256 );

    memset(freqs_chars,     0, sizeof(double) * 256 );
    memset(counts_chars,    0, sizeof(double) * 256 );


    reinit_mab_fuzzers();

}

// Discounts for MABs
void discount_freqs(float dgamma, int dis_cou)
{
    for( int i=0; i<256; i++){
        freqs_left[i]       *= pow( dgamma, dis_cou) ; 
        freqs_right[i]      *= pow( dgamma, dis_cou) ; 
        freqs_insert[i]     *= pow( dgamma, dis_cou) ; 
        freqs_put[i]        *= pow( dgamma, dis_cou) ; 

        counts_chars[i]     *= pow( dgamma, dis_cou) ; 
    }
}



//
//
// The fuzzer schedules
//
//
void fuzzer( bool vanilla_fuzzer, 
    testcase_file *one,
    int no_iterations,  int no_execs_block, 
    float min_running_time, 
    Time iteration_start_time
){

    START_TIMER(begin_time)

    string original_input           = one->testcase_filename;

    // prepare the input file (seed)
    prepare_tmp_input( original_input );


    ordered_suggested.clear();
    current_nodes.clear();
    branch_candidates.clear();
    byte_to_branches.clear();
    traceId_to_branch.clear();
    bAddr_to_traceId.clear();
    set_all_dep_bytes.clear();
    tc_found_solutions.clear();

    use_random_walk_score = 0;
    use_unsolved_branches_score = 0;

    bool dataflow_fuzzer = !vanilla_fuzzer;
    if( dataflow_fuzzer ) 
    {

        // Get the set of all branchs (ordered_suggested) and all nodes (current_nodes)
        set <branch_address_pair> set_suggested;
        for(int i=0; i<branches_with_dependency.size(); i++) set_suggested.insert( branches_with_dependency[i] );
        for( int i=0; i<branch_addresses.size(); i++){
            branch_address_pair ba = get_ba(branch_addresses[i][0],branch_addresses[i][1]); 
            auto it = set_suggested.find(ba); 
            if( it != set_suggested.end() ){ 
                ordered_suggested.push_back( {ba,i} );
                current_nodes.insert( branch_addresses[i][0] );
            }
        }

        // make sure all nodes are already present in the previous "Nodes" structure
        for( auto iz=current_nodes.begin(); iz!=current_nodes.end(); iz++){
            int node_testcase_count = nodes_get_testcase_count( *iz );
            if( node_testcase_count <= 0){
                continue;
            }
        }

        // if use_random_walk_score is set, then make random walks 
        // on the CFG, starting from the target node, and see how many 
        // previously unseen nodes will appear on the paths 
        use_random_walk_score = mtr.randInt() % 2;
        if( use_random_walk_score)
            do_random_walk_on_all_nodes( RANDOM_WALK_LENGTH );
            

        bool extreme = mtr.randInt() % 2;

        use_unsolved_branches_score = mtr.randInt() % 2 ;
        if( use_unsolved_branches_score )
            use_unsolved_branches_score = 5 + (mtr.randInt() % (50 + extreme * 500 ) );             

        //
        // Determine score of branches
        // Higher score gives higher chance of the branch being picked for fuzzing
        // by the dataflow fuzzers
        // 
        for( int i=0; i< ordered_suggested.size(); i++){

            targetNode n;
            n.target_branch_address = ordered_suggested[i].first;
            n.target_index = ordered_suggested[i].second;
            n.index_in_suggested = i;
            n.index_in_trace = branch_addresses[ n.target_index ][4];

            
            // prefer branches that are found more recently 
            int age = glob_iter - branch_get_found_at_iteration(n.target_branch_address); 
            n.score = 1.0 / (1 + log( 1 + age ) ); 
            
            // prefer branches that appear in fewer testcases
            int node_testcase_count = nodes_get_testcase_count( n.target_branch_address.first );
            if( node_testcase_count > 0  )  n.score *= 1.0 / node_testcase_count;
            else                            n.score *= 1.0 / (1 + get_no_testcases() );

            // prefer branches that have higher unsolved percentage 
            if( use_unsolved_branches_score ){
                int node_num_outcoming = nodes_get_number_solved( n.target_branch_address.first );
                int node_num_maxoutcom = nodes_get_number_max   ( n.target_branch_address.first );
                int  branch_type = branch_get_branch_type( n.target_branch_address );
                if (    node_num_outcoming >= 0  && 
                        node_num_maxoutcom >= 0  && 
                        branch_type > 0 
                    ) {

                        if (1==branch_type && node_num_outcoming < 2 ) 
                            n.score *= use_unsolved_branches_score;
                        if (2==branch_type && node_num_maxoutcom > node_num_outcoming ) 
                            n.score *= mmin( use_unsolved_branches_score/2.0 * ( node_num_maxoutcom - node_num_outcoming) , use_unsolved_branches_score)  ;
                        

                }
            }

            // prefer branches that were fuzzed less
            n.score *= 1/ (1 + pow(2,  1 + branch_get_times_fuzzed(n.target_branch_address) ) ) ;

            // prefer branches that have smaller repetition (max 2)
            n.score *= ( n.target_branch_address.second <= 2 ) ? (2 + extreme * 2000.0) : 1.0;

            // prefer branches that can lead to still unexplored branches
            n.score *= use_random_walk_score ? ( 1  + pow(2, (mtr.randInt() % 4) * nodes_get_random_walk_score(n.target_branch_address.first) ) ) : 1;

            branch_candidates.push_back ( n );

        }


        // map byte to branches 
        // used by some fuzzers
        for( int i=0; i< branch_candidates.size(); i++){
            auto branch_address = ordered_suggested[ branch_candidates[i].index_in_suggested ].first;
            auto deps = branch_get_correct_dependency( branch_address );
            for( int k=0; k<deps.size(); k++){
                int byte_pos = deps[k];
                auto it = byte_to_branches.find( byte_pos );
                if( it == byte_to_branches.end() ){
                    byte_to_branches.insert( {byte_pos, 0});
                }
                it = byte_to_branches.find( byte_pos );
                it->second ++;
            }
        }

        // map trace id to branch and vice-versa 
        // used by some fuzzers 
        for( int i=0; i<branch_addresses.size(); i++){
            traceId_to_branch.insert( {  branch_addresses[i][4], i });
            bAddr_to_traceId.insert( { get_ba( branch_addresses[i][0], branch_addresses[i][1]) , branch_addresses[i][4] } );
        }

        // determine all dependent bytes by the branches
        // used by some fuzzers
        for(int i=0; i<branches_with_dependency.size(); i++){ 
            auto dep_vars = branch_get_correct_dependency(  branches_with_dependency[i] );
            for( int j=0; j<dep_vars.size(); j++){
                set_all_dep_bytes.insert( dep_vars[j] );
            }
        }
    }

    if( dataflow_fuzzer && 0 == set_all_dep_bytes.size() ) return;





    // prepare and read the input file into array
    int lenx;
    byte * x = read_binary_file( TMP_INPUT , lenx );

    // frequencies and counts
    double s = 0;
    for( int i=0; i< lenx; i++)
        s += ++counts_chars[ x[i]];
    if( s > 0){
        double s2 = 0, c2 = 0;
        for( int i=0; i < 256; i++){
            freqs_chars[i] = (float) counts_chars[i] / s;

            s2 += freqs_chars[i];
            c2 += freqs_chars[i] > 0 ;

        }
        freqs_chars_avg = s2 / c2;
    }
    // remove outliers
    freqs_chars_avg = 0;
    int totnm = 0;
    float min_perct = 0.90;
    float closest_chars_avg = 1.0;
    for( int i=0; i<256; i++)
        totnm += freqs_chars[i] > 0;
    if( totnm > 0 ){
        for( int i=0; i<256; i++){
            if( freqs_chars[i] <= 0 ) continue;
            int ltotnm = 0;
            for( int j=0; j<256; j++)
                ltotnm += freqs_chars[j] >= freqs_chars[i] ;
            if( (float)ltotnm / totnm >= min_perct && (float)ltotnm / totnm < closest_chars_avg ){
                freqs_chars_avg = freqs_chars[i];
                closest_chars_avg = (float)ltotnm / totnm ; 
            }
        }
    }



    float   found_new;
    int     INIT_NO_ITERATIONS          = no_iterations;
    float   INIT_MIN_RUNNING_TIME       = min_running_time;
    bool    not_increased_no_iterations = true;
    int     begin_important_tc          = count_current_important_tc();
    int     found_any_new_cov           = false;



    // Initialize all fuzzers according to the chosen fuzzing strategy (vanilla or dataflow)
    // Vanilla includes:
    //          1) bytes mutation: positions are determined with GA 
    //          2) mingle        : apply previously found good mutations to current seed
    //          3) copyremove    : copy/remove of byte sequences of current seed
    //          4) combiner      : combine (concatenate bytes sequence of ) different seeds 
    // Data-flow includes:
    //          1) bytes mutations: positions chosen according to inference info
    //          2) mingle        : --||--
    //          3) branches      : mutate bytes of particular branches
    //          4) system        : solve systems to invert some branches
    //          5) branch_ga     : invert branches with GA
                    
    auto time_pre_init  = TIME_DIFFERENCE_SECONDS( begin_time ) ;
    START_TIMER( time_init );
    init_fuzzer_bytes_mutation  ( no_execs_block, x, lenx , vanilla_fuzzer, original_input, one->trace_hash, begin_time );
    init_fuzzer_mingle          ( no_execs_block, x, lenx , vanilla_fuzzer, begin_time );
    if( vanilla_fuzzer ){
        init_fuzzer_copyremove          ( no_execs_block, x, lenx , glob_iter, begin_time );
        init_fuzzer_combiner        ( no_execs_block, x, lenx, min_running_time / 5.0, begin_time  );
    }
    else{
        init_fuzzer_branches        ( no_execs_block, x, lenx, begin_time );
        init_fuzzer_system          ( no_execs_block, x, lenx , original_input, begin_time );
        init_fuzzer_branch_ga       ( no_execs_block, x, lenx, begin_time );
    }
    fuzzer_init_time        += TIME_DIFFERENCE_SECONDS( begin_time );
    auto time_only_init     =  TIME_DIFFERENCE_SECONDS( begin_time ) - time_pre_init;


    //
    // Deactivate some fuzzers if they don't have "candidates"
    //
    vector <bool> use_fuzzer;
    for( int i=0; i<5; i++)
        use_fuzzer.push_back( true );
                
    bool found_at_least_one = false;
    if( vanilla_fuzzer ){
        if( !can_run_fuzzer_mingle()    ) use_fuzzer[0] = false; else found_at_least_one = true;
        if( !can_run_bytes_mutation()   ) use_fuzzer[1] = false; else found_at_least_one = true;
        if( !can_run_fuzzer_copyremove()    ) use_fuzzer[2] = false; else found_at_least_one = true;
        if( !can_run_combiner ()        ) use_fuzzer[3] = false; else found_at_least_one = true;
        use_fuzzer[4] = false;

        // do not use *mingler* if reinitializing new coverage
        // because it leaks information (as it stores previous findings)  
        if( RE_INIT_USE)
            use_fuzzer[0] = false;
    }
    else{
        if( !can_run_system()           ) use_fuzzer[0] = false; else found_at_least_one = true;
        if( !can_run_branches()         ) use_fuzzer[1] = false; else found_at_least_one = true;
        if( !can_run_bytes_mutation()   ) use_fuzzer[2] = false; else found_at_least_one = true;
        if( !can_run_branch_ga()        ) use_fuzzer[3] = false; else found_at_least_one = true;
        if( !can_run_fuzzer_mingle()    ) use_fuzzer[4] = false; else found_at_least_one = true;

        if( RE_INIT_USE )
            use_fuzzer[4] = false;
    }


    if( !found_at_least_one ) return;

    // discounts
    mab_recompute( mab_choice_fuzzer_vanilla );
    mab_recompute( mab_choice_fuzzer_vanilla_cov     );
    mab_recompute( mab_choice_fuzzer_dataflow );
    mab_recompute( mab_choice_fuzzer_dataflow_cov     );


    float   latest_wavg_testcases_per_iter  = wavg_testcases_per_iter;
    float   latest_wavg_testcases_per_time  = wavg_testcases_per_time;
    int     cur_iteration                   = 0;


    auto time_prestart = TIME_DIFFERENCE_SECONDS( begin_time ) - time_only_init - time_pre_init ;

    vector <int> one_runs;
    for( int i=0; i<use_fuzzer.size(); i++)
        if( use_fuzzer[i] )
            one_runs.push_back( i );
    
    bool simple_run;

    while(1)
    {
        cur_iteration ++;
        found_new_cov = 0;      // too lazy to eliminate this global var used in coverage 

        // stop current fuzzing iteration if time budget has been spent
        if( TIME_DIFFERENCE_SECONDS(begin_time) > TIMEOUT_FUZZER_ROUND ){
            break;
        }


        // choose the fuzzer
        int fuzzer_choice = mab_sample( 
                vanilla_fuzzer ? 
                (coverage_type ? mab_choice_fuzzer_vanilla_cov  : mab_choice_fuzzer_vanilla   ) :
                (coverage_type ? mab_choice_fuzzer_dataflow_cov : mab_choice_fuzzer_dataflow  ) ,                 
                vanilla_fuzzer ? mab_choice_fuzzer_vanilla_cov.size() : mab_choice_fuzzer_dataflow_cov.size(), 
                PRINT_FUZZER_ITERATIONS , 
                use_fuzzer
                );


        simple_run =  cur_iteration - 1 < one_runs.size();
        if( simple_run )
            fuzzer_choice = one_runs[ cur_iteration - 1 ];


        if( fuzzer_choice < 0 ){
            return;
        }
        
        fuzzer_current_main = fuzzer_choice;
        fuzzer_current_sub  = 0  ;

                
        START_TIMER(timf)
        double begin_tot_fuzz_time          = tot_fuzz_time;
        double begin_coverage_update_time   = coverage_update_time;
        double begin_write_speed            = write_speed;


        int begin_crash_uniq = crash_uniq;


        uint32_t    COV_begin                       = get_coverage_entries();
        uint32_t    COV_begin_all                   = get_coverage_entries_all();


        // execute no_execs_block of the chosen fuzzer
        if( vanilla_fuzzer){
            if ( 0 == fuzzer_choice ) found_new = exec_block_fuzzer_mingle  ( min_running_time );
            if ( 1 == fuzzer_choice ) found_new = exec_block_bytes_mutation ( min_running_time );
            if ( 2 == fuzzer_choice ) found_new = exec_block_fuzzer_copyremove  ( min_running_time );
            if ( 3 == fuzzer_choice ) found_new = exec_block_combiner        ( min_running_time );
        }
        else{
            if ( 0 == fuzzer_choice ) found_new = exec_block_system         ( min_running_time );
            if ( 1 == fuzzer_choice ) found_new = exec_block_branches       ( min_running_time );
            if ( 2 == fuzzer_choice ) found_new = exec_block_bytes_mutation ( min_running_time );
            if ( 3 == fuzzer_choice ) found_new = exec_branch_ga            ( min_running_time );
            if ( 4 == fuzzer_choice ) found_new = exec_block_fuzzer_mingle  ( min_running_time );
        }
        
        
        found_any_new_cov |= found_new_cov>0;

        int tot_crash = crash_uniq - begin_crash_uniq;
        found_new       += tot_crash;
        found_new_cov   += tot_crash;


        auto tztim = TIME_DIFFERENCE_SECONDS(timf);
        if( vanilla_fuzzer ){
            mab_update( mab_choice_fuzzer_vanilla    , fuzzer_choice , (float)found_new,         tztim );
            mab_update( mab_choice_fuzzer_vanilla_cov, fuzzer_choice , (float)found_new_cov,     tztim );
        }
        else{
            mab_update( mab_choice_fuzzer_dataflow    , fuzzer_choice , (float)found_new,         tztim );
            mab_update( mab_choice_fuzzer_dataflow_cov, fuzzer_choice , (float)found_new_cov,     tztim );
        }



        // determine if it is time to stop fuzzing the current testcase
        // stop can be based on: 
        // 1) number of passed iterations, or
        // 2) time

        bool stop_based_on_iterations   = false;
        if( cur_iteration >= no_iterations )    stop_based_on_iterations = true;
        if( !stop_based_on_iterations ) continue;


        bool stop_based_on_time         = false;
        auto tim_dif = TIME_DIFFERENCE_SECONDS(begin_time);
        if( tim_dif >= min_running_time ) stop_based_on_time = true;
        if( !stop_based_on_time ) continue;


        if( stop_based_on_iterations ){

            // if the number of found testcases per iteration is above the average, 
            // then allow it to run for more iterations
            if( no_iterations < 5 * INIT_NO_ITERATIONS && 
                (count_current_important_tc() - begin_important_tc) / (cur_iteration + 1.0 ) >  latest_wavg_testcases_per_iter
            ){
                no_iterations += 1.0 * INIT_NO_ITERATIONS;
                latest_wavg_testcases_per_iter =  0.5 * latest_wavg_testcases_per_iter 
                                                + 0.5 * (count_current_important_tc() - begin_important_tc) / (cur_iteration + 1.0 );
                continue;
            }
        }

        if( stop_based_on_time ){

            // if the number of found testcases per time is above the weighted average 
            // then allow it to run for more time 
            if( min_running_time  < 5 * INIT_MIN_RUNNING_TIME && 
                (count_current_important_tc() - begin_important_tc) / TIME_DIFFERENCE_SECONDS(iteration_start_time) > latest_wavg_testcases_per_time 
            ){
                min_running_time += 1.0 * INIT_MIN_RUNNING_TIME;
                latest_wavg_testcases_per_time =  0.5 * (count_current_important_tc() - begin_important_tc) / TIME_DIFFERENCE_SECONDS(iteration_start_time) 
                                                + 0.5 * latest_wavg_testcases_per_time;
                continue;
            }
        }

        break;

    }



    auto time_only_runs = TIME_DIFFERENCE_SECONDS( begin_time ) -time_only_init-time_pre_init-time_prestart;


    // update the average number of testcases found per iterations and per time
    // it is exponential moving average
    float prev_wavg_testcases_per_iter = wavg_testcases_per_iter;
    float prev_wavg_testcases_per_time = wavg_testcases_per_time;
    wavg_testcases_per_iter  =  wavg * ( (count_current_important_tc() - begin_important_tc)/ ( cur_iteration + 1.0) ) 
                                + (1-wavg) *  wavg_testcases_per_iter ;
    wavg_testcases_per_time  =  wavg * ( (count_current_important_tc() - begin_important_tc)/ TIME_DIFFERENCE_SECONDS(iteration_start_time) ) 
                                + (1-wavg) *  wavg_testcases_per_time ;



    // Finalize fuzzers
    fini_fuzzer_bytes_mutation();
    fini_fuzzer_mingle();
    if( vanilla_fuzzer ){
        fini_fuzzer_copyremove();
        fini_fuzzer_combiner();
    }
    else{
        fini_fuzzer_branches();
        fini_fuzzer_system();
        fini_fuzzer_branch_ga();
    }
    
    free( x );



}