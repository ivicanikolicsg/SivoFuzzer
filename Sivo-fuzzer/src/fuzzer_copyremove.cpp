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
static Time mbegin_time;

vector< node_mab > mab_copyremove_mode;
vector< node_mab > mab_copyremove_lognumcutpaste;
vector< node_mab > mab_copyremove_loglength;


static bool can_use_divs;
static int iter_no = 0;
static vector <bool> multiplier;
static vector <bool> multiplier_loglen;

#define RANGE  8 
#define OFFSET 0 

double freqs_left[256], freqs_right[256];
double freqs_insert[256], freqs_put[256];
vector <float> w_left, w_right, w_insert, w_put;


bool choose_one_smart(int lenx, int &v0, int &v1, int max_diff_len)
{
    if( all_found_solutions.size() == 0 ) return false;

    int trials = 1000;
    do{
        int ind0 = mtr.randInt() % all_found_solutions.size();
        int ind1 = mtr.randInt() % all_found_solutions.size();
        if( all_found_solutions[ ind0 ].size() && all_found_solutions[ ind1 ].size() ){
            v0 = all_found_solutions[ ind0 ].begin()->first;
            v1 = all_found_solutions[ ind1 ].begin()->first;

            if( v0 > v1 ){
                int tmp = v0;
                v0 = v1;
                v1 = tmp;
            }

            if( v0 < lenx && v1 < lenx && v0 < v1  && v1 - v0 <= max_diff_len ) break;
        }

    }while( --trials > 0);

    if( v0 < lenx && v1 < lenx && v0 < v1 ) return true;

    return false;
}

bool choose_0( byte *y, int len_y, int &v0, int &v1 )
{
    v0 = v1 = 0;

    if( len_y == 0) return false;

    int ind0, ind1;
    ind0 = discrete_distribution_sample( w_left );
    ind1 = discrete_distribution_sample( w_right );
    if( ind0 < 0 || ind1 < 0 ) return false;

    bool f0 = false;  
    int target_point = mtr.randInt() % len_y;  
    for( int i=target_point; i<len_y; i++){
        if( y[i] == ind0 ){
            v0 = i;
            f0 = true;
            break;
        }
    }
    if( !f0 )
        for( int i=target_point; i>=0; i--){
            if( y[i] == ind0 ){
                v0 = i;
                f0 = true;
                break;
            }
        }
    if( !f0 )
        return false;
    

    vector<uint32_t> ops1;
    for( int i=v0+1; i< mmin(v0 + FUZZ_COPYREMOVE_CHECK_CHOOSE_LEN, len_y) ; i++){
        if( y[i] == ind1  ){
            ops1.push_back( i ) ;
        }
    }
    if(ops1.size() <= 1) return false;
    v1 = ops1[ mtr.randInt() % ops1.size() ];

    return true;

}

bool choose_1( byte *y, int len_y, int &v0, int &v1,  int &v2 )
{
     v0 = v1 = v2 = 0;

    int ind0, ind1, ind2;
    ind0 = discrete_distribution_sample( w_left );
    ind1 = discrete_distribution_sample( w_right );
    ind2 = ind0;
    if( ind0 < 0 || ind1 < 0 || ind2 < 0 ) return false;

    bool f0 = false;
    for( int i=mtr.randInt() % len_y; i<len_y; i++){
        if( y[i] == ind0 ){
            v0 = i;
            f0 = true;
            break;
        }
    }
    if( !f0 )
        return false;
    

    vector<uint32_t> ops1;
    for( int i=v0+1; i<mmin(v0 + FUZZ_COPYREMOVE_CHECK_CHOOSE_LEN, len_y); i++){
        if( y[i] == ind1 ){
            ops1.push_back( i ) ;
        }

    }
    if(ops1.size() <= 1) return false;
    v1 = ops1[ mtr.randInt() % ops1.size() ];

    bool f2 = false;
    for( int i= mtr.randInt() % len_y; i<len_y; i++){
        if( y[i] == ind2  ){
            v2 = i;
            f2 = true;
            break;
        }
    }
    if( !f2 )
        return false;
    
    return true;
}


