
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


static int no_execs;

static byte *y;
static int lenx;
static Time mbegin_time;
static bool can_use_freq;
static vector <bool> multiplier;

vector < pair < string, vector<float> > > tcc;
int no_add_scores=0;
map < string, int > sampled_tc_to_times;
vector <int> sampled_times;
vector < vector<float> > individual_scores;

vector  < node_mab > mab_combiner_strategy;
vector  < node_mab > mab_combiner_number;
vector  < node_mab > mab_combiner_type;

extern double freqs_insert[256], freqs_put[256];
extern vector <float> w_left, w_right, w_insert, w_put;

#define RANGE  16 
#define OFFSET 0 



bool choose_0(  byte *x0, int len_0, int min_len, uint32_t &cut )
{
    cut = 0;
    if ( min_len == 0 ) return false;

    int ind0 = discrete_distribution_sample( w_insert );
    if( ind0 < 0 ) return false;
    for( int i= 1 + (mtr.randInt() % min_len) ; i<  min_len ; i++){

        if( x0[i] == ind0 ){
            cut = i;
            return true;
        }
    }

    return false;
    
}

bool choose_1(  byte *x0, int len_0,  byte *x1, int len_1, uint32_t &cut0, uint32_t &cut1 )
{
    cut0 = cut1 = 0;
    if( len_0 == 0 || len_1 == 0 ) return false;

    bool f0 = false;
    int ind0 = discrete_distribution_sample( w_insert );
    if( ind0 < 0 ) return false;

    for( int i= mtr.randInt() % len_0  ; i<  len_0 ; i++){

        if( x0[i] == ind0  ){
            cut0 = i;
            f0 = true;
            break;
        }
    }
    if( !f0 ) return false;
    

    bool f1 = false;
    int ind1 = ind0;
    if( ind1 < 0 ) return false;    
    for( int i= mtr.randInt() % len_1 ; i<  len_1 ; i++){

        if( x1[i] == ind1 ){
            cut1 = i;
            f1 = true;;
            break;
        }
    }
    if( !f1 )return false;

    return true;
}



void init_fuzzer_combiner( int init_no_execs, byte *init_x, int init_lenx, float max_node_score_time, Time begin_time  )
{

    lenx        = init_lenx;
    y           = (byte *)malloc( init_lenx );
    memcpy( y, init_x, lenx );
    no_execs    = init_no_execs;
    mbegin_time = begin_time;


    testcase_get_testcases_(tcc);
    individual_scores.clear();
    no_add_scores = tcc.size() > 0 ? tcc[0].second.size() : 0;
    vector <float> maxx;
    if( tcc.size() > 0 )
        for( int i=0; i< no_add_scores; i++){
            individual_scores.push_back( vector<float>() );
            maxx.push_back( 0 );
        }

    sampled_times.clear();
    for( int i=0; i< tcc.size(); i++){
        for( int j=0; j< no_add_scores ; j++){
            individual_scores[j].push_back( tcc[i].second[j] );
            maxx[j] = mmax( maxx[j], tcc[i].second[j] );
        }

        auto it = sampled_tc_to_times.find( tcc[i].first );
        if( it == sampled_tc_to_times.end() ){
            sampled_tc_to_times.insert ({ tcc[i].first, 0 } );
            sampled_times.push_back( 0 );
        }
        else{
            sampled_times.push_back( it->second );
        }
    }


    for( int i=0; i< tcc.size(); i++){
        for( int j=0; j< no_add_scores ; j++)
            if( maxx[j] > 0 )   
                individual_scores[j][i] = pow( 2, 10 * individual_scores[j][i] / maxx[j] ) / (1.0 + sampled_times[i] ) ;
    }


    multiplier.clear();
    for( int i=0; i<mab_combiner_number.size(); i++){
        multiplier.push_back( true  );
    }

    mab_recompute( mab_combiner_type );
    mab_recompute( mab_combiner_number );
    mab_recompute( mab_combiner_strategy );
    

    float aux;
    bool b ;
    can_use_freq = true;
    COMP_W( freqs_insert,   w_insert  , aux, b); can_use_freq = can_use_freq && b ; 

    can_use_freq = can_use_freq && USE_LEARNING;

}

void fini_fuzzer_combiner()
{
    free( y );


    for( int i=0; i< tcc.size(); i++){
        auto it = sampled_tc_to_times.find( tcc[i].first );
        if( it == sampled_tc_to_times.end() ){
            printf( KERR "Cannot find smapled tc to times: %s  : %ld \n" KNRM, tcc[i].first.c_str(), sampled_tc_to_times.size() );
            exit(2);
        }

        it->second = sampled_times[i];

    }

}


bool can_run_combiner( )
{
    return tcc.size() > 0 && lenx < MAX_LENGTH_PROPOSE;
}


