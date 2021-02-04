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

static vector<int>   cand_indices;
static vector<float> cand_probs;
static map <int,int> one_solution;
static int fitness_const;
static int fitness_type;
static int fitness_cmp;
static int crossover_type;
static int oper_type;


static int cur_found;
static bool keep_going;

#define MAX_POPULATION_SIZE 24
#define CROSS_PERCT 80
#define MUTATE_PERCT 5
#define MAX_ITER 20

#define BAD_FITNESS ( (long int )1<<40 )

static int USE_IN_PARALLEL = 10;
static int POPULATION_SIZE = 20;

typedef struct {
    vector <uint8_t> byte_vals;
    long int fitness;
}solution;


typedef struct {
    int cand_index;
    int index_in_cand;
    int branch_index;
    targetNode one_candidate;
    branch_address_pair target_branch_address;
    int target_index;
    int index_in_trace;
    int current_index_in_trace;
    int branch_type;
    
    int fitness_const;
    int fitness_type;
    int fitness_cmp;
    int crossover_type;
    int oper_type;
    set <uint64_t> diff_fit_vals;

    bool found_some_index;
    bool found_target;

    solution population[MAX_POPULATION_SIZE];

    vector <int> deps;
    int no_iterations;

}node_ga;


static vector < node_ga > c;
static int ga_current_stage;
static int ga_current_fitness_index;
static int ga_generations_run;




void solution_mutate( solution &v);

#define INIT_POPUL ((long int)-1)

void solution_init( solution &v, vector <int> &tmp_deps , int init_type)
{
    v.byte_vals.clear();
    v.fitness = INIT_POPUL;
    if( 0 == init_type ){ // random or with a single mutation
        for( int i=0; i<tmp_deps.size(); i++)
            if( tmp_deps[i] < lenx )
                v.byte_vals.push_back( one_rand_byte() );
    }
    else{
        for( int i=0; i<tmp_deps.size(); i++)
            if( tmp_deps[i] < lenx )
                v.byte_vals.push_back( x_ri[ tmp_deps[i] ]  );
        
        solution_mutate( v );
    }
}

void solution_mutate( solution &v )
{
    if( v.byte_vals.size() == 0) return;

    v.byte_vals[mtr.randInt() % v.byte_vals.size() ] ^= one_nonzero_rand_nibble();
}

int mhamming_weight( uint64_t f )
{   
    int s =0;
    for( int i=0; i<32; i++) s += (f>>i) & 1;
    return s;
}


