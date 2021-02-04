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


#include "testcases.h"
#include "misc.h"
#include "files_etc.h"
#include "branches.h"
#include <set>
#include <map>
#include <fstream>
#include <string.h>
#include "fork_servers.h"
#include "executions.h"
#include "nodes.h"
#include "multi_arm_banditos.h"
#include "solutions.h"
#include "fuzzer.h"


//
//
// Selection and processing of seeds
//
//


map < uint64_t , testcase_file * > active_testcase_hashes;
map < string , testcase_file * > *current_testcases;
map < string , testcase_file * > all_testcases;
map < string , testcase_file * > allowed_testcases;
map < string , testcase_file * > min_testcases_cover;
map < string , testcase_file * > min_testcases_fast_cover_only;
map < string , testcase_file * > min_testcases_fast_cover_all;
map < string , testcase_file * > external_testcases;


static vector < node_mab > mab_testcases_vanilla;
static vector < node_mab > mab_testcases_dataflow;
static vector < node_mab > mab_testcases_vanilla_cov;
static vector < node_mab > mab_testcases_dataflow_cov;


int tc_class;
vector < node_mab > mab_tc_class;
vector < node_mab > mab_tc_class_cov;

static map <int, string > tc_sampling_name;

static bool VANILLA_FUZZER;

static float average_sampled = 0;
static float mininum_sampled = 0;

Time last_min_tc;
Time last_time_redo_index;

extern double total_time;

map < int, pair< testcase_file * ,float >  > shtc_cover_only, shtc_cover_all;
static bool change_in_tc_according_to_cover_only = false;
static bool change_in_tc_according_to_cover_all  = false;

map < int, pair< testcase_file * ,float >  > reinit_shtc_cover_only, reinit_shtc_cover_all;
map < string , testcase_file * > reinit_crashed;

map < string , testcase_file * > prev_reinit_testcases;
map < string , testcase_file * > temp_testcases;

void minimize_tc_according_to_branch(testcase_file *tc );


// 
// Add seed file to the pool (by producing all the necessary info about the seed)
// 
bool add_testcase(string filename)
{

    if ( all_testcases.find(filename) != all_testcases.end() ){
        return false;
    }

    if ( !file_exists ( filename ) ){
        return false;
    }

    uint64_t testcase_hash = hash_file(filename);
    auto it = active_testcase_hashes.find( testcase_hash ); 
    if ( it != active_testcase_hashes.end() ){
        return  false;
    }

    string filename_only =  get_only_filename_from_filepath( filename ); 
    if( filename_only.size() == 0){
        return false;
    }   

    if(PRINT_TESTCASES_INFO)
        cout <<filename<<endl;


    // Get the path and branches
    prepare_tmp_input( filename );

    int no_input_bytes = file_size(filename.c_str() );
    if( PRINT_ADDED_TESTCASES )
        printf( "[ ] Adding testcase:%s:%d \n", filename.c_str(),no_input_bytes);

    string coverage_edges_filename = FOLDER_AUX+"/edgecoverage_"+filename_only;
    string coverage_nodes_filename = FOLDER_AUX+"/nodecoverage_"+filename_only;
    string coverage_afl_filename   = FOLDER_AUX+"/aflcoverage_"+filename_only;
    uint32_t no_ifs_switches = 0;
    uint32_t trace_length   = 0;

    execution_all_coverages_produce_and_save_to_file( coverage_edges_filename, coverage_nodes_filename, coverage_afl_filename, no_ifs_switches, trace_length );
    execution_edge_coverage_update();
    uint64_t tc_trace_hash = hash_showmap;
    fill_node_coverage_counts_with_inserts( coverage_nodes_filename );
    fill_nodes_from_edgecoverage_file( coverage_edges_filename );

    auto iz = itest.find( filename );
    int depth = 0, path_length = 0, is_new_cover = 0, my_id=-1;
    if( iz != itest.end() ){
        depth           = iz->second.depth;
        path_length     = iz->second.path_length;
        is_new_cover    = iz->second.is_new_cover;
        my_id           = iz->second.my_id;

        itest.erase( iz ) ;
    }


    testcase_file *tc            = new testcase_file;
    tc->valid                    = true;
    tc->testcase_filename        = filename;
    tc->testcase_hash            = testcase_hash;
    tc->no_input_bytes           = no_input_bytes;
    tc->times_sampled_dataflow      = 0;
    tc->times_sampled_vanilla       = 0;
    tc->last_iter_sampled        = 0;
    tc->missing_solutions        = 0;
    tc->missing_solutions_next   = 0;
    tc->number_of_branches       = no_ifs_switches;
    tc->my_id                    = my_id;
    tc->coverage_score           = -1;
    tc->coverage_edges_filename  = coverage_edges_filename;
    tc->coverage_nodes_filename  = coverage_nodes_filename;
    tc->coverage_afl_filename    = coverage_afl_filename;
    tc->tot_execs                = 0;
    tc->tot_time                 = 0;
    tc->found_tc                 = 0;
    tc->found_tc_cov             = 0;
    tc->timetouts                = 0;
    tc->timetout_tc              = 0;
    tc->trace_length             = trace_length;
    tc->trace_hash               = tc_trace_hash;
    tc->depth                    = depth;
    tc->path_length              = path_length;
    tc->found_crash              = false;
    tc->is_new_cover             = is_new_cover;

    
    active_testcase_hashes.insert( {testcase_hash, tc});

    all_testcases.insert(           { filename , tc } );
    current_testcases->insert(      { filename , tc } );
    allowed_testcases.insert(       { filename , tc } );

    if( filename.find("external") != string::npos){
        external_testcases.insert( { filename , tc } );
    }

    minimize_tc_according_to_branch( tc );

    return true;
}



