
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


static byte *x;
static int lenx;
static int no_execs;
static string original_input;
static int type_vanilla_simple_or_ga;
static uint32_t start_found;
static Time mbegin_time;

static byte *y;
vector <uint32_t> vec_all_dep_bytes;

vector< node_mab > mab_bytesmutate_vanilla_ga_no_mutations;
vector< node_mab > mab_bytesmutate_vanilla_simple_or_ga;
vector< node_mab > mab_bytesmutate_vanilla_simple_weights_simple_or_weighted;
vector< node_mab > mab_bytesmutate_data_lognumber;
vector< node_mab > mab_bytesmutate_data_rand_or_count;

static bool vanilla_fuzzer;
static vector <float> weights_rand_bytes;

static vector <int>   positions_known_bytes;
static vector <float> weights_known_bytes;

static int type_weights_weigthed_or_simple;

static vector <bool> multiplier;

static float time_spent;
static uint64_t original_trace_hash;
static set <int> mut_position;
static int PARALLEL_POSITIONS = 1;


#define N 20
#define CROSS_PERCT 80
#define MUTATE_PERCT 5

static uint32_t GN;
static int ga_current_stage;
static int ga_current_fitness_index;
static int ga_generations_run;
static map <uint32_t , uint32_t> sv;
static int no_active = 50, origin_no_active;
static node_gs PopulationA[N],PopulationB[N];
static node_gs (*Population)[N], (*NewPopulation)[N];

static int    no_found = 0;
static int    no_trials = 0;

static int    cur_found;


void randomize_gs( node_gs &n   )
{
    memset( n.active, 0, lenx );
    if (lenx == 0) return;

    for( int i=0; i<no_active; i++)
        n.active[ mtr.randInt() % lenx] = 1;
}

void initialize_gs( node_gs &n )
{
    n.fitness   = -1;
    n.lenx      = lenx;
    n.tot       = 0;

    if( n.bytes == NULL) n.bytes     = (byte *)malloc( lenx );
    if( n.active== NULL) n.active    = (byte *)malloc( lenx );

}

void sample_one_gs( node_gs &n )
{
    memcpy( n.bytes, x, lenx );
    for(int i=0; i<lenx; i++)
        if( n.active[i] ){                
            n.bytes[i] += (mtr.randInt() % 2 ) ? ( (1 - 2*(mtr.randInt()%2 ) ) *  (1 + (mtr.randInt() % 35 ) ) ) : one_nonzero_uniformly_random_byte();
        }

}

void compute_one_fitness( node_gs &n )
{
    n.tot = 0;
    for( int i=0; i<n.lenx; i++)
        n.tot += n.active[i];

    no_trials ++;
    write_bytes_to_binary_file( TMP_INPUT, n.bytes, n.lenx);
    int good_exec = run_one_fork( RUN_GETTER );
    if( good_exec && execution_edge_coverage_update()  ){
        check_and_add_solution( TMP_INPUT, SOL_TYPE_MUTATE_RAND , ""  );
        no_found ++;
        cur_found ++;
        start_found ++;  
    }

    n.fitness = 0;
    set <uint32_t> new_br;
    for( int i=0; i<__new_branch_index[0]; i++){
        if( BRANCH_IS_IMPORTANT(branches_operations[i]) ) 
        {
            if( new_br.find( branches_trace[i] ) == new_br.end() )
            {
                auto it = sv.find( branches_trace[i] );
                if( it == sv.end() ) continue;
                new_br.insert( branches_trace[i] );

                n.fitness += it->second != compute_val( i );
            }
        }
    }
}

void crossover_one(  node_gs &p0,  node_gs &p1,  node_gs &c0,  node_gs &c1 )
{

    initialize_gs( c0 );
    initialize_gs( c1 );

    for( int i=0; i< p0.lenx; i++){
        c0.active[i] = (mtr.randInt() % 2 ) ? p0.active[i] : p1.active[i];
        c1.active[i] = (mtr.randInt() % 2 ) ? p0.active[i] : p1.active[i];
    }
}