int exec_block_combiner( float min_running_time  )
{
    int found_new = 0;
    uint32_t ind1;
    int trials;
    set <uint32_t> indices;
    int len0;
    byte *x0 = NULL;
    int found_one_solution = 0;


    if( tcc.size() == 0){
        printf( KINFO "Empty combiner set\n" KNRM );
        return 0;
    }

    
    for( int e=0; e<no_execs; e++){

        if( TIME_DIFFERENCE_SECONDS(mbegin_time) >= min_running_time ) break; 


        vector < vector< vector<uint8_t> > > cho;

        vector <bool>res_mult { true, true, can_use_freq , can_use_freq  };
        int combiner_type        = mab_sample( mab_combiner_type, mab_combiner_type.size(), false, res_mult );
        int use_smart            = combiner_type % 2;

        int combiner_number      = mab_sample( mab_combiner_number, mab_combiner_number.size(), false, multiplier);

        vector <bool>res_str;
        int combiner_strategy    = mab_sample( mab_combiner_strategy, mab_combiner_strategy.size(), false, res_str );

        fuzzer_current_main     = use_smart ? SOL_TYPE_COMBINERSMART : SOL_TYPE_COMBINER;
        START_TIMER( begin_time_one_combiner );

        found_one_solution      = 0;
        x0                      = NULL ; 
        indices.clear(); 
        for( int k=0; k < 2 + combiner_number; k++){
            
            trials = 0;
            do 
            {
                ind1 = discrete_distribution_sample( individual_scores[combiner_strategy] );
                trials ++;
                
            }
            while( indices.find(ind1) != indices.end() && trials < 1000 );


            if( ind1 < 0 || trials >= 1000) continue;

            sampled_times[ ind1 ] ++;


            string f1 = testcase_get_filename( tcc[ind1].first );
            if( f1.size() == 0 ) continue;

            indices.insert( ind1 );

            if( 1 == indices.size() ){
                                 
                len0    = lenx;
                x0 = (byte *)malloc(len0 );
                memcpy( x0, y, len0 );
                
                continue;
            }

            int len1;
            byte * x1 = read_binary_file( f1 , len1 );


            uint32_t  cut0 = len0 > 0 ? (mtr.randInt() % len0 ) : 0;
            uint32_t  cut1 = len1 > 0 ? (mtr.randInt() % len1 ) : 0;


            uint32_t newcut0, newcut1;
            bool U1 = use_smart && len0 > 0 && len1 > 0 && 
                choose_1( x0, len0, x1, len1, newcut0, newcut1 );
            
            if( U1 )    
            {
                cut0 = newcut0 ;
                cut1 = newcut1 ;


                int offset = - OFFSET + ( mtr.randInt() % (2*OFFSET+1) );
                if( cut0 + offset >= 0 && cut0 + offset < len0 )
                    cut0 = newcut0 + offset;

                offset = - OFFSET + ( mtr.randInt() % (2*OFFSET+1) );
                if( cut1 + offset >= 0 && cut1 + offset < len1 )
                    cut1 = newcut1 + offset;
            }



            vector <vector<uint8_t> > bc;
            vector <uint8_t> onc;
            onc.push_back ( 1 );
            bc.push_back( onc );
            onc.clear();
            for( int i=cut0-RANGE; i< cut0 ; i++)
                onc.push_back( i >= 0 && i < len0 ? x0[ i ] : 0 );
            bc.push_back( onc );
            onc.clear();
            for( int i=cut1 ; i< cut1+RANGE; i++)
                onc.push_back( i >= 0 && i < len1 ? x1[ i ] : 0 );
            bc.push_back( onc );
            cho.push_back( bc );


            byte *y = (byte *)malloc( cut0 + (len1 - cut1 ) );
            memcpy( y, x0, cut0 );
            memcpy( y + cut0 , x1+ cut1 , len1-cut1 );

            free( x0 );
            x0 = y;
            len0 = cut0 + (len1 - cut1 );

            free( x1 );

            if( len0 > MAX_LENGTH_PROPOSE ) break;

        }

        if( NULL != x0 ){

            write_bytes_to_binary_file(TMP_INPUT, x0, len0  );
            int good_exec           = run_one_fork( RUN_TRACER );
            if( good_exec && execution_edge_coverage_update() )
            {

                check_and_add_solution( TMP_INPUT, 
                    use_smart ? SOL_TYPE_COMBINERSMART :SOL_TYPE_COMBINER , "" );

                found_new ++;
                found_new_cov += 2==glob_found_different;
                found_one_solution = 1;
                
                
                for( int i=0; i<cho.size(); i++){


                    if( 3 == cho[i].size() && 1 == cho[i][0][0] )
                    {
                        for( int j=0; j<cho[i][1].size(); j++)
                            freqs_insert[ cho[i][1][j] ] += 1.0 ;

                        for( int j=0; j<cho[i][2].size(); j++)
                            freqs_insert[ cho[i][2][j] ] += 1.0  ;
                    }
                }

                
            }

            free( x0 );
        }

        auto timf = TIME_DIFFERENCE_SECONDS( begin_time_one_combiner );
        mab_update( mab_combiner_strategy ,   combiner_strategy,   found_one_solution, timf );
        mab_update( mab_combiner_type,        combiner_type,       found_one_solution, timf );
        mab_update( mab_combiner_number,      combiner_number,     found_one_solution, timf );


    }


    return found_new;
}