// 
// process all seed candidates, i.e. add the files to the pool
// 
void update_testcases_candidates()
{
    retrieve_testcase_files_from_afl_instances();

    // remove the ones that have the already seen hash / filename (for some weird reason) as the one before
    string onefile;
    int tot_proced = 0;
    while( (onefile=get_untested_file()).length() > 0){

        add_testcase(onefile);

        if( ++tot_proced > MAX_PROCESS_TESTCASES_PER_RUN )
            break;
    }
}





//
//
// Stats about the seeds
// 
//


void set_stats_sampled(bool DF)
{
    average_sampled = 0;
    float s = 0, t = 0;
    for( auto it=current_testcases->begin(); it!=current_testcases->end(); it++)
    {
        s += DF ? it->second->times_sampled_vanilla : it->second->times_sampled_dataflow;
        t += 1;
    }
    average_sampled = (t > 0) ? (s/ t) : 0;

    mininum_sampled = DF ? testcase_times_passed_vanilla(true) : testcase_times_passed_dataflow(true);

}



int testcase_get_passed_dataflow(bool once)
{
    int tot_times_sampled = 0;
    for( auto it= current_testcases->begin(); it!=current_testcases->end(); it++)
        tot_times_sampled += once ? (it->second->times_sampled_dataflow>0) : (it->second->times_sampled_dataflow == 0);
    return tot_times_sampled;
}

int testcase_get_passed_vanilla(bool once)
{
    int tot_times_sampled_vanilla = 0;
    for( auto it= current_testcases->begin(); it!=current_testcases->end(); it++)
        tot_times_sampled_vanilla += once ? (it->second->times_sampled_vanilla>0) : (it->second->times_sampled_vanilla == 0);
    return tot_times_sampled_vanilla;
}

int testcase_get_passed_any(bool once)
{
    int tot_times_sampled_any = 0;
    for( auto it= current_testcases->begin(); it!=current_testcases->end(); it++)
        tot_times_sampled_any += once ? 
                                (it->second->times_sampled_vanilla>0 || it->second->times_sampled_dataflow>0) : 
                                (it->second->times_sampled_vanilla == 0 && it->second->times_sampled_dataflow == 0 );
    return tot_times_sampled_any;
}


int testcase_times_passed_dataflow(bool is_it_min)
{
    int times_sampled = -1;
    for( auto it= current_testcases->begin(); it!=current_testcases->end(); it++)
        if( (is_it_min && it->second->times_sampled_dataflow < times_sampled || times_sampled < 0) ||  
            (!is_it_min && it->second->times_sampled_dataflow > times_sampled ) 
        )
          times_sampled = it->second->times_sampled_dataflow;
    return times_sampled;
}

int testcase_times_passed_vanilla(bool is_it_min)
{
    int times_sampled_vanilla = -1;
    for( auto it= current_testcases->begin(); it!=current_testcases->end(); it++)
        if( (is_it_min && it->second->times_sampled_vanilla < times_sampled_vanilla || times_sampled_vanilla < 0) ||  
            (!is_it_min && it->second->times_sampled_vanilla > times_sampled_vanilla ) 
        )
          times_sampled_vanilla = it->second->times_sampled_vanilla;
    return times_sampled_vanilla;
}





// make sure that the seeds are not oversampled
bool is_oversampled(testcase_file *tf, bool DF)
{
    
    int sampl = DF ? tf->times_sampled_vanilla : tf->times_sampled_dataflow;
    bool average_bad = sampl  > 10 * average_sampled;
    bool minimum_bad = sampl  > 10 && 10 * mininum_sampled < sampl;

    return average_bad || minimum_bad;
}





//
//
// can_sample_XXX show which seed criteria are allowed (according to the chosen seed class)
//
//



bool can_sample_testcase_random()
{
    return true;
}

bool can_sample_testcase_count()
{
    for( auto it=current_testcases->begin(); it!=current_testcases->end(); it++)
        if( (it->second)->last_iter_sampled + TC_SAMPLE_WAIT_ITER_BEFORE_RESAMPLE_COUNT < glob_iter )
            return true;

    return false;
}


bool can_sample_testcase_success( bool DF )
{
    for( auto it=current_testcases->begin(); it!= current_testcases->end(); it++){  

        if( is_oversampled( it->second, DF )) continue;

        if( (0==coverage_type ? it->second->found_tc : it->second->found_tc_cov) > 0 )
            return true;
    }
    return false;

}