void one_ga_step()
{


    // initialize 
    if( 0 == ga_current_stage )
    {

        no_found = 0;
        no_trials = 0;

        // Run and collect initial values
        prepare_tmp_input( original_input );
        run_one_fork( RUN_GETTER );

        sv.clear();
        map <int,int> counters;
        for( int ii=0; ii < __new_branch_index[0]; ii++) 
        {
            if( BRANCH_IS_UNIMPORTANT( branches_operations[ii] ) )
                continue;

            auto it = sv.find( branches_trace[ii] );
            if( it != sv.end() ) continue;

            uint32_t val =  compute_val(ii);        
            sv.insert( {    branches_trace[ii]  , val } );
        }

        // decide on the number of mutated bytes
        vector <bool> ml;
        for(int i=0; i<mab_bytesmutate_vanilla_ga_no_mutations.size(); i++){
            if( (1 << i) < lenx/32 )    ml.push_back( true );
            else                        ml.push_back( false );
        }
        origin_no_active = mab_sample( mab_bytesmutate_vanilla_ga_no_mutations, mab_bytesmutate_vanilla_ga_no_mutations.size(), false, ml );
        origin_no_active = mmax( origin_no_active, 0 );
        no_active = 1 << origin_no_active;
        if( no_active > lenx ) no_active = lenx;


        // init population
        for( int i=0; i<GN; i++){
            initialize_gs( PopulationA[i] );
            randomize_gs ( PopulationA[i] );
        }


        // sample 
        for( int i=0; i<GN; i++)
            sample_one_gs( PopulationA[i] );

        Population      = &PopulationA;
        NewPopulation   = &PopulationB;

        ga_current_stage = 1;
        ga_current_fitness_index = 0;

    }
    // compute fitness
    else if( 1 == ga_current_stage )
    {

        compute_one_fitness( (*Population)[ga_current_fitness_index] );

        ga_current_fitness_index ++;
        
        // finished all
        if( ga_current_fitness_index >= GN){
        
            //crossover
            vector <float> w,cw;
            for( int i=0; i<GN; i++)
                w.push_back( (*Population)[i].fitness );
            int ind1, ind2;
            int cur_pop_ind = 0;

            // crossover the first CROSS_PERCT percentage
            for( int l=0; l < CROSS_PERCT / 100.0 * GN / 2; l++){
                ind1 = discrete_distribution_sample( w );
                cw = w;
                cw[ind1] /= 10000000.0;
                ind2 = discrete_distribution_sample( cw );

                crossover_one(  (*Population)[ind1]             , (*Population)[ind2], 
                                (*NewPopulation)[cur_pop_ind]   , (*NewPopulation)[cur_pop_ind+1]
                        );
                cur_pop_ind += 2;
            }
            // copy the remain
            int l =0;
            while(cur_pop_ind < GN ){
                int best_index = -1;
                for( int i=0; i< GN; i++)
                    if( best_index < 0 || (*Population[i]).fitness > (*Population)[best_index].fitness )
                        best_index = i;

                initialize_gs( (*NewPopulation)[cur_pop_ind] );
                (*NewPopulation)[cur_pop_ind].fitness = (*Population)[best_index].fitness;
                byte *tmp;
                tmp                                     = (*NewPopulation)[cur_pop_ind].active ; 
                (*NewPopulation)[cur_pop_ind].active    = (*Population)[best_index].active;
                (*Population)[best_index].active        = tmp;
                tmp                                     = (*NewPopulation)[cur_pop_ind].bytes ; 
                (*NewPopulation)[cur_pop_ind].bytes     = (*Population)[best_index].bytes;
                (*Population)[best_index].bytes         = tmp;


                l++;
                cur_pop_ind++;
            }

            // ga mutate
            for( int i=0; i < CROSS_PERCT / 100.0 * GN / 2; i++)
            //for( int i=0; i<GN; i++)
                if( ( mtr.randInt() % 100) < MUTATE_PERCT ){
                    randomize_gs( (*NewPopulation)[i] ) ;
                }

            // resample
            for( int i=0; i<GN; i++)
                sample_one_gs( (*NewPopulation)[i] );

            
            // reverse
            if ( Population == &PopulationA ){
                Population      = &PopulationB;  
                NewPopulation   = &PopulationA;  
            }
            else{
                Population      = &PopulationA;  
                NewPopulation   = &PopulationB;  
            }

            ga_current_stage = 1;
            ga_current_fitness_index = 0;
            ga_generations_run++;

        }

    }

}