bool choose_2( byte *y, int len_y, int &v0, int &v1 )
{
    v0 = v1 = 0;

    int ind0, ind1;
    ind0 = discrete_distribution_sample( w_left );
    ind1 = discrete_distribution_sample( w_right );
    if( ind0 < 0 || ind1 < 0 ) return false;

    vector<uint32_t> ops0;
    v0 = -1;
    for( int i=mtr.randInt() % len_y; i<len_y; i++){
        if( y[i] == ind0 ){
            v0 = i;
            break;
        }
    }
    if( v0 < 0){
        v0 = 0;
        return false;
    }

    vector<uint32_t> ops1;
    for( int i=v0+1; i<mmin(v0 + FUZZ_COPYREMOVE_CHECK_CHOOSE_LEN, len_y); i++){
        if( y[i] == ind1  ){
            ops1.push_back( i ) ;
        }

    }
    if(ops1.size() <= 1) return false;
    v1 = ops1[ mtr.randInt() % ops1.size() ];

    return true;
}


void init_fuzzer_copyremove( int init_no_execs, byte *init_x, int init_lenx, int itern, Time begin_time )
{
    x           = init_x;
    lenx        = init_lenx;
    no_execs    = init_no_execs;
    iter_no     = itern;
    mbegin_time = begin_time;


    float aux;
    bool b;

    can_use_divs = true;

    COMP_W( freqs_left ,    w_left    , aux, b ); can_use_divs = can_use_divs && b ; 
    COMP_W( freqs_right,    w_right   , aux, b ); can_use_divs = can_use_divs && b ; 

    can_use_divs = can_use_divs && USE_LEARNING;


    multiplier_loglen.clear();
    for( int i=0; i<mab_copyremove_lognumcutpaste.size(); i++){
        multiplier_loglen.push_back( true );
    }
        
    mab_recompute( mab_copyremove_mode );
    mab_recompute( mab_copyremove_lognumcutpaste );
    mab_recompute( mab_copyremove_loglength );

}


bool can_run_fuzzer_copyremove( )
{
    return lenx >= 5;
}


