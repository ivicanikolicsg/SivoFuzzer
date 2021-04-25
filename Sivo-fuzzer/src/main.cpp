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

#include <chrono>
#include "misc.h"
#include "files_etc.h"
#include "testcases.h"
#include "branches.h"
#include "branch_inference.h"
#include "branch_inference_slow.h"
#include "branch_inference_constraints.h"
#include "branch_inference_misc.h"
#include "solving_constraints.h"
#include "solutions.h"
#include "fork_servers.h"
#include "fuzzer.h"
#include "executions.h"
#include "nodes.h"
#include "multi_arm_banditos.h"
#include "config.h"    



vector <branch_address_pair> branches_with_dependency; 
VM branch_addresses;

vector < node_mab > mab_fuzzer_vanilla_or_dataflow;
vector < node_mab > mab_fuzzer_vanilla_or_dataflow_cov;
vector < node_mab > mab_choose_percentage;
extern vector < node_mab > mab_choice_fuzzer_vanilla;
extern vector < node_mab > mab_choice_fuzzer_dataflow;
extern vector < node_mab > mab_choice_fuzzer_vanilla_cov;
extern vector < node_mab > mab_choice_fuzzer_dataflow_cov;

Time start_fuzzing_time;
double total_time = 0;
float time_vanilla_fuzzer              = 1.0;
float time_max_slow_inference       = 1.0;
float time_max_important_inference  = 3.0;
float time_max_unmutable_inference  = 3.0;
float time_vanilla_fuzzer_min          = 0.2; 
float time_vanilla_fuzzer_max          = 5.0;
float time_last_iteration_update    = 0;
float time_vanilla_fuzzer_increment    = 0.2; 

uint32_t tot_vanilla_runs              = 1;
uint32_t tot_dataflow_runs             = 1;

uint32_t *origin_values;

vector< node_mab > mab_choice_forced_exec;
vector <int> unmutable;
bool unmutable_inferred;

double  frac_time_tc_sample     = 0.0;
double  MAX_FRAC_TIME_TC_SAMPLE = 0.1;

extern map < pair<int, int>,int> true_coverage; 


/*
 * main - program entry point
 *  @argc : number of arguments
 *  @argv : arguments
 *    [0]    = "sivo"
 *    [1]    = output/ folder
 *    [2...] = afl-style launch comm for target binary
 *
 * NOTE: argp is kinda fucked on andromeda; don't know why but using it would
 *       be ideal in the future
 */
