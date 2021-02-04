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

#include "solutions.h"
#include "files_etc.h"
#include "executions.h"
#include "fork_servers.h"
#include "minimizer.h"
#include "testcases.h"


//
//
// Solutions are actually seeds. This keeps track of correctly naming them, 
// i.e. assigning the right ID, and naming the seed files according to the 
// fuzzer that was used to find them. Processing seeds that lead to 
// timeouts and crashes is also partially defined here. 
//
// 

int solution_id=0;
set <uint32_t> crash_line;
int hang_id = 0;
int solutions_on_timeout = 0;
double tot_sol_add_time = 0;
map < int , int > solution_types;
map < int , set<uint32_t> > solution_types_iter;
map < int , int > solution_first;
map < pair<int, int>,int> true_coverage; 

extern uint32_t *branches_instrument;
extern uint32_t *__new_branch_index;


string create_id(string temp_input, bool external_tc, int sol_type, string add_info )
{

    
    if ( ! file_exists(temp_input) ){
        printf( KERR "(create_id)Input file does not exist: %s\n" KNRM , temp_input.c_str() );
        return "";
    }
    string solution_folder = OUTPUT_FOLDER+'/'+FUZZER_OUTPUT_FOLDER_NAME+"/queue/";



    // true covearge 
    float ACTUAL_TIMEOUT_EXECUTION = TIMEOUT_EXECUTION;
    TIMEOUT_EXECUTION = MAX_EXTERNAL_TC_TIMEOUT_EXEC;
    int good_run_this = run_one_fork( RUN_GETTER );
    TIMEOUT_EXECUTION = ACTUAL_TIMEOUT_EXECUTION;
    bool real_update = false;
    int found_new = false;
    if( good_run_this ){
            
        map < pair<int, int>,int> counters; 
        for( int i=1; i<__new_branch_index[0]; i++ ){
        
            pair<int, int> p = make_pair( branches_trace[i-1], branches_trace[i] );

            auto zz = counters.find(p); 
            if( zz == counters.end() ) 
                counters.insert( { p , 1 } );
            else
                zz->second ++;
        }    

        for( auto zz=counters.begin(); zz!= counters.end(); zz++){

            auto edge           = zz->first;         // edge
            auto count          = zz->second;    // count
            int compressed_bit  = count >=256 ? 128 : ectb[count];
            
            auto it = true_coverage.find( edge );
            if( it == true_coverage.end() ){
                found_new = real_update = true;
                true_coverage.insert( {edge, 0} );
                it = true_coverage.find( edge );
            }
            real_update = real_update || (it->second | compressed_bit) != it->second; 
            it->second |= compressed_bit;
        }

        if( !real_update && 
            !external_tc && 
            (2!=glob_found_different) && 
            !RE_INIT_USE
        ){
                printf( KINFO "Not a real update: %d %ld \n" KNRM, 
                    __new_branch_index[0], counters.size()
                );
                return "";
        }
    }


    char reinit_info[1024];
    if( RE_INIT_USE ) sprintf(reinit_info, "reinit-%02d-", RE_CURR_REPEATS );
    else sprintf(reinit_info,"%s","");


    char cdest_file[1024];
    if (external_tc ){
        // if external, it must increase the coverage to be accepted
        // Give more time to external
        prepare_tmp_input(temp_input);
        float ACTUAL_TIMEOUT_EXECUTION = TIMEOUT_EXECUTION;
        TIMEOUT_EXECUTION = MAX_EXTERNAL_TC_TIMEOUT_EXEC;
        int good_run = run_one_fork( RUN_TRACER );
        TIMEOUT_EXECUTION = ACTUAL_TIMEOUT_EXECUTION;
        if( !good_run || !execution_edge_coverage_update() ){
            printf( "Cannot properly execute (%d) or file %s does not increase coverage\n", good_run, temp_input.c_str() );
            return "";
        }

        sprintf(cdest_file,"%s%sid:%06d,src:external%s",solution_folder.c_str(),reinit_info, solution_id, 
                (2==glob_found_different) ? "," SUFFIX_COV:"");
    }
    else
        sprintf(cdest_file,"%s%sid:%06d,src:%06d,type:%s,fuzz:%s%s%s",
                solution_folder.c_str(),
                reinit_info,
                solution_id, 
                current_testcase->my_id, 
                fuzzer_type ? "dataflow" : "vanillla",
                get_solution_name(sol_type).c_str(),
                add_info.c_str(),
                (2==glob_found_different) ? "," SUFFIX_COV:""
                 );
    string dest_file = string(cdest_file);
    if( PRINT_FOUND_TESTCASES ){
        printf( KGRN "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
        printf( "New testcase %s\n",  dest_file.c_str() );
        printf( "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n" KNRM);
    }

    interim_testcase ite;
    ite.depth           =  (current_testcase != NULL) ? (current_testcase->depth + 1) : 0;
    ite.is_new_cover    =  2==glob_found_different;
    ite.my_id           =  solution_id;
    itest.insert( {dest_file, ite});

    copy_binary_file( temp_input, dest_file );

    solution_id ++;


    if( good_run_this && 
        2==glob_found_different && 
        !RE_INIT_USE && 
        !found_new && 
        (__new_branch_index[0] < MAX_BRANCHES_INSTRUMENTED - 1)
    ){
        printf( KINFO "No real new coverage found: %d %d %d : %s \n" KNRM, 
            glob_found_different, RE_INIT_USE, found_new,  
            cdest_file );

        printf("#edges: %ld\n", true_coverage.size() );
        int noz =0;
        for( int j=0; j<1<<16; j++)
            noz += showmap[j] > 0 ;
        printf("#afledges: %d\n", noz );

    }



    // keep the full (un-minimized file) as well
    /*
    string dest_file_full=dest_file; 
    string squ  = "/queue/", nsqu = "/queue_full/";
    size_t pos = dest_file_full.find(squ);
    if( pos >= 0){
        dest_file_full.replace(pos, squ.length(), nsqu);
        //simple_exec("cp "+temp_input + " "+dest_file_full);
        copy_binary_file( temp_input, dest_file_full );

    }
    */


    if( !external_tc)
        minimize_testcase(dest_file);

    
    return dest_file;
}


void count_true_coverage( uint32_t cttc[2] )
{
    cttc[0] = true_coverage.size();
    cttc[1] = 0;
    for( auto it= true_coverage.begin(); it!= true_coverage.end(); it++)
    {
        for( int i=0; i<8; i++)
            cttc[1] += (it->second >> i) & 1; 
    }
}



void solution_add_found_types( int CURRENT_SOLUTION_TYPE  )
{
    // solution type (for logging purposes only)
    if( solution_types.find( CURRENT_SOLUTION_TYPE ) == solution_types.end() ) 
        solution_types.insert( {CURRENT_SOLUTION_TYPE,0});
    solution_types.find( CURRENT_SOLUTION_TYPE )->second ++;

    // solution type (with iteration number)
    if( solution_types_iter.find( CURRENT_SOLUTION_TYPE ) == solution_types_iter.end() ) 
        solution_types_iter.insert( {CURRENT_SOLUTION_TYPE , set<uint32_t>()} );
    solution_types_iter.find( CURRENT_SOLUTION_TYPE )->second.insert(glob_iter);

    // which type results in completely new edges
    if( solution_first.find( CURRENT_SOLUTION_TYPE ) == solution_first.end() ) 
        solution_first.insert( {CURRENT_SOLUTION_TYPE , 0} );
    if( 2 == glob_found_different)
    {
        solution_first.find( CURRENT_SOLUTION_TYPE )->second ++;
    }

}

void check_and_add_solution( string tmp_input_file, int CURRENT_SOLUTION_TYPE, string add_info )
{

    if( last_crashed ) return;

    if( last_is_timeout_max ) 
        solutions_on_timeout ++;


    START_TIMER(start_add_sol_time);

    //# create a new testcase file and place it in the queue folder
    string dest_file = create_id( tmp_input_file, false , CURRENT_SOLUTION_TYPE, add_info );

    if (dest_file.size() != 0){

        // add type
        solution_add_found_types( CURRENT_SOLUTION_TYPE );

        // add to the queue of testcases
        get_testcases_from_folder(OUTPUT_FOLDER+"/"+FUZZER_OUTPUT_FOLDER_NAME +"/queue", false);

    }

    tot_sol_add_time += TIME_DIFFERENCE_SECONDS( start_add_sol_time );


    return ;

}


void create_lookup_for_sol_id_to_string()
{

    sol_type_id_to_str.insert( { SOL_TYPE_IMPORTED ,        "imported" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_BRANCH_GA ,       "branch-ga" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_SOLVER_A ,        "solverA" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_SOLVER_B ,        "solverB" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_BRANCH_PRIMARY ,  "branch-primary" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_BRANCH_SECOND ,   "branch-second" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_MINGLER       ,   "mingler" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_MINGLER_DUMB ,    "minglerD" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_MINGLER_SMART ,   "minglerS" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_MUTATE_KNOWN ,    "mutate-known" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_MUTATE_RAND ,     "mutate-rand-ga" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_MUTATE_RAND1 ,    "mutate-rand-1" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_COPREM_RAND ,     "copyrem-rand" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_COPREM_REAL ,     "copyrem-real" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_COPREM_RE_DIVS ,  "copyrem-learn" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_COPREM_RE_PREV ,  "copyrem-prev" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_COMBINER  ,       "combiner" } ) ;
    sol_type_id_to_str.insert( { SOL_TYPE_COMBINERSMART  ,  "combiner-learn" } ) ;


    
}

string get_solution_name( int stype  )
{
    auto it = sol_type_id_to_str.find( stype );
    if( it == sol_type_id_to_str.end() ) return "unknown"+to_string(stype);

    return it->second;
}

uint32_t count_current_important_tc()
{
    return solution_id + crash_uniq;
}

void process_crash( int status )
{
    crash_id++;

    uint32_t last_bb = 0;


    //if( STORE_CRASHES && execution_showmap_crashes_new() )
    if( STORE_CRASHES )
    {
        if( 0 ==  __new_branch_index[0] ){
            processing_crash = true;
            run_one_fork( RUN_GETTER );
            processing_crash == false;
        }

        if(  __new_branch_index[0] > 0  ){
            last_bb = branches_trace[ __new_branch_index[0] - 1 ];
            if( crash_line.find( last_bb  ) == crash_line.end() ){

                crash_uniq++;
                crash_line.insert( last_bb  );
                
                // add type
                solution_add_found_types( fuzzer_current_main );


                string crashes_folder   = OUTPUT_FOLDER+"/"+FUZZER_OUTPUT_FOLDER_NAME+"/crashes/";
                string fuz_type         = get_solution_name(fuzzer_current_main);
                char ccrash_file[1024];
                sprintf(ccrash_file, "%scrash-uid:%06d,totnum:%06d,status:%02d,bb:%08x,type:%s,fuzz:%s,src:%06d",
                        crashes_folder.c_str(), crash_uniq, crash_id, status, last_bb, 
                        fuzzer_type ? "dataflow" : "vanillla", fuz_type.c_str(),
                        current_testcase->my_id );
                string crash_file = string( ccrash_file );

                copy_binary_file( TMP_INPUT, crash_file );

                current_testcase->found_crash = true;
            }
        }

    }

    
}


void store_timeout_testcase()
{

    hang_id++;

    if(!STORE_TIMEOUTS ) return;

    string hangs_folder     = OUTPUT_FOLDER+"/"+FUZZER_OUTPUT_FOLDER_NAME+"/hangs/";
    string fuz_type         = get_solution_name(fuzzer_current_main);

    char chang_file[1024];
    sprintf(chang_file, "%shang-id:%06d,type:%s,fuzz:%s",
            hangs_folder.c_str(), hang_id, 
            fuzzer_type ? "dataflow" : "vanillla",
            fuz_type.c_str() );
    string hang_file = string( chang_file );

    copy_binary_file( TMP_INPUT, hang_file );

}


void process_timeout(   int type_of_run  ,
                        uint64_t &res_runs, uint64_t &res_timo, 
                        int QTS, uint8_t queue_timeouts[], 
                        uint32_t &times_successfull_used_temp_timeout, uint32_t &times_used_temp_timeout   )
{

    // if increases coverage then assign MAX_TIMEOUT_EXECUTION and re-run
    if( STORE_TIMEOUTS && execution_showmap_timeout_new() ){
        
        float ACTUAL_TIMEOUT_EXECUTION = TIMEOUT_EXECUTION;
        
        TIMEOUT_EXECUTION = MAX_TIMEOUT_EXECUTION;

        processing_timeout_max = true;
        run_one_fork(  type_of_run  );
        processing_timeout_max = false;

        TIMEOUT_EXECUTION = ACTUAL_TIMEOUT_EXECUTION;

        return;
    }


    // ONLY if the final run among the last FACTOR_INCREASE_TIMEOUT runs timeout
    // then repeat the execution with timeout increased by a factor of FACTOR_INCREASE_TIMEOUT           
    queue_timeouts[ res_runs % int(FACTOR_INCREASE_TIMEOUT) ] = 1;
    int s = 0;
    for( int i=0; i< int(FACTOR_INCREASE_TIMEOUT) ; i++) s += queue_timeouts[i];
    if( s <= 1 ){

        // If certain percentage of runs with already increased execution time (by FACTOR_INCREASE_TIMEOUT) 
        // still timeouts, then increase the FACTOR_INCREASE_TIMEOUT 
        float prz = times_used_temp_timeout > 0 ? 100.0 * times_successfull_used_temp_timeout /times_used_temp_timeout : 0.0;
        if( times_used_temp_timeout > 5 && 
            prz < 50.0 && 
            int(FACTOR_INCREASE_TIMEOUT) < QTS 
        )
            FACTOR_INCREASE_TIMEOUT = mmin( QTS, 1.5 * FACTOR_INCREASE_TIMEOUT);

        printf( KINFO "Repeating execution with larger timeout: %5.1f :  %5d  %5d  :   %.1f  : %.3f x %.1f ->  %.3f  \n" KNRM,
            prz,
            times_successfull_used_temp_timeout, times_used_temp_timeout,
            100.0 * res_timo/res_runs, 
            TIMEOUT_EXECUTION, FACTOR_INCREASE_TIMEOUT, mmin( MAX_TIMEOUT_EXECUTION, TIMEOUT_EXECUTION * FACTOR_INCREASE_TIMEOUT ) );  
        fflush(stdout);

        processing_temp_timeout = true;
        times_used_temp_timeout ++;
        run_one_fork(  type_of_run  );
    }


}