void init_fuzzer_bytes_mutation(  int init_no_execs, byte *init_x, int init_lenx, bool real_dum_fuz, string roriginal_input, uint64_t o_trace_hash, Time begin_time )
{

    x                   = init_x;
    lenx                = init_lenx;
    no_execs            = init_no_execs;
    vanilla_fuzzer         = real_dum_fuz;
    original_input      = roriginal_input;
    original_trace_hash = o_trace_hash;
    mbegin_time         = begin_time;


    y = (byte *)malloc(lenx + 1);

    start_found = 0;
    time_spent  = 0;
        
    mab_recompute( mab_bytesmutate_vanilla_ga_no_mutations );
    mab_recompute( mab_bytesmutate_vanilla_simple_or_ga );
    mab_recompute( mab_bytesmutate_vanilla_simple_weights_simple_or_weighted );
    mab_recompute( mab_bytesmutate_data_lognumber );
    mab_recompute( mab_bytesmutate_data_rand_or_count );

    vec_all_dep_bytes.clear();
    weights_rand_bytes.clear();
    weights_known_bytes.clear();
    positions_known_bytes.clear();
    mut_position.clear();

    ga_current_stage = 0;
    for( int i=0; i<N; i++){
        PopulationA[i].active = NULL;
        PopulationA[i].bytes  = NULL;
        PopulationB[i].active = NULL;
        PopulationB[i].bytes  = NULL;
    }
    ga_generations_run = 0;
    GN                 = 20; 
    
    if( vanilla_fuzzer){

        type_vanilla_simple_or_ga = mab_sample( mab_bytesmutate_vanilla_simple_or_ga, 2, false, multiplier );        
        for( int i=0; i<mmin(512,lenx); i++)
            weights_rand_bytes.push_back( 1.0 / log (3 + i ) );
        PARALLEL_POSITIONS = 1 + (mtr.randInt() % 3);       
    }
    else{

        // create vector of dependent bytes 
        if( PRINT_DEBUG_FUZZER_BYTES ) printf("Dependent bytes: ");
        for( auto it=set_all_dep_bytes.begin(); it!=set_all_dep_bytes.end(); it++){
            vec_all_dep_bytes.push_back( *it );
            if( PRINT_DEBUG_FUZZER_BYTES ) printf("%d ", *it );
        }
        if( PRINT_DEBUG_FUZZER_BYTES ) printf("\n");

        // create distribution of dependent bytes according to the number of branches the byte is present 
        for( auto it = byte_to_branches.begin(); it != byte_to_branches.end(); it++){
            if( it->second > 0 ){
                positions_known_bytes.push_back( it->first );
                weights_known_bytes.push_back( pow( it->second, 2) );
            }
        }
    }


}

void fini_fuzzer_bytes_mutation()
{
    free( y );

    for( int i=0; i < N; i++){
        if( PopulationA[i].active != NULL ) free(  PopulationA[i].active );
        if( PopulationA[i].bytes  != NULL ) free(  PopulationA[i].bytes  );
        if( PopulationB[i].active != NULL ) free(  PopulationB[i].active );
        if( PopulationB[i].bytes  != NULL ) free(  PopulationB[i].bytes  );
    }
    
    if( vanilla_fuzzer ){

        mab_update( mab_bytesmutate_vanilla_simple_or_ga, type_vanilla_simple_or_ga, start_found, time_spent ); 
        if( 1 == type_vanilla_simple_or_ga){
            if( no_active == (1 << (origin_no_active))  && no_trials > 0  && glob_iter > 1){
                mab_update( mab_bytesmutate_vanilla_ga_no_mutations, origin_no_active,  no_found, no_trials );    
            }
        }
    }
}


bool can_run_bytes_mutation( )
{
    return lenx > 0 && ( vanilla_fuzzer || vec_all_dep_bytes.size() > 0 && weights_known_bytes.size() > 0 );
}