int main(int argc, char *argv[])
{
    double  fuzzer_time                = 0;
    double  tot_time                   = 0;
    double  tot_filestcupdates_time    = 0;
    double  tot_testcase_sample_time   = 0;
    double  tot_branch_inference_time  = 0;
    double  tot_branch_dependency_time = 0;
    double  tot_system_solve_time      = 0;
    double  exec_time_trace            = 0;
    double  exec_time_force            = 0;
    uint32_t last_coverage             = 0;
    bool    vanilla_fuzzer, dataflow_fuzzer;
    float min_run_time;
    int fuzzer_iterations;
    int failed_in_row = 0;
    int max_failed_in_row = 10;
    float CURRENT_TIMEOUT_EXECUTION    = TIMEOUT_EXECUTION;

    double full_time_spent[2] = {0.01,0.01}; 
    int fuzz_du_sm_sols[2] = {0,0};
    vector <float> choice_perct{0.9, 0.95, 0.99999999} ;
    start_fuzzing_time      = GET_TIMER();
    time_change_max_length  = GET_TIMER();


    /* check if we are using piping method hack */
    char *_using_piping = getenv("USING_PIPING");
    using_piping = (_using_piping != NULL);

    /* initialize target_comm with appropriate strings */
    int found__ = 2;
    for( int i=0; i<argc; i++){
        if( !strcmp( argv[i], "--" ) ){
            found__ = i+1;
            break;
        }
    }
    for (size_t i= found__; i<argc; ++i)
        target_comm.push_back(argv[i]); 
    

    /* check for slaves             */
    int slave = -1;
    for( int i=0; i+1< found__ ; i++)
        if( !strcmp( argv[i], "-S") || !strcmp( argv[i], "-s") ){
            FUZZER_OUTPUT_FOLDER_NAME = argv[i+1];
            printf( KINFO "Slave : %s\n" KNRM ,FUZZER_OUTPUT_FOLDER_NAME.c_str() );
        }



    /* set output path(s)                                                   *
     * NOTE: Aux/ must be somewhere in OUTPUT_FOLDER for the same reason as *
     *       tmp_input.txt; however, since this is a folder, it will be     *
     *       mistaken for an afl slave output folder (which can crash the   *
     *       fuzzer). so we place it i FUZZER_OUTPUT_FOLDER_NAME            */
    OUTPUT_FOLDER = string(argv[1]);
    FOLDER_AUX    = OUTPUT_FOLDER + "/" + FUZZER_OUTPUT_FOLDER_NAME + "/Aux";

    /* original initializations */
    init_folders();
    init_tmp_inputs();
    init_random();
    find_all_afl_instance_folders();
    setup_signal_handlers();
    prepare_fuzzers();
    init_coverage();
    create_lookup_for_compressed_bit();
    create_lookup_for_sol_id_to_string();
    init_differences();
    init_testcase_stuff();

    nodes_fill_ifs(target_comm[0] ) ;             // assume llvm-ifs-name is in the same folder as the binary
    nodes_fill_switch_cases(target_comm[0]);      // assume llvm-switch-name is in the same folder as the binary
    nodes_fill_cfg(target_comm[0] ) ;             // assume llvm-cfg-name ---||---

    setup_shmem();
    bind_to_free_cpu();
    setup_fork_servers();
    if( !init_fork_servers(TMP_INPUT) )
        exit(1);

    // MAB for vanilla/dataflow fuzzer
    for( int i=0; i<2; i++)                     mab_fuzzer_vanilla_or_dataflow.push_back        ( mab_init_allow_small( 16  ) );
    for( int i=0; i<2; i++)                     mab_fuzzer_vanilla_or_dataflow_cov.push_back    ( mab_init_allow_small( 16  ) );
    // MAB for percentage
    for( int i=0; i< choice_perct.size(); i++)  mab_choose_percentage.push_back    ( mab_init_value(0.1, 1.0) );

    time_vanilla_fuzzer = time_vanilla_fuzzer_min;

    // Discounts for MABs
    mab_discount_add(  mab_fuzzer_vanilla_or_dataflow         , &discount_times );
    mab_discount_add(  mab_fuzzer_vanilla_or_dataflow_cov     , &discount_times_cov );


    // re-inits are related to flushing and temporarily regenerating from scratch new coverage 
    if( RE_INIT_APPLY ){
        setup_reinit( true );
    }
    else{
        RE_INIT_USE = false;
    }


    glob_iter = 0;
    //
    // One Sivo iteration, i.e.
    // 1) select seed (testcase)
    // 2) apply mutations to produce new seeds
    // 3) store seeds that increase coverage
    while(1){        

        glob_iter++;

        SAMPLE_RANDOMLY_THIS_ROUND              = false;
        SAMPLE_RANDOMLY_THIS_ROUND_NO_UPDATE    = false;
        SAMPLE_NOT_RANDOMLY                     = false;


        // Slowly increase the maximum allowed length of a seed (testcase). 
        // This concerns only the fuzzing strategies that can increase the length of the seeds.
        // The max length is increased if in the last TIME_BETWEEN_MAX_LEN_INCR seconds, 
        // the percentage of new seeds does not exceed MIN_PERCT_NEW_TC - 1.
        if( TIME_DIFFERENCE_SECONDS (time_change_max_length ) > TIME_BETWEEN_MAX_LEN_INCR  ){

            time_change_max_length = GET_TIMER();
            if( !RE_INIT_USE ){

                float curtc = count_current_important_tc() + 1.0;
                if( curtc <= 1 || curtc / mmax(1,last_length_tc) <= MIN_PERCT_NEW_TC  ){
                    MAX_LENGTH_PROPOSE *= 2;
                    MAX_LENGTH_PROPOSE = mmin( MAX_LENGTH_PROPOSE, MAX_LENGTH_TOTAL );
                    printf( KINFO "Updated MAX_LENGTH_PROPOSE: %d \n", MAX_LENGTH_PROPOSE );
                }
                last_length_tc = curtc;
            }
        }

        
        //
        // Process all external testcase files from other instances (if there are masters,slaves, whatnot).
        //
        START_TIMER ( begin_time_files_tcupdates );
        find_all_afl_instance_folders();
        update_testcases_candidates();
        tot_filestcupdates_time += TIME_DIFFERENCE_SECONDS( begin_time_files_tcupdates );


        //
        // Switch indices of the labels of all basic blocks
        // 
        redo_with_different_index();


        //
        // Choose coverage type 
        // 0) Take into account all seeds, or 
        // 1) only seeds that increase coverage (and NOT seeds that provide ONLY new logarithmic count) 
        coverage_type = mtr.rand() < FUZZER_COVERAGE_RATIO;

        // 
        // Choose Fuzzer_strategy, i.e. either vanilla or data-flow.
        //        
        mab_recompute( mab_fuzzer_vanilla_or_dataflow );
        mab_recompute( mab_fuzzer_vanilla_or_dataflow_cov );
        vector <bool > use_choices = { true, true };
        fuzzer_type = mab_sample( 0 == coverage_type ? mab_fuzzer_vanilla_or_dataflow : mab_fuzzer_vanilla_or_dataflow_cov , 2, false , use_choices  );
        // at least 1/2 is vanilla fuzzer calls 
        if( mtr.rand() < 0.5 )
            fuzzer_type = 0;        
        // alter for the beginning few rounds to make sure both are chosen
        if      ( glob_iter < SAMPLE_RANDOMLY_UNTIL_ROUND/2 ) fuzzer_type = 1;
        else if ( glob_iter < SAMPLE_RANDOMLY_UNTIL_ROUND   ) fuzzer_type = 0;

        // if user specified to turn off some of them, then do so
        if( !USE_VANILLA_FUZZERS && !USE_DATAFLOW_FUZZERS){
            printf( KERR "Need to select at least one vanilla/dataflow fuzzer\n" KNRM );
            exit(1);
        }
        if( 0 == fuzzer_type && !USE_VANILLA_FUZZERS )   fuzzer_type = 1;
        if( 1 == fuzzer_type && !USE_DATAFLOW_FUZZERS )  fuzzer_type = 0;

        vanilla_fuzzer  = fuzzer_type == 0;
        dataflow_fuzzer = fuzzer_type == 1;


        START_TIMER(begin_time);


        // if initial reinit then sample randomly without updates
        if( RE_INIT_APPLY && RE_INIT_USE ){
            SAMPLE_RANDOMLY_THIS_ROUND_NO_UPDATE = true;
            if( mtr.randInt() % 2 )
                SAMPLE_NOT_RANDOMLY             = true;
        }

        // sometimes sample randomly with updates
        if( !(mtr.randInt() % 3 ) ){
            SAMPLE_RANDOMLY_THIS_ROUND = true;
        }


        //
        // Choose testcase class
        // 
        testcases_choose_class();

        //
        // Choose testcase criterion.
        // Then sample a testcase according to the chosen class and criterion. 
        //
        START_TIMER ( begin_time_testcase );
        testcase_file *one  = full_get_testcase( fuzzer_type );
        current_testcase    = one;
        tot_testcase_sample_time += TIME_DIFFERENCE_SECONDS( begin_time_testcase );



        //
        // If data-flow fuzzer, then first infer the branches, 
        // then the depenency ( + advanced inferences of dependency )
        // 
        if( dataflow_fuzzer ){

            // The branches are stored accross processing of different seeds 
            // (because the inference is not 100% accurate )
            // and updated for the particular seed 
            

            // Some branches just have too many different counts, i.e basic blocks are repeated too many times.
            // Jumping jindices decides which to take into account (most of the first 100, and then less and less)
            reinit_jumping_jindices();
            unmutable.clear();
            unmutable_inferred = false;

            // if there are too many stored branches
            // flush occasionally all found branches
            // and start inference from beginning
            if( branch_get_total_number() > MAX_STORE_BRANCHES &&  0 == (mtr.randInt() % 3) )
                branch_clear_all_branches();
            
            //
            // Get all branch addresses 
            // 
            START_TIMER ( branch_inference_time ) ;
            branch_addresses.clear();
            branch_addresses = execution_get_branches();
            import_branches( branch_addresses, glob_iter );
            auto one_time_branch_inference = TIME_DIFFERENCE_SECONDS( branch_inference_time );
            tot_branch_inference_time += one_time_branch_inference;

            if(branch_addresses.size() == 0){
                printf("No branch addresses produced from the testcase\n"); 
                continue;
            }
            
            // flush dependency every once in a while
            if( 0 == (mtr.randInt() % 10) )
                branch_remove_all_dependencies();


            // run once the testcases and store the trace, update coverage
            prepare_tmp_input( one->testcase_filename );
            run_one_fork( RUN_GETTER );  
            store_branch_trace();
            execution_edge_coverage_update();

            // store the computed values that later are used to infer depenendency 
            origin_values = (uint32_t *)malloc( __new_branch_index[0] * sizeof(uint32_t) );
            for( int i=0; i<  __new_branch_index[0]; i++){
                if( BRANCH_IS_IMPORTANT( branches_operations[i] ))
                    origin_values[i] = compute_val( i );
                else
                    origin_values[i] = 0;
            }


            branches_with_dependency.clear();

            // Set variable max dependencies per branch
            MAX_DEPENDENCIES_PER_BRANCH = 16 + (1<< (mtr.randInt() % 7) );

            int forced_inference = -1;

            // splurge factor incrases time budget of dependency inference
            // of certain (lucky) seeds
            float splurge_factor = 1.0;
            if( !(mtr.randInt() % 10 ) ) splurge_factor = 3.0;

            START_TIMER( begin_time_slow )

            // infer random individual bytes with some time budget
            setup_differences(  one->no_input_bytes );
            set <int> set_passed, set_important;
            int bytes_passed = branch_inference_try_all_bytes( 
                    one->testcase_filename, one->no_input_bytes, splurge_factor * time_max_slow_inference,
                    set_passed, set_important);



            auto dep_count_slow_var = count_deps_vars( branches_with_dependency );

            float perct_dep_to_passed = 100.0 * set_important.size() / set_passed.size();
            bool call_inference = set_passed.size() > 0 && perct_dep_to_passed < 50.0;


            // if not signifincant percentage passed, then can call  improved inferences I and II
            bool call_improved_inferences = false;
            if( one->no_input_bytes && (float)set_passed.size() / one->no_input_bytes < 0.50 )
                call_improved_inferences = true;


            //
            // inference improvement 1 (detect consecutive chunks of unimportant bytes and ignore those positions)
            // If not all bytes are important (perct_dep_to_passed is not 100%)
            // then:
            //      1) infer the important bytes
            //      2) run slow inference for them
            //
            START_TIMER( begin_time_infer_I )
            if( USE_IMPROVED_INFERENCE && 
                call_improved_inferences && 
                bytes_passed < one->no_input_bytes && 
                call_inference 
            ){

                START_TIMER( time_add_infer );

                int64_t init_dep_count = count_deps( branches_with_dependency );

                vector <int> importantbytes;
                float execs_per_sec = (float) bytes_passed / (splurge_factor * time_max_slow_inference);  
                START_TIMER( begin_time_infer_I_important );
                branch_inference_detect_important_bytes( one->testcase_filename, importantbytes, splurge_factor * time_max_important_inference, 
                        origin_values, execs_per_sec  );
                if( importantbytes.size() > 0 ){

                    // then infer the dependencies for the detected set 
                    set_passed.clear();
                    set_important.clear();
                    int new_bytes_passed = branch_inference_target_bytes( one->testcase_filename, 0, importantbytes, splurge_factor * time_max_slow_inference,
                            set_passed, set_important) ; 

                    float new_perct_dep_to_passed = 100.0 * set_important.size() / set_passed.size();

                }

            }    
            free( origin_values );


            int64_t dep_count_infer_II_vars = count_deps_vars(branches_with_dependency);  
            START_TIMER( begin_time_infer_II )

            //
            // inference improvement 2 (probabilistic group testing)
            // If slow did not pass all bytes 
            // then:
            //      1) Find "unmutable" bytes (if you mutate them end up with much different execution path)
            //      2) Fix unmutable bytes, then use probabilistic group testing
            //
            bool inferred_all_bytes_slow = bytes_passed >= one->no_input_bytes ;            
            mab_recompute( mab_choose_percentage );
            vector <bool> tup;
            int percentage_choice = mab_sample( mab_choose_percentage, mab_choose_percentage.size(), false , tup  );
            float percentage = choice_perct[  percentage_choice]; 


            if( USE_IMPROVED_INFERENCE && 
                call_improved_inferences && 
                !inferred_all_bytes_slow )
            {

                // decide on forced /unforced execution
                vector <bool> dum;
                forced_inference = mab_sample( mab_choice_forced_exec, 2, false, dum  );

                // infer the unmutable bytes 
                int64_t dep_count_before_inference_II = count_deps( branches_with_dependency );
                START_TIMER( begin_time_infer_II_unmut)
                unmutable.clear();
                unmutable_inferred = true;
                branch_inference_detect_unmutable_bytes( one->testcase_filename, unmutable, 
                splurge_factor * time_max_unmutable_inference, percentage, forced_inference);
            

                // if unmutable is not empty, then infer those bytes
                START_TIMER( begin_time_infer_II_slow_unmut)
                if( unmutable.size() > 0  && unmutable.size() < one->no_input_bytes )
                    branch_inference_target_bytes( one->testcase_filename, 0, unmutable, splurge_factor * time_max_slow_inference, set_passed, set_important );

                // fast inference
                START_TIMER( begin_time_infer_II_fast)
                if( unmutable.size() < one->no_input_bytes )
                    branch_inference_fast(one->testcase_filename, one->times_sampled_dataflow, forced_inference, unmutable, splurge_factor * time_max_unmutable_inference );
                
                // Update MAB
                int64_t dep_count_after_inference_II = count_deps( branches_with_dependency );
                float dep_increase = dep_count_before_inference_II > 0 ? 
                                ( mmin( 10.0 , (float) dep_count_after_inference_II / dep_count_before_inference_II ) ):
                                ( dep_count_after_inference_II > 0 ? 10.0 : 0.0 ) ; 
                mab_update( mab_choice_forced_exec, forced_inference, dep_increase, 1 );
            }
            dep_count_infer_II_vars = count_deps_vars(branches_with_dependency) - dep_count_infer_II_vars;


            //
            // Finish the remaining non-inferred bytes (if their percentage is low)
            //
            if( !call_improved_inferences && 
                one->no_input_bytes && 
                (float)set_passed.size() < one->no_input_bytes  
            ){
                START_TIMER( begin_time_slow2 )
                auto dep_count_slow2 = count_deps(branches_with_dependency);
                auto dep_count_slow_var2 = count_deps_vars( branches_with_dependency );

                vector<int> byte_positions;
                for( int i=0; i< one->no_input_bytes; i++)
                    if( set_passed.find( i ) == set_passed.end() )
                         byte_positions.push_back( i );

                branch_inference_target_bytes( one->testcase_filename, 0, 
                            byte_positions, splurge_factor * time_max_slow_inference,
                            set_passed, set_important); 
                dep_count_slow_var += count_deps_vars( branches_with_dependency ) - dep_count_slow_var2;
            }


            // Add sometimes previusly found branches  
            if( mtr.randInt() % 2 ){

                set <branch_address_pair> found_so_far;
                for(int j=0; j<branches_with_dependency.size(); j++)
                    found_so_far.insert( branches_with_dependency[j] );
            
                int smadds = 0;
                for(int j=0; j<branch_addresses.size(); j++){
                    branch_address_pair ba= get_ba( branch_addresses[j][0], branch_addresses[j][1]  );
                    if( branch_exist(ba) &&  
                        found_so_far.find(ba) == found_so_far.end()  &&
                        get_dependency(ba).size() > 0 
                        && branch_good_for_iteration( ba )  
                        )
                        {
                            found_so_far.insert( ba );
                            branches_with_dependency.push_back( ba );
                            smadds++;
                        }
                }
            }
            


            if(PRINT_DEBUG_INFERRED_BYTES)
                print_branches_with_dependency( branches_with_dependency );

            
            //
            // This is related to the system solver
            //
            set <branch_address_pair> to_remove_branches;
            vector <branch_address_pair> newly_found_dependent_branches;
            for( auto it=dep_newly_found_bas.begin(); it!=dep_newly_found_bas.end(); it ++) 
                if( it->second > 1 ){
                    branch_address_pair ba = it->first;
                    newly_found_dependent_branches.push_back( ba );
                    to_remove_branches.insert( ba );
                }
            for( auto it=to_remove_branches.begin(); it!=to_remove_branches.end(); it ++) 
                dep_newly_found_bas.erase( *it );
            value_constraints_inference(branches_with_dependency, newly_found_dependent_branches, one->testcase_filename);


            auto time_of_branch_dependency_inference = TIME_DIFFERENCE_SECONDS (branch_inference_time ) ;
            tot_branch_dependency_time += time_of_branch_dependency_inference;

            min_run_time = min(10.0, (double)one->times_sampled_dataflow) * time_of_branch_dependency_inference;
            min_run_time = max( (double)min_run_time, (double)time_vanilla_fuzzer );
            min_run_time = min( 10.0 , (double)min_run_time );
            fuzzer_iterations = 32 * one->times_sampled_dataflow ;

            free_differences();

            tot_dataflow_runs++;

            // update MAB for percentage
            if( dep_count_infer_II_vars > 0  ){
                float av = dep_count_infer_II_vars/( 1.0 + dep_count_slow_var ) ;
                av = mmax(0.01, mmin( 100, av ) );
                mab_update( mab_choose_percentage, percentage_choice , av * log( 1+ dep_count_slow_var ), log( 1+ dep_count_slow_var ) );
            }


        }
        else{

            min_run_time    = time_vanilla_fuzzer ;
            fuzzer_iterations = 8 ;
            tot_vanilla_runs ++;
        }

        // below are for stats
        uint64_t    begin_fork_executions           = tot_fork_executions;
        double      begin_tot_fuzz_time             = tot_fuzz_time;
        uint64_t    begin_coverage_update_total     = coverage_update_total;
        uint64_t    begin_coverage_update_total_bb  = coverage_update_total_bb;
        double      begin_coverage_update_time      = coverage_update_time;
        uint64_t    begin_trace_lengths             = trace_lengths;
        float       begin_write_speed               = write_speed;
        uint64_t    begin_write_length              = write_length;
        uint64_t    begin_tot_writes_to_file        = tot_writes_to_file;
        uint32_t    TC_begin                        = count_current_important_tc();
        uint32_t    COV_begin                       = get_coverage_entries_all();
        uint32_t    begin_timeouts                  = timeouts; 
        uint32_t    begin_solutions_on_timeout      = solutions_on_timeout; 

        

        START_TIMER( start_fuzzer_time);
        uint32_t begin_important_tc = count_current_important_tc();


        //
        //
        // Fuzz the seed according to the chozen fuzzing strategy
        //
        //
        fuzzer( vanilla_fuzzer , one, fuzzer_iterations, NO_EXECS_BLOCK, min_run_time, begin_time );


        // get information about coverage obtained in this iteration (used to update the MAB fuzzer choice)
        float straight_findings_cov = 0, straight_findings_cov_true = 0;
        for( int i=0; i< (vanilla_fuzzer ? mab_choice_fuzzer_vanilla.size() : mab_choice_fuzzer_dataflow.size()); i++){
            straight_findings_cov       += vanilla_fuzzer ? mab_choice_fuzzer_vanilla[i].new_num      :  mab_choice_fuzzer_dataflow[i].new_num       ;
            straight_findings_cov_true  += vanilla_fuzzer ? mab_choice_fuzzer_vanilla_cov[i].new_num  :  mab_choice_fuzzer_dataflow_cov[i].new_num   ;
        }
        auto tdiftim = TIME_DIFFERENCE_SECONDS( begin_time ) ;


        // update MABs 
        fuzz_du_sm_sols[ fuzzer_type ] += count_current_important_tc() - TC_begin;

        mab_update(   mab_fuzzer_vanilla_or_dataflow     ,  fuzzer_type , straight_findings_cov     , tdiftim );
        mab_update(   mab_fuzzer_vanilla_or_dataflow_cov ,  fuzzer_type , straight_findings_cov_true, tdiftim );

        discount_times      = straight_findings_cov;
        discount_times_cov  = straight_findings_cov_true;
        mab_discount_all_mabs();
        discount_freqs( Gamma , straight_findings_cov  );



        // update timings 
        fuzzer_time    += TIME_DIFFERENCE_SECONDS(start_fuzzer_time );
        total_time      = TIME_DIFFERENCE_SECONDS(start_fuzzing_time );
        frac_time_tc_sample = total_time > 0.1 ? tot_testcase_sample_time / total_time : 0.0 ;


        // update time spent
        full_time_spent[fuzzer_type] += TIME_DIFFERENCE_SECONDS( begin_time ); 


        // update MAB for sampling testcase method
        testcases_update_sampling_method ( straight_findings_cov , straight_findings_cov_true, tdiftim );

        // update number of executions and time for the chosen testcase
        testcases_update_testcase_stats(    one->testcase_filename, 
                                            tot_fork_executions-begin_fork_executions, 
                                            tot_fuzz_time-begin_tot_fuzz_time,
                                            straight_findings_cov,
                                            straight_findings_cov_true,
                                            timeouts - begin_timeouts,
                                            solutions_on_timeout - begin_solutions_on_timeout  );


        //
        // Print stats
        //
        //printf( KIMP );
        printf("\n*************************************************************\n");
        printf("SIVO fuzzing: %s\n", argv[found__] );
        printf("*************************************************************\n");
        printf( "Iterations              :         %10d \n" , glob_iter );
        printf( "Time total              :         %10.1f s\n", total_time);
        printf( "Binary  executions      :         %10ld    \n", tot_fork_executions );
        printf("\n");
        
        printf( "Seeds                   :            tot      edge-increasing  \n" );
        for( auto it=solution_types.begin(); it!=solution_types.end(); it++){
            printf( "\t %16s          %4d        ", get_solution_name(it->first).c_str(), it->second );
            auto zz = solution_first.find( it->first );
            if( zz != solution_first.end() ){
                printf("%4d ", zz->second);
            }
            printf("\n");
        }
        printf("\n");
        uint32_t cttc[2];
        count_true_coverage( cttc );
        printf( KGRN );
        printf( "Coverage #edges/#logcnt :         %10d %10d \n", cttc[0], cttc[1] );
        printf( "Crashes uniq/tot        :         %10d %10d \n", crash_uniq, crash_id  );        
        printf( KNRM );
        printf( "Timeouts conf/tot       :         %10d %10ld \n", hang_id, timeouts  );

        printf("\n");
        printf("*************************************************************\n");

        
    }


    printf("Fuzzer finished\n"); 


    fini_coverage();
    fini_fork_servers();


    return 0;
}