bool can_sample_testcase_length( bool DF  )
{
    for( auto it=current_testcases->begin(); it!= current_testcases->end(); it++){   

        if( is_oversampled( it->second, DF )) continue;

        return true;
    }

    return false;
}



bool can_sample_testcase_new(bool DF )
{
    for( auto it=current_testcases->begin(); it!= current_testcases->end(); it++){   

        if( is_oversampled( it->second, DF )) continue;

        if( it->second->is_new_cover )
            return true;
    }

    return false;
}


bool can_sample_testcase_crash( bool DF )
{
    for( auto it=current_testcases->begin(); it!= current_testcases->end(); it++){

        if( is_oversampled( it->second, DF )) continue;

        if( it->second->found_crash) return  true;
    }
    return false;
}



//
//
// sample_testcase_XXX sample one seed according to the chosen class and the XXX criterion 
//
//

testcase_file *sample_testcase_random( )
{
    vector <string> candidates;
    for( auto it=current_testcases->rbegin(); it!=current_testcases->rend(); it++)
        candidates.push_back( it->first);

    auto one_cand = candidates[ mtr.randInt() % candidates.size() ];
    auto fi = current_testcases->find( one_cand );

    return fi->second;
}


testcase_file *sample_testcase_count(bool DF)
{
    int seen_least = -1;
    vector <string> candidates;
    for( auto it=current_testcases->begin(); it!=current_testcases->end(); it++)
    {
        if( (it->second)->last_iter_sampled + TC_SAMPLE_WAIT_ITER_BEFORE_RESAMPLE_COUNT < glob_iter ){

            if( seen_least < 0 || 
                ( DF ? it->second->times_sampled_vanilla : it->second->times_sampled_dataflow )   <= seen_least  
            ){
                if( ( DF ? it->second->times_sampled_vanilla : it->second->times_sampled_dataflow )   < seen_least )
                        candidates.clear();
                seen_least = DF ? it->second->times_sampled_vanilla : it->second->times_sampled_dataflow ; 
                candidates.push_back( it->first  );
            }
        }
    }

    if( seen_least < 0 || 0 == candidates.size() ){
        printf( KRED "Impossible seen_last = %d. How come  can_sample_testcasecount did not fail ? \n" KNRM, seen_least);
        printf( KRED "Impossible cand size is %ld !\n" KNRM,  candidates.size() );
        exit(0);
    }

    auto one_cand = candidates[ mtr.randInt() % candidates.size() ];
    auto fi = current_testcases->find( one_cand );

    return fi->second;
}



testcase_file *sample_testcase_success( bool DF )
{
    vector <float> v;
    vector <string >fnames;
    vector <float> sampl;
    float maxv = 0;
    for( auto it=current_testcases->begin(); it!= current_testcases->end(); it++){

        if( is_oversampled( it->second, DF )) continue;
                
        if( ( (0==coverage_type) ? it->second->found_tc : it->second->found_tc_cov) > 0 )
        {
            fnames.push_back( it->first );
            v.push_back( ( (0==coverage_type) ? it->second->found_tc : it->second->found_tc_cov)/ it->second->tot_time );
            maxv = mmax( maxv, v[ v.size() -1 ] );
            sampl.push_back ( DF ? it->second->times_sampled_vanilla : it->second->times_sampled_dataflow ) ;
        }
    }
    if( 0 == v.size() ){
        printf( KRED "Impossible candidate size = 0. How come  can_sample_testcase_success did not fail ? \n" KNRM);
        exit(0);
    }
    if( maxv > 0)
        for( int i=0; i<v.size(); i++)
            v[i] = pow( 2, 30 * v[i]/maxv ) / pow( 2, sampl[i] ) ;

    int ind = discrete_distribution_sample( v );
    string best_candidate = fnames[ind];

    auto it= current_testcases->find( best_candidate ); 
    return it->second;
}


testcase_file *sample_testcase_length( bool DF )
{
    vector <float> v;
    vector <string >fnames;
    vector <float> sampl;
    float maxv = 0;
    for( auto it=current_testcases->begin(); it!= current_testcases->end(); it++){

        if( is_oversampled( it->second, DF )) continue;

        fnames.push_back( it->first );
        v.push_back( 1 + it->second->no_input_bytes );
        maxv = mmax( maxv, v[ v.size() -1 ] );
        sampl.push_back ( DF ? it->second->times_sampled_vanilla : it->second->times_sampled_dataflow ) ;
    }
    if( 0 == v.size() ){
        printf( KRED "Impossible candidate size = 0. How come  can_sample_testcase_length did not fail ? \n" KNRM);
        exit(0);
    }
    if( maxv > 0)
        for( int i=0; i<v.size(); i++)
            v[i] = pow( 2, 30 * v[i]/maxv ) / pow( 2, sampl[i] ) ;


    int ind = discrete_distribution_sample( v );
    string best_candidate = fnames[ind];

    auto it= current_testcases->find( best_candidate ); 
    return it->second;
}