int solutions_update_fitness( int pop_index )
{
    memcpy( y, x_ri, lenx );
    for( int k=0; k<c.size(); k++){
        for( int i=0; i<c[k].deps.size(); i++)
            if( c[k].deps[i] < lenx ){
                y[ c[k].deps[i] ] = c[k].population[ pop_index ].byte_vals[i];
            }
    }

    write_bytes_to_binary_file( TMP_INPUT, y, lenx);
    int good_exec = run_one_fork( RUN_GETTER );
    if( good_exec && execution_edge_coverage_update()  ){

        check_and_add_solution( TMP_INPUT, SOL_TYPE_BRANCH_GA , ""  );

        one_solution.clear();
        for( int k=0; k<c.size(); k++)
            for( int i=0; i<c[k].deps.size(); i++)
                if( c[k].deps[i] < lenx )
                    one_solution.insert( {c[k].deps[i], y[c[k].deps[i]] });
        finalize_solution( one_solution );

        cur_found ++;
        found_new_cov += 2==glob_found_different;

    }


    map <branch_address_pair,int> stargets;
    for( int k=0; k<c.size(); k++ ){
        c[k].current_index_in_trace = -1;
        stargets.insert( {c[k].target_branch_address, k} );
    }


    map <int,int> counters;
    for( int i=0; i < __new_branch_index[0] ; i++){

        if( UNPACK_BRANCH_TYPE( branches_operations[i]) )
        {

            auto it = counters.find( branches_trace[i] );
            if( it == counters.end() ){
                counters.insert( {branches_trace[i], 0});
                it = counters.find( branches_trace[i] );
            }
            else
                it->second ++;

            auto p = get_ba( it->first, it->second );

            if( stargets.find( p ) != stargets.end()  ){
                c[ stargets.find( p )->second ].current_index_in_trace = i;

                bool all_found = true;
                for( int k=0; k < c.size(); k++)
                    if( c[k].current_index_in_trace < 0 ){
                        all_found = false;
                        break;
                    }
                if( all_found) 
                    break;
            }
        }
    }


    for(int k=0; k < c.size(); k++)
    {

        int index = c[k].current_index_in_trace;
        c[k].found_some_index |= index >= 0;
        if( index < 0 ) continue;

        int index_in_old_trace = c[k].index_in_trace;


        if( branches_trace[index] != store_branches_trace[ index_in_old_trace ] ){
            c[k].population[pop_index].fitness =  BAD_FITNESS;
        }
        else
        {

            if( 2 == c[k].branch_type ){
                if( 0 == c[k].fitness_type ) 
                    c[k].population[pop_index].fitness = mhamming_weight( branches_taken[index] ^ c[k].fitness_const  );
                else c[k].population[pop_index].fitness = branches_taken[index] > c[k].fitness_const ? 
                                branches_taken[index] - c[k].fitness_const :
                                c[k].fitness_const - branches_taken[index] ;
            }
            else{
                if( 51 == c[k].oper_type ) // ICMP
                {
                    if( 0 == c[k].fitness_cmp){ //A==B
                        if( 0 == c[k].fitness_type ) 
                            c[k].population[pop_index].fitness = mhamming_weight( branches_value1[index] ^ branches_value2[index]  );
                        else c[k].population[pop_index].fitness = branches_value1[index] > branches_value2[index] ? 
                                        branches_value1[index] - branches_value2[index] :
                                        branches_value2[index] - branches_value1[index] ;
                    }
                    else if( 1 == c[k].fitness_cmp){ // A > B
                        if( branches_value1[index] > branches_value2[index] ){
                            c[k].found_target = true;
                            continue;
                        } 
                        c[k].population[pop_index].fitness = branches_value2[index] - branches_value1[index];
                    }
                    else if( 2 == c[k].fitness_cmp){ // A < B
                        if( branches_value1[index] < branches_value2[index] ){
                            c[k].found_target = true;
                            continue;
                        } 
                        c[k].population[pop_index].fitness = branches_value1[index] - branches_value2[index];
                    }
                    else if( 3 == c[k].fitness_cmp){ // A > B  signed
                        if( (int)branches_value1[index] > (int)branches_value2[index] ){
                            c[k].found_target = true;
                            continue;
                        } 
                        c[k].population[pop_index].fitness = (int)branches_value2[index] - (int)branches_value1[index];
                    }
                    else if( 4 == c[k].fitness_cmp){ // A < B signed
                        if( (int)branches_value1[index] < (int)branches_value2[index] ){
                            c[k].found_target = true;
                            continue;
                        } 
                        c[k].population[pop_index].fitness = (int)branches_value1[index] - (int)branches_value2[index];
                    }
                }
                else if( 52 == c[k].oper_type ) // fcmp
                {
                    c[k].population[pop_index].fitness = branches_valuedouble[index];
                }
            }

        }

        if( 0 == c[k].population[pop_index].fitness )
            c[k].found_target = true;

    }

    return 0;
    
}


