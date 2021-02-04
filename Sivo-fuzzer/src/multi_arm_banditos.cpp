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

#include "multi_arm_banditos.h"
#include "misc.h"
#include "mab_softmax.h"


//
//
// Multi-armed bandits functions
//
//


// sample randomly (used in exploration)
int sample_randomly( vector < node_mab > &x, int number, bool print_me, vector <bool> &use_choices )
{

    if( print_me )
        printf("Sampling randomly \n");

    vector <uint32_t> indices;
    for( int i=0; i< mmin(number, x.size()); i++){
        if( i >= x.size() ) break;
        if( use_choices.size() > i && !use_choices[i]  ) continue;
        indices.push_back( i );
    }
    if( indices.size() > 0 )
        return indices[ mtr.randInt() % indices.size() ];

    return -1;
}


// sample according to MAB
int mab_sample( vector < node_mab > &x, int number, bool print_me, vector <bool> &use )
{
    // make sure there is some available selections (use) 
    if( use.size() >= number){
        bool onegood = false;
        for( int i=0; i< mmin(number, x.size()); i++)
            if( use[i] ) {
                onegood = true;
                break;
            }
        if( !onegood ){
            return -1;
        }
    }


    // the first SAMPLE_RANDOMLY_UNTIL_ROUND rounds sample uniformly at random
    if( glob_iter < SAMPLE_RANDOMLY_UNTIL_ROUND || 
        SAMPLE_RANDOMLY_THIS_ROUND || 
        !SAMPLE_NOT_RANDOMLY && SAMPLE_RANDOMLY_THIS_ROUND_NO_UPDATE 
    )
        return sample_randomly( x, number, print_me, use );


    return multi_arm_bandit_softmax ( x, number, print_me, use ); 
}


// update MAB according to newly produced coverage and time to produce it
void mab_update( vector < node_mab > &x, int index, float add_cover, float timtot )
{

    // ignore the first SAMPLE_RANDOMLY_UNTIL_ROUND iterations
    if( glob_iter < SAMPLE_RANDOMLY_UNTIL_ROUND  || SAMPLE_RANDOMLY_THIS_ROUND_NO_UPDATE  ) return;

    // sanity checks
    if( index < 0 || index >= x.size() || x.size() == 0 ) return;
    if( add_cover < 0 || timtot < 0 ){
        printf( KRED "Impossible coverage/time: %f %f\n" KNRM, add_cover, timtot );
        exit(2);
    }

    // general updates
    x[index].updates            += 1;
    for( int i=0; i< x.size(); i++){
        x[i].tot_updates ++;
    }
    x[index].new_num            += add_cover; 
    x[index].new_denom          += timtot  ;
    x[index].new_updates        += 1;


    x[index].running_num        += add_cover ; 
    x[index].running_denom      += timtot  ;


    // undiscounted
    x[index].drunning_num        += add_cover ; 
    x[index].drunning_denom      += timtot  ;
    x[index].dupdates            += 1;
    
}


// This is basically only to be able to accumulate all coverage produced by different fuzzers during one fuzzer() call
void mab_recompute( vector < node_mab > &x )
{
    
    for(int i=0; i<x.size(); i++){

        x[i].new_num            = 0;
        x[i].new_denom          = 0;
        x[i].new_updates        = 0;
    }

}


// Initializations (different types, see below)
node_mab mab_init()
{

    node_mab x;

    x.tot_updates       = 1;
    x.updates           = 1;
    x.last_update       = 0;

    x.running_num       = 10.0; 
    x.running_denom     = 1.0; 
    
    x.new_num           = 0;
    x.new_denom         = 0;
    x.new_updates       = 0;

    x.gamma             = Gamma;
    x.discount_times    = &discount_times_other;


    x.drunning_num       = x.running_num; 
    x.drunning_denom     = x.running_denom;
    x.dupdates           = x.updates;
    x.stop_disc          = false;

    x.allow_small        = false;

    return x;

}

node_mab mab_init( float different_gamma )
{
    node_mab n = mab_init();
    n.gamma = different_gamma;

    return n;
}


node_mab mab_init_value( float a, float b)
{
    node_mab mi = mab_init();

    mi.running_num      = a;
    mi.running_denom    = b;


    return mi;
}


node_mab mab_init_allow_small( int all_sm)
{
    node_mab mi = mab_init();

    mi.allow_small      = all_sm;

    return mi;
}


// Discounts for MAB 
void mab_discount_add( vector < node_mab > &x, int *discount_times_addr )
{
    for( int i=0; i<x.size(); i++){
        x[i].discount_times = discount_times_addr;
        all_mabs.push_back( &(x[i] ) );
    }

}

void mab_discount_all_mabs()
{
    
    if( glob_iter < SAMPLE_RANDOMLY_UNTIL_ROUND   || SAMPLE_RANDOMLY_THIS_ROUND_NO_UPDATE   ) return;


    for( int i=0; i < all_mabs.size(); i++){

        if( *(all_mabs[i]->discount_times) == 0 ) continue;

        all_mabs[i]->running_num        *= pow( all_mabs[i]->gamma, *(all_mabs[i]->discount_times)); 
        all_mabs[i]->running_denom      *= pow( all_mabs[i]->gamma, *(all_mabs[i]->discount_times)); 
        all_mabs[i]->updates            *= pow( all_mabs[i]->gamma, *(all_mabs[i]->discount_times)); 

    }
}




int mab_get_best_index(  vector < node_mab > &x  )
{
    int best_index = -1;
    for( int i=0; i< x.size(); i++){
        if( x[i].running_denom > 0 && 
            ( best_index < 0 || x[i].running_num / x[i].running_denom > x[best_index].running_num / x[best_index].running_denom ))
                best_index = i;
    }

    return best_index;
}

node_mab mab_init_best_value(   vector < node_mab > &x   )
{

    int best_index = mab_get_best_index( x );
    if( best_index < 0  ) return mab_init();

    return mab_init_value( x[best_index].running_num, x[best_index].running_denom );
}