int exec_block_fuzzer_copyremove( float min_running_time  )
{
    byte *y = NULL;
    int len_y;
    int found_new = 0;
    int remove_or_add;
    int posi_modifyat, posi_copyfrom;
    bool rand_or_real ;
    bool use_prev_solutions, use_divs;
    map <int,int> usedc;
    map <int,int> used_ar_p;
    int tot_used = 0;
    int v0, v1, v2, v3;


    for( int e=0; e<no_execs; e++){

        if( TIME_DIFFERENCE_SECONDS(mbegin_time) >= min_running_time ) break; 

        int no_copyremove_oper_log = mab_sample( mab_copyremove_lognumcutpaste, mab_copyremove_lognumcutpaste.size(), false, multiplier_loglen );

        int tot_copyremove_opers= 1<<(no_copyremove_oper_log);     
        START_TIMER( begin_time_one_copyremove );
        
        /* free memory allocated during last iteration first; similar check after for */
        if (y) {
            free(y);
            y = NULL;
        }

        /* guaranteed that we aren't losing reference to heap allocation */
        y = (byte *)malloc( lenx );
        len_y = lenx;       
        memcpy(y, x, len_y);

        vector <bool> multiplier_mode = { true, true, can_use_divs , all_found_solutions.size()>0 };
        int mode = mab_sample( mab_copyremove_mode, 4, false, multiplier_mode );

        rand_or_real        =   0 == mode ? 0 : 1 ; 
        use_divs            =   2 == mode;
        use_prev_solutions  =   3 == mode;


        fuzzer_current_main = SOL_TYPE_COPREM_RAND + mode;
        
        vector < vector< vector<uint8_t> > > cho;

        usedc.clear();
        used_ar_p.clear();
        tot_used = 0;
        for( int me=0; me < tot_copyremove_opers; me++){
            
            int chosen_len = 1;

            if (len_y >= 32 ){
                int chosen_log_len = mab_sample( mab_copyremove_loglength, int(log(len_y/8)/log(2)), false, multiplier );
                if( usedc.find( chosen_log_len ) == usedc.end() ) 
                    usedc.insert( {chosen_log_len, 0} );
                usedc.find( chosen_log_len )->second ++;
                tot_used ++;
                if( chosen_log_len > 0 )
                    chosen_len = (1<< (chosen_log_len - 1) )  + ( mtr.randInt() % (1<< (chosen_log_len) ) ) ;
            }

                        

            remove_or_add = mtr.randInt() % 2;


            if( len_y > MAX_LENGTH_PROPOSE )
                remove_or_add = 0;

            if( 0 == remove_or_add ){   // remove

                if (len_y <= 2 || len_y - chosen_len <= 0) break;
                posi_modifyat = mtr.randInt() % (len_y - chosen_len + 1);

                if( use_prev_solutions && choose_one_smart(len_y, v0, v1, chosen_len)  )
                {
                    chosen_len = v1 - v0;
                    posi_modifyat = v0;
                }
                if( use_divs && choose_0( y, len_y, v0, v1 ) ){

                    int newv0, newv1;
                    newv0 = newv1 = -1;
                    
                    int offset = - OFFSET + ( mtr.randInt() % (2*OFFSET+1) );
                    if( v0 + offset >= 0 && v0 + offset < len_y )
                        newv0 = v0 + offset;

                    offset = - OFFSET + ( mtr.randInt() % (2*OFFSET+1) );
                    if( v1 + offset >= 0 && v1 + offset < len_y && v1 > v0)
                        newv1 = v1 + offset;

                    if( newv0 >=0 && newv1 >= 0 && newv1 > newv0 ){
                        v0 = newv0;
                        v1 = newv1;
                    }
                    
                    chosen_len = v1 - v0;
                    posi_modifyat = v0;

                }

                vector <vector<uint8_t> > bc;
                vector <uint8_t> onc;
                onc.push_back ( 0 );
                bc.push_back( onc );
                onc.clear();
                for( int i=posi_modifyat-RANGE; i< posi_modifyat + min(RANGE, chosen_len); i++)
                    onc.push_back( i >= 0 && i < len_y ? y[ i ] : 0 );
                bc.push_back( onc );
                onc.clear();
                for( int i=posi_modifyat + mmax(1,chosen_len -RANGE); i< posi_modifyat + chosen_len  + RANGE; i++)
                    onc.push_back(  i >= 0 && i < len_y ? y[ i ] : 0 );
                bc.push_back( onc );
                cho.push_back( bc );
                

                memmove(y + posi_modifyat, y + posi_modifyat + chosen_len, len_y - (posi_modifyat+chosen_len) );
                len_y = len_y - chosen_len;

            }
            else{   // add

                // once in a blue moon choose very large length
                if( 0 == rand_or_real && 
                    1 == tot_copyremove_opers &&
                    !use_prev_solutions && 
                    iter_no > 10  &&  
                    0 == (mtr.randInt() % 100 ) ){
                    chosen_len = mtr.randInt() % MAX_LENGTH_PROPOSE;
                }

                if (len_y + chosen_len > MAX_TESTCASE_SIZE ) break; 

                posi_modifyat = mtr.randInt() % len_y;
                posi_copyfrom = mtr.randInt() % len_y;

                if(     use_prev_solutions 
                    &&  choose_one_smart(len_y, v0, v1, chosen_len)  
                    &&  choose_one_smart(len_y, v2, v3, 1<<30 ) )
                {
                    chosen_len = v1 - v0;
                    posi_copyfrom = v0;
                    posi_modifyat = v2;
                }

                if( use_divs &&  choose_1( y, len_y, v0, v1, v2) ){

                    int newv0, newv1;
                    newv0 = newv1 = -1;

                    int offset = - OFFSET + ( mtr.randInt() % (2*OFFSET+1) );
                    if( v0 + offset >= 0 && v0 + offset < len_y )
                        newv0 = v0 + offset;

                    offset = - OFFSET + ( mtr.randInt() % (2*OFFSET+1) );
                    if( v1 + offset >= 0 && v1 + offset < len_y )
                        newv1 = v1 + offset;

                    if( newv0 >=0 && newv1 >= 0 && newv1 > newv0 ){
                        v0 = newv0;
                        v1 = newv1;
                    }

                    offset = - OFFSET + ( mtr.randInt() % (2*OFFSET+1) );
                    if( v2 + offset >= 0 && v2 + offset < len_y )
                        v2 = v2 + offset;                    

                    chosen_len = v1 - v0;
                    posi_copyfrom = v0;
                    posi_modifyat = v2;
                }


                if( rand_or_real )
                {
                    vector <vector<uint8_t> > bc;
                    vector <uint8_t> onc;
                    onc.push_back ( 1 );
                    bc.push_back( onc );
                    onc.clear();
                    for( int i=posi_copyfrom-RANGE; i< posi_copyfrom + min(RANGE, chosen_len); i++)
                        onc.push_back( i >= 0 && i < len_y ? y[ i ] : 0 );
                    bc.push_back( onc );
                    onc.clear();
                    for( int i=posi_modifyat + mmax(1,chosen_len -RANGE); i< posi_modifyat + chosen_len  + RANGE; i++)
                        onc.push_back(  i >= 0 && i < len_y ? y[ i ] : 0 );
                    bc.push_back( onc );
                    onc.clear();
                    for( int i=posi_modifyat - RANGE; i< posi_modifyat  /* + RANGE */; i++)
                        onc.push_back(  i >= 0 && i < len_y ? y[ i ] : 0 );
                    bc.push_back( onc );
                    cho.push_back( bc );
                }

                // once in a blue moon, make a lot of copies of the same real byte sequence
                if( rand_or_real && 
                    !(mtr.randInt() % 50) && 
                    tot_copyremove_opers - me > 2 &&  
                    len_y + chosen_len * (tot_copyremove_opers - me) < MAX_LENGTH_PROPOSE
                ){
                    byte *tmp_y = (byte *) malloc( len_y + chosen_len * (tot_copyremove_opers - me) );
                    memcpy( tmp_y, y, posi_modifyat );
                    for( int ii=0; ii < tot_copyremove_opers - me; ii++)
                        memcpy( tmp_y + posi_modifyat + chosen_len * ii, y + posi_copyfrom, chosen_len );
                    memcpy( tmp_y + posi_modifyat + chosen_len * (tot_copyremove_opers - me), y + posi_modifyat, len_y - posi_modifyat );

                    free(y);
                    y = tmp_y;
                    len_y = len_y + chosen_len* (tot_copyremove_opers - me);

                    break;

                }

                // once in a blue moon, all bytes are the same
                bool all_same = !(mtr.randInt() % 50 );
                char chosen_byte;
                if( all_same )
                    chosen_byte = !rand_or_real ? one_rand_byte()  : y[ mtr.randInt() % len_y ]; 
                byte *tmp_y = (byte *)malloc(len_y + chosen_len);
                memcpy(tmp_y, y, posi_modifyat);
                for( int i=0; i<chosen_len; i++){
                    if( all_same )
                        tmp_y[posi_modifyat+i] = chosen_byte;
                    else
                        tmp_y[posi_modifyat+i] = (!rand_or_real) ? one_rand_byte()  : 
                            ( posi_copyfrom + i  < len_y ? y[posi_copyfrom + i ] : one_rand_byte() );
                }
                memcpy(tmp_y + posi_modifyat + chosen_len, y + posi_modifyat, len_y - posi_modifyat);


                
                if( !rand_or_real )
                {
                    vector <vector<uint8_t> > bc;
                    vector <uint8_t> onc;
                    onc.push_back ( 1 );
                    bc.push_back( onc );
                    onc.clear();
                    for( int i=posi_modifyat-RANGE; i< posi_modifyat + min(RANGE, chosen_len); i++)
                        onc.push_back( i >= 0 && i < len_y ? y[ i ] : 0 );
                    bc.push_back( onc );
                    onc.clear();
                    for( int i=posi_modifyat + mmax(1,chosen_len -RANGE); i< posi_modifyat + chosen_len  + RANGE; i++)
                        onc.push_back(  i >= 0 && i < len_y ? y[ i ] : 0 );
                    bc.push_back( onc );
                    onc.clear();
                    bc.push_back( onc );
                    cho.push_back( bc );
                }

                free(y);
                y = tmp_y;
                len_y = len_y + chosen_len;

            }

        }

        write_bytes_to_binary_file( TMP_INPUT, y, len_y  );

        int good_exec = run_one_fork( RUN_TRACER );
        int found_one_solution = 0;
        if( good_exec && execution_edge_coverage_update() )
        {

            check_and_add_solution( TMP_INPUT, SOL_TYPE_COPREM_RAND + mode , "" );
            
            found_new ++;
            found_new_cov += 2==glob_found_different;
            found_one_solution = 1;
            
            if( 0 != mode){

                for( int i=0; i<cho.size(); i++){
                    if( 3 == cho[i].size() && cho[i][0][0] == 0 )
                    {
                        for( int j=0; j<cho[i][1].size(); j++)
                            freqs_left[ cho[i][1][j] ] += 1.0 ;
 
                        for( int j=0; j<cho[i][2].size(); j++)
                            freqs_right[ cho[i][2][j] ] += 1.0  ;
                    }
                    if( 4 == cho[i].size() && cho[i][0][0] == 1 )
                    {
                        for( int j=0; j<cho[i][1].size(); j++)
                            freqs_left[ cho[i][1][j] ] += 1.0  ;
 
                        for( int j=0; j<cho[i][2].size(); j++)
                            freqs_right[ cho[i][2][j] ] += 1.0 ;

                        for( int j=0; j<cho[i][3].size(); j++)
                            freqs_left[ cho[i][3][j] ] += 1.0  ;
                    }

                    if( 5 == cho[i].size() && 2 == cho[i][0][0] )
                    {
                        for( int j=0; j<cho[i][1].size(); j++)
                            freqs_left[ cho[i][1][j] ] += 1.0 ;
 
                        for( int j=0; j<cho[i][2].size(); j++)
                            freqs_right[ cho[i][2][j] ] += 1.0  ;

                        for( int j=0; j<cho[i][3].size(); j++)
                            freqs_left[ cho[i][3][j] ] += 1.0   ;
 
                        for( int j=0; j<cho[i][4].size(); j++)
                            freqs_right[ cho[i][4][j] ] += 1.0  ;
                    }
                }
            }
        }

        auto timf = TIME_DIFFERENCE_SECONDS( begin_time_one_copyremove );

        mab_update( mab_copyremove_lognumcutpaste, no_copyremove_oper_log,  found_one_solution, timf );
        mab_update( mab_copyremove_mode, mode,  found_one_solution, timf );

        for( auto it=usedc.begin(); it!=usedc.end(); it++){
            mab_update( mab_copyremove_loglength, it->first,  found_one_solution, it->second /* timf */ );
        }


    }

    /* free final alocation */
    if (y) 
        free(y);


    return found_new;
}


void fini_fuzzer_copyremove()
{

}