bool ga_sample_into_vector()
{

    if( cand_probs.size() == 0 ) return false;

    set <uint32_t> abytes;
    for( int k=0; k< c.size(); k++){
        
        for( auto it=c[k].deps.begin(); it!= c[k].deps.end(); it++)
            abytes.insert( *it );
    }

    vector<int  >   c_cand_indices;
    vector<float>   c_cand_probs;
    vector<int >    c_index_in_cand;

    for( int i=0; i<cand_indices.size(); i++){

        auto deps = branch_get_correct_dependency( branch_candidates[ cand_indices[i] ].target_branch_address );
        int found = false;
        
        for( int j= 0; j<deps.size(); j++ )
            if( abytes.count( deps[j] ) ){
                found = true;
                break;
            }
        if( !found ){
            c_cand_indices.push_back    ( cand_indices[i] );
            c_cand_probs.push_back      ( cand_probs[i] );
            c_index_in_cand.push_back   ( i );
        }

    }

    if( 0 ==  c_cand_indices.size() ) return false;

    while( c_cand_indices.size() > 0 ){

        node_ga n;

        if( mtr.randInt() % 2)
            n.cand_index = discrete_distribution_sample( c_cand_probs );
        else
            n.cand_index = mtr.randInt() % c_cand_probs.size();

        n.index_in_cand = c_index_in_cand[n.cand_index];  

        n.branch_index = c_cand_indices[ n.cand_index ];
        n.one_candidate = branch_candidates[ n.branch_index ];
        n.target_branch_address   = n.one_candidate.target_branch_address;
        n.target_index                            = n.one_candidate.target_index;
        n.index_in_trace                          = n.one_candidate.index_in_trace;
        n.branch_type                             = UNPACK_BRANCH_TYPE( store_branches_opera[ n.index_in_trace]);
        n.oper_type                               = UNPACK_BRANCH_COMPARE_TYPE( store_branches_opera[ n.index_in_trace]);
        n.deps  = branch_get_correct_dependency( n.target_branch_address );

        if( n.deps.size() == 0 || n.branch_type <= 0 ){
            c_cand_indices.erase( c_cand_indices.begin() + n.cand_index );
            c_cand_probs.erase( c_cand_probs.begin() + n.cand_index );
            continue;
        }

        n.fitness_type = mtr.randInt() % 2;
        n.crossover_type = mtr.randInt() % 2;
        if ( 2 == n.branch_type )
            n.fitness_const = nodes_choose_random_unsolved_switch( n.target_branch_address.first ) ;
        else if( 1 == n.branch_type){
            if( mtr.randInt() % 2)
                n.fitness_cmp = 0;
            else
                n.fitness_cmp = 1 + (mtr.randInt() % 4);
        }
        n.diff_fit_vals.clear();
        n.no_iterations = 0; 

        c.push_back( n );

        cand_probs[ n.index_in_cand ] /= 20.0;

        break;
    }


    return c_cand_indices.size() > 0;

}


void init_ga_solver()
{

    write_bytes_to_binary_file( TMP_INPUT, x_ri, lenx);
    run_one_fork( RUN_GETTER );

    cand_indices.clear();
    cand_probs.clear();
    for( int i=0; i< branch_candidates.size(); i++){

        int target_index = branch_candidates[ i ].target_index;
        int index_in_trace = branch_candidates[ i ].index_in_trace;

        // something is wrong with the execution
        if( __new_branch_index[0] != store_branches_tot
            || branches_trace[index_in_trace] != store_branches_trace[index_in_trace] ){

                continue;

            }

        // most likely this is never true, but just in case
        if( BRANCH_IS_UNIMPORTANT(branches_operations[index_in_trace] ) )
            continue;

        // if cannot extract the values (value1,value2, value_float), then skip
        if( ! USED_BRANCH_VAL_ANY(branches_operations[index_in_trace] ) )
            continue;

        // if the operation is not icmp or fcmp, then objective cannot be set
        if( !OPERATION_IS_SOME_CMP(branches_operations[index_in_trace]) )
            continue;


        // depends at least on 2 bytes (1 byte means most likely can be found with normal branch fuzzer)        
        auto tmp_deps = branch_get_correct_dependency( branch_candidates[ i ].target_branch_address );
        if( tmp_deps.size() <= 1) continue;
    

        cand_indices.push_back( i );
        cand_probs.push_back( branch_candidates[i].score   );

    }

    if ( no_execs > cand_indices.size() )
        no_execs = cand_indices.size();

}