int exec_block_bytes_mutation( float min_running_time )
{
    int found_new = 0;
    map <int,int> one_solution;
    int byte_ind, byte_pos;
    int good_exec;

    START_TIMER ( begin_time_spent );

    int init_no_found = cur_found;

    for( int e=0; e<no_execs; e++){

        if( TIME_DIFFERENCE_SECONDS(mbegin_time) >= min_running_time ) break; 

        if( vanilla_fuzzer){

            if( 0 == type_vanilla_simple_or_ga ){
                fuzzer_current_main = SOL_TYPE_MUTATE_RAND1;
                type_weights_weigthed_or_simple = mab_sample( mab_bytesmutate_vanilla_simple_weights_simple_or_weighted, 2, false, multiplier );        
                memcpy( y, x, lenx );

                int bindex = 0;
                for(int k=0; k < PARALLEL_POSITIONS; k++){
                    if( 0 == type_weights_weigthed_or_simple){  
                        bindex = discrete_distribution_sample(  weights_rand_bytes );
                        if( bindex >= 0 && bindex < weights_rand_bytes.size() )
                            weights_rand_bytes[bindex] /= 10;

                    }
                    else{                        
                        bindex = mtr.randInt() % lenx;
                    }
                    if( bindex >= 0 && bindex < lenx )
                        mut_position.insert( bindex );
                }


                for(auto it=mut_position.begin(); it!= mut_position.end(); it++)
                    y[ *it ] += (mtr.randInt() % 2 ) ? ( (1 - 2*(mtr.randInt()%2 ) ) *  (1 + (mtr.randInt() % 35 ) ) ) : one_nonzero_uniformly_random_byte();

                write_bytes_to_binary_file(TMP_INPUT, y, lenx);
                good_exec = run_one_fork( RUN_TRACER );
                int found_new_sw = 0;
                if( good_exec && execution_edge_coverage_update() ){

                    check_and_add_solution( TMP_INPUT, SOL_TYPE_MUTATE_RAND1, ""  );
                    finalize_solution( one_solution );
                    
                    start_found ++;
                    found_new ++;
                    found_new_cov += 2==glob_found_different;
                    found_new_sw = 1;
                }

                if( good_exec && hash_showmap != original_trace_hash || mut_position.size() > 100 ){
                    mut_position.clear();
                }
                mab_update( mab_bytesmutate_vanilla_simple_weights_simple_or_weighted, type_weights_weigthed_or_simple, found_new_sw, 1.0 ); 
            }
            else{
                fuzzer_current_main = SOL_TYPE_MUTATE_RAND;
                one_ga_step();
            }
            continue;
        }

        if( 0 == vec_all_dep_bytes.size()   ) break;
        if( 0 == weights_known_bytes.size() ) break;

        fuzzer_current_main = SOL_TYPE_MUTATE_KNOWN;
        
        int log_how_many        = mab_sample( mab_bytesmutate_data_lognumber, mab_bytesmutate_data_lognumber.size(), false, multiplier );
        int how_many_to_mutate  = 1<<log_how_many;
        int use_rand_or_count   = mab_sample( mab_bytesmutate_data_rand_or_count, 2, false, multiplier );

        memcpy( y, x, lenx);
        one_solution.clear();
        for( int j=0; j<how_many_to_mutate; j++){

            if( 0 == use_rand_or_count )
                byte_pos = vec_all_dep_bytes[ mtr.randInt() % vec_all_dep_bytes.size() ];            
            else{
                int byte_index = discrete_distribution_sample( weights_known_bytes );
                if( byte_index > 0 ){
                    weights_known_bytes[byte_index] /= 100.0;
                    byte_pos = positions_known_bytes[ byte_index ] ;
                }
            }

            if( byte_pos >= 0 && byte_pos < lenx){
                y[ byte_pos ]  += (mtr.randInt() % 2 ) ? ( (1 - 2*(mtr.randInt()%2 ) ) *  (1 + (mtr.randInt() % 35 ) ) ) : one_nonzero_uniformly_random_byte();
                one_solution.insert( {byte_pos, y[byte_pos] });
            }
        }


        write_bytes_to_binary_file(TMP_INPUT, y, lenx);
        good_exec = run_one_fork( RUN_TRACER );
        int found_one_new = 0;
        if( good_exec && execution_edge_coverage_update() )
        {

            check_and_add_solution( TMP_INPUT, SOL_TYPE_MUTATE_KNOWN, ""  );
            finalize_solution( one_solution );
            
            found_one_new = 1;
            found_new ++;
            found_new_cov += 2==glob_found_different;
        
        }
                
        mab_update( mab_bytesmutate_data_lognumber,     log_how_many,       found_one_new, 1.0 ); 
        mab_update( mab_bytesmutate_data_rand_or_count, use_rand_or_count,  found_one_new, 1.0 ); 

    }

    time_spent += TIME_DIFFERENCE_SECONDS ( begin_time_spent );


    return found_new + cur_found - init_no_found;
}
