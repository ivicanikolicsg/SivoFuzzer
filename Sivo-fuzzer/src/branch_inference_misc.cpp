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


#include "branch_inference_misc.h"
#include "fork_servers.h"
#include "executions.h"
#include "fork_servers.h"
#include "misc.h"


static byte     *x, *y, *u, *d;
static int      no_input_bytes;
static Time     begin_time_detect_important; 
static Time     begin_time_unmutable; 
static float    time_max_slow_inference;
static uint32_t *origin_values;
static set <branch_address_pair> seen;
static float    seen_perct;
static bool     use_forced;

static int      cur_set;
static int      max_set     = 128;
static int      max_perct   = 100.0;
static float    last_comp_perct = 0;

static int      important_size_set_1;
static int      important_size_stop;

byte *differences[MAX_NO_DIFF];



void init_differences()
{
    for( int i=0; i<MAX_NO_DIFF; i++)
        differences[i] = NULL;
}

void setup_differences( int lenx )
{
    for( int j=0; j<MAX_NO_DIFF; j++)
        differences[j] = (byte *)malloc( lenx );

    for( int i=0; i<lenx; i++){
        set <int> v;
        int ind = 0;
        while( v.size() < MAX_NO_DIFF ){
            auto o = one_nonzero_uniformly_random_byte();
            if( v.find( o ) == v.end()){
                differences[ind++][i] = o;
                v.insert( o );
            }
        }
    }
}

void free_differences()
{
    for( int j=0; j<MAX_NO_DIFF; j++)
        if( differences[j] != NULL) {
            free(differences[j] );
            differences[j] = NULL;
        }
}


uint64_t count_br_with_dep( vector  < branch_address_pair > branches_with_dependency )
{
    uint64_t s = 0;
    for( auto it= branches_with_dependency.begin(); it!=branches_with_dependency.end(); it++ ){
        s += get_dependency( *it ).size() > 0;
    }
    return s;
}

uint64_t count_deps( vector  < branch_address_pair > branches_with_dependency )
{
    uint64_t s = 0;
    for( auto it= branches_with_dependency.begin(); it!=branches_with_dependency.end(); it++ ){
        s += get_dependency( *it ).size();
    }
    return s;
}


uint64_t count_deps_vars( vector  < branch_address_pair > branches_with_dependency )
{
    uint64_t s = 0;
    set <int> D;
    for( auto it= branches_with_dependency.begin(); it!=branches_with_dependency.end(); it++ ){
        auto S = get_dependency( *it );
        for( auto zz = S.begin(); zz != S.end(); zz ++ )
            D.insert( *zz );
    }
    return D.size();
}



bool execution_differs_from_primary( )
{
    run_one_fork( RUN_GETTER );
    if( __new_branch_index[0] != store_branches_tot ){ 
        return true;
    }

    for( int i=0; i< __new_branch_index[0] ; i++){
        if( branches_trace[i] ^ store_branches_trace[i] ){ 
            return true;
        }
        
        if( BRANCH_IS_IMPORTANT( branches_operations[i]) && (compute_val( i ) ^ origin_values[i] ) )
        {
            return true;
        }
        
    }

    return false;
}


bool detect_range_important_bytes( int left, int right )
{
    if( TIME_DIFFERENCE_SECONDS( begin_time_detect_important )  >  time_max_slow_inference ){ 
        printf( KERR "Timeout on detect important bytes \n" KNRM );
        return false;
    }

    if( left == right ) return false;

    if( right - left <= important_size_set_1 && u[left] == 0)
        memset( u + left, 1, right-left );


    if( right - left < important_size_stop )
    { 
        return true;
    }


    if( left + 1 == right ){
        u[left] = 1;
        return true;
    }

    int mid = left + (right-left)/2;


    bool fl=false, fr = false;

    int v0, v1, u0, u1;
    if( mtr.randInt() % 2){
        v0 = left; v1 = mid;
        u0 = mid ; u1 = right;
    }
    else{
        u0 = left; u1 = mid;
        v0 = mid ; v1 = right;
    }

    // left
    memcpy( y, x, no_input_bytes);
    for( int i=v0; i<v1; i++)
        y[i] ^= differences[0][i];
    write_bytes_to_binary_file(TMP_INPUT, y, no_input_bytes);
    if( execution_differs_from_primary() )
        fl = true;
    else
        memset( u + v0, 0, v1-v0 );

    // right
    memcpy( y, x, no_input_bytes);
    for( int i=u0; i<u1; i++)
        y[i] ^= differences[0][i];
    write_bytes_to_binary_file(TMP_INPUT, y, no_input_bytes);
    if( execution_differs_from_primary() )
        fr = true;
    else
        memset( u + u0, 0, u1-u0 );

    if( fl )
        detect_range_important_bytes( v0, v1 );
    if( fr )
        detect_range_important_bytes( u0, u1 );

    
    return true;

}