void solutions_crossover( int crossover_type, solution &p1, solution &p2, solution &c1, solution &c2)
{
    uint32_t cross_size = 8 * p1.byte_vals.size();
    if( 0 == cross_size ){
        printf("Incorrect cross size: %d\n", cross_size);
        exit(1);
    }

    c1.fitness = INIT_POPUL;
    c2.fitness = INIT_POPUL;

    c1.byte_vals.clear();
    c2.byte_vals.clear();
    for( int i=0; i< p1.byte_vals.size(); i++){
        c1.byte_vals.push_back( 0) ;
        c2.byte_vals.push_back( 0) ;
    }

    if( crossover_type ){

        uint32_t cross_point = mtr.randInt() % cross_size;

        for( int i=0; i<cross_point; i++){
            c1.byte_vals[i/8] ^= ( (p1.byte_vals[i/8] >> (i%8) ) & 1) << (i%8);
            c2.byte_vals[i/8] ^= ( (p2.byte_vals[i/8] >> (i%8) ) & 1) << (i%8);
        }
        for( int i=cross_point; i<cross_size; i++){
            c1.byte_vals[i/8] ^= ( (p2.byte_vals[i/8] >> (i%8) ) & 1) << (i%8);
            c2.byte_vals[i/8] ^= ( (p1.byte_vals[i/8] >> (i%8) ) & 1) << (i%8);
        }
    }
    {
        for( int i=0; i<cross_size; i++){
            if( mtr.randInt() % 2 ){
                c1.byte_vals[i/8] ^= ( (p1.byte_vals[i/8] >> (i%8) ) & 1) << (i%8);
                c2.byte_vals[i/8] ^= ( (p2.byte_vals[i/8] >> (i%8) ) & 1) << (i%8);
            }
            else{
                c1.byte_vals[i/8] ^= ( (p2.byte_vals[i/8] >> (i%8) ) & 1) << (i%8);
                c2.byte_vals[i/8] ^= ( (p1.byte_vals[i/8] >> (i%8) ) & 1) << (i%8);
            }
        }
    }
}