testcase_file *sample_testcase_new( bool DF )
{
    vector <float> v;
    vector <string >fnames;
    
    for( auto it=current_testcases->begin(); it!= current_testcases->end(); it++){

        if( is_oversampled( it->second, DF )) continue;

        if( it->first.find(SUFFIX_COV)  == string::npos ) continue;

        v.push_back( 1.0 / pow( 4, DF ? it->second->times_sampled_vanilla : it->second->times_sampled_dataflow  ) );
        fnames.push_back( it->first );
    }
    if( 0 == v.size() ){
        printf( KRED "Impossible candidate size = 0. How come  can_sample_testcase_new did not fail ? \n" KNRM);
        exit(0);
    }

    int ind = discrete_distribution_sample( v );
    string best_candidate = fnames[ind];

    auto it= current_testcases->find( best_candidate ); 
    return it->second;
}



testcase_file *sample_testcase_crash( bool DF )
{
    vector <float> v;
    vector <string >fnames;
    vector <float> sampl;
    float maxv = 0;
    for( auto it=current_testcases->begin(); it!= current_testcases->end(); it++){

        if( is_oversampled( it->second, DF )) continue;

        if ( ! it->second->found_crash ) continue;

        fnames.push_back( it->first );
        v.push_back( 1.0 );
        maxv = mmax( maxv, v[ v.size() -1 ] );
        sampl.push_back ( DF ? it->second->times_sampled_vanilla : it->second->times_sampled_dataflow ) ;
    }
    if( 0 == v.size() ){
        printf( KRED "Impossible candidate size = 0. How come  can_sample_testcase_crash did not fail ? \n" KNRM);
        exit(0);
    }
    if( maxv > 0)
        for( int i=0; i<v.size(); i++)
            v[i] /= 1.0 + sampl[i]  ;

    int ind = discrete_distribution_sample( v );
    string best_candidate = fnames[ind];

    auto it= current_testcases->find( best_candidate ); 
    return it->second;
}