bool branch_inference_detect_important_bytes( string original_input, vector <int> &ib, 
        float rtime_max_slow_inference, uint32_t *rorigin_values, float execs_per_sec )
{
    // read the original input
    if( !prepare_tmp_input( original_input )) return false;

    x = read_binary_file(TMP_INPUT, no_input_bytes);
    y = (unsigned char *)malloc(no_input_bytes);
    u = (unsigned char *)malloc(no_input_bytes);

    origin_values = rorigin_values;


    memset( u, 0, no_input_bytes);
    time_max_slow_inference = rtime_max_slow_inference;
    begin_time_detect_important = GET_TIMER();

    float tot_execs = execs_per_sec * rtime_max_slow_inference;
    float min_size_set1 = execs_per_sec > 0 ? (no_input_bytes / ( 2.0 * tot_execs * 0.5 )) : 8;
    min_size_set1 = mmax( min_size_set1, 8 );
    min_size_set1 = mmin( min_size_set1, 32 );
    //printf( KRED "EXEC/sec: %f  : %f  \n" KNRM , tot_execs , min_size_set1 );
    

    important_size_set_1 = 32;
    important_size_stop = 16;

    important_size_stop = min_size_set1;
    important_size_set_1 = 2 * min_size_set1;

    detect_range_important_bytes( 0, no_input_bytes );


    for( int i=0; i<no_input_bytes; i++)
        if( u[i] ) ib.push_back( i );

    free(x);
    free(y);
    free(u);

    return true;
}





//
//
// UNMUTABLE
//
//


bool execution_differs_percentage()
{


    if( 0 == seen.size() ){
        last_comp_perct = 100.0;
        return true;
    }


    if( !use_forced ) run_one_fork( RUN_GETTER );
    else run_one_fork( RUN_FORCER );


    map <int,int> counters;
    int tot_found=0, tot_notfound=0;

    int cnt =0;
    for(int i=0; i< __new_branch_index[0]; i++){

        if( BRANCH_IS_UNIMPORTANT( branches_operations[i] ) ) continue;

        auto zz = counters.find( branches_trace[i] );
        if( zz == counters.end() ) counters.insert( { branches_trace[i], 0} );
        else zz -> second ++;
        zz = counters.find( branches_trace[i] ) ;  

        branch_address_pair ba = get_ba( zz->first , zz->second ) ;

        tot_found += seen.find( ba ) != seen.end();
        tot_notfound += seen.find( ba ) == seen.end();

    }

    last_comp_perct = 1.0 * tot_found / seen.size();

    return last_comp_perct < seen_perct;
}



bool detect_range_unmutable_bytes( int left, int right )
{
    if( TIME_DIFFERENCE_SECONDS( begin_time_unmutable )  >  time_max_slow_inference ){
        printf("timeout\n"); 
        return false;
    }


    if( right - left < mmin( no_input_bytes / 100, 32) ){ 
        return true;
    }
    if( left + 1 >= right ){
        return true;
    }


    int mid = left + (right-left)/2;


    bool fl=false, fr = false;
    float p1 = 0, p2 = 0;

    int v0, v1, u0, u1;
    if( mtr.randInt() % 2){
        v0 = left; v1 = mid;
        u0 = mid ; u1 = right;
    }
    else{
        u0 = left; u1 = mid;
        v0 = mid ; v1 = right;
    }

    // left
    memcpy( y, x, no_input_bytes);
    for( int i=v0; i<v1; i++)
        y[i] ^= differences[0][i];
    write_bytes_to_binary_file(TMP_INPUT, y, no_input_bytes);
    if( execution_differs_percentage() )
        fl = true;
    else
        memset( u + v0, 0, v1-v0 );
    p1 = last_comp_perct;

    // right
    memcpy( y, x, no_input_bytes);
    for( int i=u0; i<u1; i++)
        y[i] ^= differences[0][i];
    write_bytes_to_binary_file(TMP_INPUT, y, no_input_bytes);
    if( execution_differs_percentage() )
        fr = true;
    else
        memset( u + u0, 0, u1-u0 );

    p2 = last_comp_perct;


    if( fl )
        detect_range_unmutable_bytes( v0, v1 );
    if( fr )
        detect_range_unmutable_bytes( u0, u1 );

    
    return true;

}



bool branch_inference_detect_unmutable_bytes( 
        string original_input, vector <int> &unmut, 
        float rtime_max_slow_inference,
        float percentage, bool forced_inference )
{
    // read the original input
    if( !prepare_tmp_input( original_input )) return false;
    
    x = read_binary_file(TMP_INPUT, no_input_bytes);
    y = (byte *)malloc(no_input_bytes);
    u = (byte *)malloc(no_input_bytes);
    
    memset( u, 1, no_input_bytes);
    time_max_slow_inference = rtime_max_slow_inference;
    begin_time_unmutable = GET_TIMER();

    seen_perct = percentage;
    use_forced = forced_inference;

    if( !use_forced ) run_one_fork( RUN_GETTER );
    else run_one_fork( RUN_FORCER );

    map <int,int> counters;
    seen.clear();
    for( int i=0; i< store_branches_tot; i++){

        if( BRANCH_IS_UNIMPORTANT( store_branches_opera[i] ) ) continue;

        auto zz = counters.find( store_branches_trace[i] );
        if( zz == counters.end() ) counters.insert( { store_branches_trace[i], 0} );
        else zz -> second ++;
        zz = counters.find( store_branches_trace[i] ) ;  

        seen.insert( get_ba( zz->first , zz->second ) );

    }


    detect_range_unmutable_bytes( 0, no_input_bytes );


    unmut.clear();
    for( int i=0; i<no_input_bytes; i++)
        if( u[i] ) unmut.push_back( i );

    
    printf( KINFO "Unmutable inferred: %ld \n" KNRM, unmut.size() );


    free(x);
    free(y);
    free(u);

    return true;
}
