void one_branch_ga_step()
{

    //
    // Initialize 
    // 
    if( 0 == ga_current_stage )
    {

        cur_found = 0;

        // init solver
        // run once to collect original branches 
        init_ga_solver();
    
        // init population
        c.clear();

        int last_sampled;
        while( c.size() < USE_IN_PARALLEL ){
            last_sampled = ga_sample_into_vector(); 
            if( !last_sampled  )
                break;
        }

        if( c.size() == 0 ){
            keep_going = false;
            return;
        }
        

        for( int k=c.size() - 1; k >= 0 ; k-- )
            if( c[k].no_iterations == 0){
                int found_incorrect_popul = 0;
                for(int n=0; n< POPULATION_SIZE; n++){
                    solution_init( c[k].population[n], c[k].deps , mtr.randInt() % 2 );
                    if( c[k].population[n].byte_vals.size() == 0) 
                        found_incorrect_popul = 1;
                }
                if( found_incorrect_popul ){ 
                    //printf("incorrect population\n");
                    c.erase( c.begin() + k );
                    continue;
                }

                c[k].no_iterations ++;
            }

        
        // found some set to false
        // found some index is true if any individual in the population was found
        // found target is true if the objective function target has been reached
        for( int k=0; k < c.size(); k++){
            c[k].found_some_index = false;
            c[k].found_target = false;
        }


        ga_current_stage            = 1;
        ga_current_fitness_index    = 0;
        ga_generations_run          = 0;


    }
    // compute fitness
    else if( 1 == ga_current_stage )
    {
     
        solutions_update_fitness( ga_current_fitness_index );

        ga_current_fitness_index ++;
        
        // finished all
        if( ga_current_fitness_index >=  POPULATION_SIZE){


            // update iteration count and target found
            int found_any_index = false;
            for( int k=c.size() -1 ; k >=0; k--) {

                if( c[k].found_target ){
                    c.erase( c.begin() + k );
                    continue;
                }

                if( c[k].found_some_index ){
                    c[k].no_iterations ++;
                    found_any_index = true;
                }

            }
            if( c.size() == 0) return;

            if( !found_any_index){
                while( c.size() > 0 ){
                    c.erase( c.begin() );
                }
            }

            for( int k=c.size() -1 ; k >=0; k--) {

                // sort
                for( int i=0; i<POPULATION_SIZE; i++){
                    for( int j=i+1; j<POPULATION_SIZE; j++)
                        if( c[k].population[i].fitness > c[k].population[j].fitness ){
                            auto tmp = c[k].population[j];
                            c[k].population[j] = c[k].population[i];
                            c[k].population[i] = tmp;
                        }
                }

                // crossover (stochastic)
                // find weights
                vector <float> w, cw;

                w.clear();
                int found_zero = 0;
                for( int i=0; i<POPULATION_SIZE; i++){
                    if( c[k].population[i].fitness <= 0 )
                        w.push_back( 0 + 1.4e-45 );
                    else
                        w.push_back( 1.0 / c[k].population[i].fitness );
                }
                // this should ever be true because similar check already exists in update_fitness
                if( found_zero ){
                    c.erase( c.begin() + k );
                    continue;
                } 

                
                // crossover actual
                solution new_population[MAX_POPULATION_SIZE];
                int ind1,ind2;
                int cur_pop_ind = 0;
                for( int l=0; l < CROSS_PERCT / 100.0 * POPULATION_SIZE / 2; l++){
                    ind1 = discrete_distribution_sample( w );
                    cw = w;
                    cw[ind1] /= 10000000.0;
                    ind2 = discrete_distribution_sample( cw );

                    solutions_crossover( c[k].crossover_type,  c[k].population[ind1], c[k].population[ind2],
                                        new_population[cur_pop_ind], new_population[cur_pop_ind+1] );

                    cur_pop_ind += 2;
                }

                int l =0;
                while(cur_pop_ind < POPULATION_SIZE){
                    new_population[cur_pop_ind] = c[k].population[l];
                    l++;
                    cur_pop_ind++;
                }

                // copy the population 
                for( int n=0; n<POPULATION_SIZE; n++)
                    c[k].population[n] = new_population[n];


                // mutate
                for( int n=0; n<POPULATION_SIZE; n++)
                    if( ( mtr.randInt() % 100) < MUTATE_PERCT )
                        solution_mutate( c[k].population[n] );


                // remove duplicates
                for( int n=0; n<POPULATION_SIZE; n++){
                    int found = 0;
                    for( int m=n-1; m>=0; m--)
                        if( c[k].population[n].byte_vals == c[k].population[m].byte_vals ){
                            found = 1;
                            break;
                        }
                    if( found ){
                        solution_init( c[k].population[n], c[k].deps , 0 ); 
                    }
                }

                if( c[k].no_iterations > MAX_ITER ){
                    c.erase( c.begin() + k );
                    continue;
                }

            }


            // Resample
            int last_sampled;
            while( c.size() < USE_IN_PARALLEL ){
                last_sampled = ga_sample_into_vector(); 
                if( !last_sampled  )
                    break;
            }

            if( c.size() == 0 ){
                keep_going = false;
                return;
            }
            

            for( int k=c.size() - 1; k >= 0 ; k-- )
                if( c[k].no_iterations == 0){
                    int found_incorrect_popul = 0;
                    for(int n=0; n< POPULATION_SIZE; n++){
                        solution_init( c[k].population[n], c[k].deps , mtr.randInt() % 2 );
                        if( c[k].population[n].byte_vals.size() == 0) 
                            found_incorrect_popul = 1;
                    }
                    if( found_incorrect_popul ){ 
                        //printf("incorrect population\n");
                        c.erase( c.begin() + k );
                        continue;
                    }

                    c[k].no_iterations ++;
                }

            
            // found some set to false
            // found some index is true if any individual in the population was found
            // found target is true if the objective function target has been reached
            for( int k=0; k < c.size(); k++){
                c[k].found_some_index = false;
                c[k].found_target = false;
            }




            ga_current_stage = 1;
            ga_current_fitness_index = 0;
            ga_generations_run++;

        }

    }

}





void init_fuzzer_branch_ga(  int init_no_execs, byte *init_x, int init_lenx, Time begin_time  )
{
    x_ri                = init_x;
    lenx                = init_lenx;
    no_execs            = init_no_execs;
    mbegin_time         = begin_time;

    y                   = (byte *)malloc(lenx + 1);

    ga_current_stage    = 0;

    keep_going          = true;

    // decide on parallelization factor
    USE_IN_PARALLEL = 1 + 5 * (mtr.randInt() % 3 );

    // decide on population size
    POPULATION_SIZE = 6 * (1 + (mtr.randInt() % 4 ) );


}

void fini_fuzzer_branch_ga()
{
    free( y );
}



bool can_run_branch_ga( )
{
    return true;
    //return cand_indices.size() > 2;
}


int exec_branch_ga( float min_running_time )
{

    fuzzer_current_main = SOL_TYPE_BRANCH_GA;

    cur_found = 0;

    for( int e=0; e<no_execs; e++){

        if( TIME_DIFFERENCE_SECONDS(mbegin_time) >= min_running_time ) break; 
        if( !keep_going ) break;

        one_branch_ga_step();
    }

    return cur_found;

}