testcase_file *full_get_testcase(int testcase_type)
{

    testcase_file *one; 
    chosen_testcase_method = -1;

    // if there are no testcases, then something is wrong, return invalid dummy
    if (allowed_testcases.size() == 0){
        one = new testcase_file;
        one->valid = false; 
        return one;
    }

    VANILLA_FUZZER = 0 == testcase_type;

    set_stats_sampled( VANILLA_FUZZER );


    vector <bool> multip;
    for( int i=0; i<mab_testcases_vanilla.size(); i++)
        multip.push_back( true );

    multip[0] = multip[0] &&  can_sample_testcase_count         ();
    multip[1] = multip[1] &&  can_sample_testcase_success       ( VANILLA_FUZZER );
    multip[2] = multip[2] &&  can_sample_testcase_length        ( VANILLA_FUZZER );
    multip[3] = multip[3] &&  can_sample_testcase_new           ( VANILLA_FUZZER );
    multip[4] = multip[4] &&  can_sample_testcase_crash         ( VANILLA_FUZZER );
    multip[5] = multip[5] &&  can_sample_testcase_random        ();


    mab_recompute( mab_testcases_vanilla );
    mab_recompute( mab_testcases_dataflow );
    mab_recompute( mab_testcases_vanilla_cov );
    mab_recompute( mab_testcases_dataflow_cov );


    chosen_testcase_method = mab_sample( VANILLA_FUZZER ? 
                                        ( (0 == coverage_type) ? mab_testcases_vanilla  : mab_testcases_vanilla_cov) : 
                                        ( (0 == coverage_type) ? mab_testcases_dataflow : mab_testcases_dataflow_cov), 
                                        mab_testcases_vanilla.size(), 
                                        false, 
                                        multip );


    if (      0 == chosen_testcase_method ){
        used_sampling_method = VANILLA_FUZZER ? SAMPLING_METHOD_COUNT_VANILLA : SAMPLING_METHOD_COUNT_DATAFLOW;
        one         = sample_testcase_count( VANILLA_FUZZER );
    }
    else if ( 1 == chosen_testcase_method ){
        used_sampling_method = VANILLA_FUZZER ? SAMPLING_METHOD_SUCCESS_VANILLA : SAMPLING_METHOD_SUCCESS_DATAFLOW;
        one         = sample_testcase_success( VANILLA_FUZZER );
    }
    else if ( 2 == chosen_testcase_method ){
        used_sampling_method = VANILLA_FUZZER ? SAMPLING_METHOD_LENGTH_VANILLA : SAMPLING_METHOD_LENGTH_DATAFLOW;
        one         = sample_testcase_length( VANILLA_FUZZER );
    }
    else if ( 3 == chosen_testcase_method ){
        used_sampling_method = VANILLA_FUZZER ? SAMPLING_METHOD_NEW_VANILLA : SAMPLING_METHOD_NEW_DATAFLOW ;
        one         = sample_testcase_new( VANILLA_FUZZER );
    } 
    else if ( 4 == chosen_testcase_method ){
        used_sampling_method = VANILLA_FUZZER ? SAMPLING_METHOD_CRASH_VANILLA : SAMPLING_METHOD_CRASH_DATAFLOW ;
        one         = sample_testcase_crash( VANILLA_FUZZER );
    } 
    else if ( 5 == chosen_testcase_method ){
        used_sampling_method = VANILLA_FUZZER ? SAMPLING_METHOD_RANDOM_VANILLA : SAMPLING_METHOD_RANDOM_DATAFLOW ;
        one         = sample_testcase_random();
    } 



    if (! one->valid ){ 
        printf( KERR "No valid seed candidates\n" KNRM);
        exit(0);
    }

    // increase sampled count    
    if( VANILLA_FUZZER )    one->times_sampled_vanilla  ++;   
    else                    one->times_sampled_dataflow ++;


    if( used_sampling_method >= 0 && used_sampling_method < 20 )
        tc_sampling_used[used_sampling_method] ++;

    prepare_tmp_input( one->testcase_filename );


    printf(      "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf(      "Seed file %s \n",  one->testcase_filename.c_str() ) ;
    printf(      "size %7d : sample_method %15s  : sampled (van/datf) %5d %5d\n" , 
                one->no_input_bytes,  testcase_get_sampling_name(used_sampling_method).c_str(), 
                one->times_sampled_vanilla, one->times_sampled_dataflow );
    printf(     "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n" KNRM);
    fflush(stdout);


    return one;
}






//
// Choose seed class:
//      0) only fastest seeds for each edge
//      1) only fastest seeds for each edge and each logarithmic count
//      2) all seeds that at some moment inceased coverage (edge+count)
// 
void testcases_choose_class()
{
    mab_recompute( mab_tc_class );
    mab_recompute( mab_tc_class_cov );
    vector < bool > use_choices = { true, true, true  };
    tc_class = mab_sample( 0 == coverage_type  ? mab_tc_class : mab_tc_class_cov , 3, false , use_choices  );
    if( tc_class < 0 ) tc_class = 2;

    if( 0 == tc_class ){
        remove_redundant_testcases( min_testcases_fast_cover_only, change_in_tc_according_to_cover_only, shtc_cover_only );
        current_testcases   = &min_testcases_fast_cover_only;
    }
    else if( 1 == tc_class ){
        remove_redundant_testcases( min_testcases_fast_cover_all,  change_in_tc_according_to_cover_all,  shtc_cover_all );
        current_testcases   = &min_testcases_fast_cover_all;
    }
    else{
        current_testcases   = &allowed_testcases;
    }


    //
    // If reinitalization or small class sizes, then fallback to all seeds        
    if( RE_INIT_USE )
        tc_class = 2;

    if      ( 0 == tc_class && min_testcases_fast_cover_only.size() <= 10 ){
        tc_class            = 2;
        current_testcases   = &allowed_testcases;
    }
    else if ( 1 == tc_class && min_testcases_fast_cover_all.size() <= 10 ){
        tc_class            = 2;
        current_testcases   = &allowed_testcases;
    }

}


//
//
// Seed scores (with testcases_get_num_of_stats different criteria) 
// used in the combiner fuzzer to decide which to combine
//
//

uint32_t testcases_get_num_of_stats()
{
    return 6;
}

void testcase_get_testcases_( vector < pair < string, vector<float> > > &tcc )
{

    map < string , testcase_file * > *use_testcases = &allowed_testcases;

    tcc.clear();
    for( auto it= use_testcases->begin(); it!=use_testcases->end(); it++){
        vector <float> scores;
    
        scores.push_back( 1.0 ) ;
        scores.push_back( (it->second->tot_execs > 0 ) ?  (it->second->tot_execs / it->second->tot_time) : 1.0 ); 
        scores.push_back( 1.0 / ( 1 + it->second->no_input_bytes) ) ;
        scores.push_back( 1.0 / ( 1 + it->second->number_of_branches) ) ; 
        scores.push_back( it->second->no_input_bytes ) ;
        scores.push_back( it->second->number_of_branches ) ;

        tcc.push_back( { it->first, scores } );
    }
}



int get_no_testcases()
{
    return allowed_testcases.size();
}


string testcase_get_filename(string testcase)
{
    auto it = current_testcases->find( testcase );
    if( it == current_testcases->end() ) return "";

    return it->second->testcase_filename;
}



void init_testcase_stuff()
{

    // Random, Count, score, time, length, success
    for( int i=0; i<6; i++){
        mab_testcases_vanilla.push_back        ( mab_init(  ) );
        mab_testcases_dataflow.push_back       ( mab_init(  ) );
        mab_testcases_vanilla_cov.push_back    ( mab_init(  ) );
        mab_testcases_dataflow_cov.push_back   ( mab_init(  ) );

    }

    mab_discount_add( mab_testcases_vanilla        , &discount_times  );
    mab_discount_add( mab_testcases_dataflow       , &discount_times );
    mab_discount_add( mab_testcases_vanilla_cov    , &discount_times_cov );
    mab_discount_add( mab_testcases_dataflow_cov   , &discount_times_cov );


    tc_sampling_name.insert( {SAMPLING_METHOD_UNSAMPLED,       "Unsampled"});
    tc_sampling_name.insert( {SAMPLING_METHOD_RANDOM_VANILLA,  "Random Vanilla"});
    tc_sampling_name.insert( {SAMPLING_METHOD_RANDOM_DATAFLOW, "Random Dataflow"});
    tc_sampling_name.insert( {SAMPLING_METHOD_COUNT_VANILLA,   "Count Vanilla"});
    tc_sampling_name.insert( {SAMPLING_METHOD_COUNT_DATAFLOW,  "Count Dataflow"});
    tc_sampling_name.insert( {SAMPLING_METHOD_SUCCESS_VANILLA, "Success Vanilla"});
    tc_sampling_name.insert( {SAMPLING_METHOD_SUCCESS_DATAFLOW,"Success Dataflow"});
    tc_sampling_name.insert( {SAMPLING_METHOD_LENGTH_VANILLA,  "Length Vanilla"});
    tc_sampling_name.insert( {SAMPLING_METHOD_LENGTH_DATAFLOW, "Length Dataflow"});
    tc_sampling_name.insert( {SAMPLING_METHOD_NEW_VANILLA,     "New Vanilla"});
    tc_sampling_name.insert( {SAMPLING_METHOD_NEW_DATAFLOW,    "New Dataflow"});
    tc_sampling_name.insert( {SAMPLING_METHOD_CRASH_VANILLA,   "Crash Vanilla"});
    tc_sampling_name.insert( {SAMPLING_METHOD_CRASH_DATAFLOW,  "Crash Smart"});
    

    // timer
    last_min_tc = GET_TIMER();

    for( int i=0; i<3; i++) mab_tc_class.push_back    ( mab_init(  ) );
    for( int i=0; i<3; i++) mab_tc_class_cov.push_back    ( mab_init(  ) );

    mab_discount_add( mab_tc_class        , &discount_times  );
    mab_discount_add( mab_tc_class_cov    , &discount_times  );

    current_testcases   = &allowed_testcases;

    // randomize a bit so not all minimizations (across different binaries) are at the same time
    MIN_TIME_BETWEEN_MINIMIZATION_TC_T = MIN_TIME_BETWEEN_MINIMIZATION_TC + (mtr.randInt() % ( 1 + (int)MIN_TIME_BETWEEN_MINIMIZATION_TC/4) );


    last_time_redo_index = GET_TIMER();
    TIME_BETWEEN_INDEX_CHANGE = MIN_TIME_BETWEEN_INDEX_REDO + (mtr.randInt() % ( 1 + (int)MIN_TIME_BETWEEN_INDEX_REDO/4) );

}

string testcase_get_sampling_name( int meth)
{
    auto it = tc_sampling_name.find( meth );
    if( it != tc_sampling_name.end() )
        return it->second;
    return string("Unknown");
}


//
// MAB updates for seed selections
//  
void testcases_update_sampling_method( float new_findings, float new_findings_cov, double time_spent)
{

    if( chosen_testcase_method < 0 ) return;

    mab_update( VANILLA_FUZZER ? mab_testcases_vanilla  : mab_testcases_dataflow ,
                chosen_testcase_method,
                new_findings, 
                time_spent
            );

    mab_update( VANILLA_FUZZER ? mab_testcases_vanilla_cov  : mab_testcases_dataflow_cov ,
                chosen_testcase_method,
                new_findings_cov, 
                time_spent
            );

    mab_update( mab_tc_class,       tc_class , new_findings,        time_spent );
    mab_update( mab_tc_class_cov,   tc_class , new_findings_cov,    time_spent );

}

void testcases_update_testcase_stats( string tf, uint64_t forkexecs, double ttim, 
            uint32_t found_tc, uint32_t found_tc_cov, 
            uint32_t timeouts, uint32_t tc_on_timeouts  )
{

    auto fi = all_testcases.find( tf );
    if( fi != all_testcases.end() ){
        (fi->second)->tot_execs    += forkexecs;
        (fi->second)->tot_time     += ttim;
        (fi->second)->found_tc     += found_tc;
        (fi->second)->found_tc_cov += found_tc_cov;
        (fi->second)->timetouts    += timeouts;
        (fi->second)->timetout_tc  += tc_on_timeouts;
    }

}


//
//
// Find fastest seeds for each edge and for each edge+logarithmic count
//
//


#define BIT(x,b) ( ( (x)>>(b)) & 1)

void minimize_tc_according_to_branch(testcase_file *tc )
{
    for( int i=0; i< 1<<16; i++){
        if( !aflshowmap[i] ) continue;

        // coverage only
        auto it = shtc_cover_only.find( i ); 
        if( it == shtc_cover_only.end() ){
            change_in_tc_according_to_cover_only = true;
            shtc_cover_only.insert( { i, { tc , tc->number_of_branches } } );
        }
        else{
            if( it->second.second > tc->number_of_branches  ){
                change_in_tc_according_to_cover_only = true;
                it->second = make_pair( tc , tc->number_of_branches  );
            }
        }

        // all
        for( int j=0; j<8; j++){
            if( !BIT(aflshowmap[i],j) ) continue;

            auto it = shtc_cover_all.find( 8*i + j); 
            if( it == shtc_cover_all.end() ){
                change_in_tc_according_to_cover_all = true;
                shtc_cover_all.insert( { 8*i + j, { tc, tc->number_of_branches } } );
            }
            else{
                if( it->second.second > tc->number_of_branches ){
                    change_in_tc_according_to_cover_all = true;
                    it->second = make_pair( tc, tc->number_of_branches  );
                }
            }
        }
    }
}



void remove_redundant_testcases(    map < string , testcase_file * > &min_testcases, 
                                    bool &has_changed, 
                                    map < int, pair< testcase_file * , float >  > &shtc )
{

    if( !has_changed ) return;

    uint32_t begin_active_size = min_testcases.size();

    min_testcases.clear();
    for(auto it=shtc.begin(); it != shtc.end(); it++){
        min_testcases.insert( {it->second.first->testcase_filename, it->second.first} );
    }

    has_changed = false;

}



// 
//
// Reinitalization, flushing all coverage, then generating new coverage for some time from scratch 
// and accumulating everything
//

void setup_reinit( bool recycle )
{
    RE_INIT_USE                 = true;

    TIME_BETWEEN_INDEX_CHANGE    = RE_INIT_TIME_SEC + (mtr.randInt() % ( 1 + (int)RE_INIT_TIME_SEC/4) );

    if( recycle ){

        prev_real_solution_id       = solution_id;
        solution_id                 = prev_reinit_solution_id;

        set < testcase_file * > important;
        for( auto it=shtc_cover_only.begin(); it!= shtc_cover_only.end(); it++ )
            important.insert( it->second.first );
        for( auto it=shtc_cover_all.begin(); it!= shtc_cover_all.end(); it++ )
            important.insert( it->second.first );
        
        // store the previous all path important testcase
        temp_testcases.clear();
        for( auto it = all_testcases.begin(); it != all_testcases.end(); it++)
            if(     !prev_reinit_testcases.count( it->first )   // does not belong to the reinit
                ||  it->second->found_crash                     // is crash
                ||  important.count( it->second )                // is important (aka fast)
            )
                temp_testcases.insert( { it->first, it->second} );
    }
    else{

        for( auto it= shtc_cover_only.begin(); it!= shtc_cover_only.end(); it++){
            auto iz = reinit_shtc_cover_only.find( it->first );
            if( iz == reinit_shtc_cover_only.end() || iz->second.first > it->second.first )
                reinit_shtc_cover_only.insert( { it->first, { it->second.first, it->second.second } } );
        }

        for( auto it= shtc_cover_all.begin(); it!= shtc_cover_all.end(); it++){
            auto iz = reinit_shtc_cover_all.find( it->first );
            if( iz == reinit_shtc_cover_all.end() || iz->second.first > it->second.first )
                reinit_shtc_cover_all.insert( { it->first, { it->second.first, it->second.second } } );
        }

        for( auto it=all_testcases.begin(); it!= all_testcases.end(); it++)
            if( it->second->found_crash )
                reinit_crashed.insert( { it->first, it->second } );

    }


    memset( showmap,   0, 1<<16 );

    shtc_cover_only.clear();
    shtc_cover_all.clear();
    
    all_testcases.clear();
    allowed_testcases.clear();
    
    for( auto it = external_testcases.begin(); it != external_testcases.end(); it++){

        allowed_testcases.insert( { it->first, it->second } );
        all_testcases.insert( { it->first, it->second } );
        
        prepare_tmp_input( it->second->testcase_filename );
        run_one_fork( RUN_TRACER );
        execution_edge_coverage_update();
        execution_afl_coverages_produce_and_save_to_file(it->second->coverage_afl_filename);

        minimize_tc_according_to_branch( it->second );
    }

    remove_redundant_testcases( min_testcases_fast_cover_only, change_in_tc_according_to_cover_only,  shtc_cover_only );
    remove_redundant_testcases( min_testcases_fast_cover_all,  change_in_tc_according_to_cover_all ,  shtc_cover_all );

    last_time_redo_index = GET_TIMER();
    TIME_BETWEEN_INDEX_CHANGE = RE_INIT_TIME_SEC + (mtr.randInt() % ( 1 + (int)RE_INIT_TIME_SEC/4) );


    printf( KBLU "REINIT setup : %d : %6ld %6ld :  %6ld %6ld  :  %6ld %6ld \n\n" KNRM,
        RE_CURR_REPEATS,          
        temp_testcases.size(), external_testcases.size(),  
        allowed_testcases.size(), all_testcases.size(),
        min_testcases_fast_cover_only.size(), min_testcases_fast_cover_all.size()
         ) ;
}

void finish_reinit()
{

    // store reinit testcases
    prev_reinit_testcases =  all_testcases;
    printf( KBLU "REINIT store : %6ld : %6ld %6ld %6ld \n\n" KNRM, prev_reinit_testcases.size(), 
     reinit_shtc_cover_only.size(), reinit_shtc_cover_all.size(), reinit_crashed.size() )  ;

    // if the initial reinit, add all
    for(auto it=reinit_shtc_cover_only.begin(); it != reinit_shtc_cover_only.end(); it++){
        all_testcases.insert( {it->second.first->testcase_filename, it->second.first} );
        allowed_testcases.insert( {it->second.first->testcase_filename, it->second.first} );
    }
    reinit_shtc_cover_only.clear();
    for(auto it=reinit_shtc_cover_all.begin(); it != reinit_shtc_cover_all.end(); it++){
        all_testcases.insert( {it->second.first->testcase_filename, it->second.first} );
        allowed_testcases.insert( {it->second.first->testcase_filename, it->second.first} );
    }
    reinit_shtc_cover_all.clear();
    for(auto it=reinit_crashed.begin(); it != reinit_crashed.end(); it++){
        all_testcases.insert( {it->first, it->second} );
        allowed_testcases.insert( {it->first, it->second} );
    }
    reinit_crashed.clear();


    // add previous rounds important testcases
    for( auto it=temp_testcases.begin(); it != temp_testcases.end(); it++){
        all_testcases.insert( {it->first, it->second} );
        allowed_testcases.insert( {it->first, it->second} );
    }


    RE_INIT_USE                 = false;
    
    prev_reinit_solution_id     = solution_id;
    solution_id                 = prev_real_solution_id;

    REINIT_STAGES                  = 0;

    printf( KBLU "REINIT finish : %d : %6ld %6ld  \n\n" KNRM,
    RE_CURR_REPEATS,          
    allowed_testcases.size(), all_testcases.size() ) ;


}


//
//
// Switch of indices of the labels of the nodes. 
// 
//

void redo_with_different_index()
{

    bool redo_now = TIME_DIFFERENCE_SECONDS( last_time_redo_index ) >= TIME_BETWEEN_INDEX_CHANGE;

    if( !redo_now  ) return;

    START_TIMER( begin_index );

    // change map coverage index
    MAP_COVERAGE_INDEX = mtr.randInt() % 4;


    bool coming_out_of_re_init = false;

    if( RE_INIT_APPLY && RE_INIT_USE ){

        RE_CURR_REPEATS ++;

        if( RE_CURR_REPEATS < RE_INIT_REPEATS ){

            setup_reinit( false );
            tot_index_change_time += TIME_DIFFERENCE_SECONDS(  begin_index );
            return;
        }

        finish_reinit();
        coming_out_of_re_init   = true;
    
    }


    if( !coming_out_of_re_init ){

        REINIT_STAGES = (REINIT_STAGES + 1) % 4; 

        if( RE_INIT_APPLY && 0 == REINIT_STAGES ){

            setup_reinit( true );

            tot_index_change_time += TIME_DIFFERENCE_SECONDS(  begin_index );

            return;
        }
    }


    memset( showmap,   0, 1<<16 );
    shtc_cover_only.clear();
    shtc_cover_all.clear();
    allowed_testcases.clear();

    for( auto it = all_testcases.begin(); it != all_testcases.end(); it++){

        allowed_testcases.insert( { it->first, it->second } );
            
        prepare_tmp_input( it->second->testcase_filename );
        run_one_fork( RUN_TRACER );
        execution_edge_coverage_update();
        execution_afl_coverages_produce_and_save_to_file(it->second->coverage_afl_filename);

        minimize_tc_according_to_branch( it->second );
    }

    remove_redundant_testcases( min_testcases_fast_cover_only, change_in_tc_according_to_cover_only,  shtc_cover_only );
    remove_redundant_testcases( min_testcases_fast_cover_all,  change_in_tc_according_to_cover_all ,  shtc_cover_all );


    tot_index_change_time += TIME_DIFFERENCE_SECONDS(  begin_index );

    // if changing index is too slow (takes more than 10% of running time of fuzzer), 
    // then increase time between changes
    if( tot_index_change_time > 3600 && total_time > 0 && 
        tot_index_change_time/ total_time > 0.10 )
    {
        MIN_TIME_BETWEEN_INDEX_REDO *= 1.2;
        printf( KINFO "Min index switch time changed to %.1f \n" KNRM, MIN_TIME_BETWEEN_INDEX_REDO);
    }


    last_time_redo_index = GET_TIMER();
    TIME_BETWEEN_INDEX_CHANGE = MIN_TIME_BETWEEN_INDEX_REDO + ( mtr.randInt() % ( 1 + (int)MIN_TIME_BETWEEN_INDEX_REDO/4) );


    printf( KBLU "REINIT: %d : %d : %d :  %6ld %6ld %6ld %6ld :   %6ld %6ld \n\n" KNRM, 
        RE_INIT_USE, REINIT_STAGES, coming_out_of_re_init,
        min_testcases_fast_cover_only.size(), min_testcases_fast_cover_all.size(), 
        shtc_cover_only.size(), shtc_cover_all.size(), 
        allowed_testcases.size(), all_testcases.size() ) ;

}