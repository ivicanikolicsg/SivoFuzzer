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




/*
 *
 * 
 * Bolzmann exploration (softmax) 
 * 
 *  
 */

int multi_arm_bandit_softmax( vector < node_mab > &x, int number, bool print_me, vector <bool> &use_choices )
{
    int index = -1;
    float cur_best = -1;

    float max_avg = 0;
    float redcoef = 1.0;
    int ACTIVE = 0;
    for( int i=0; i< mmin(number, x.size()); i++){

        if( use_choices.size() > i && !use_choices[i]  ) continue;

        ACTIVE ++;

        if( x[i].running_denom > 0 )
            max_avg = mmax( max_avg, x[i].running_num / x[i].running_denom  );
    }
    if( max_avg > 0 )
        redcoef = 1.0 / ( 2.0 * max_avg);


    if( redcoef > 1 << 30 ) {
        for( int i=0; i<x.size(); i++){
            x[i].gamma = 1.0;

            x[i].running_num = 1.0;
            x[i].running_denom = 1.0;
            x[i].updates = 1.0;
        }

    }


    float minval = -1, maxval = -1;
    for( int i=0; i< mmin(number, x.size()); i++){
        if( i >= x.size() ) break;
        if( use_choices.size() > i && !use_choices[i]  ) continue;
        if( minval < 0 || minval > redcoef *  x[i].running_num / x[i].running_denom  ) 
            minval = redcoef *  x[i].running_num / x[i].running_denom ;
        if( maxval < 0 || maxval < redcoef *  x[i].running_num / x[i].running_denom  ) 
            maxval = redcoef *  x[i].running_num / x[i].running_denom ;
    }
    float C = 1.0, Co = 1.0;
    if( !(minval < 0 || maxval <= 0 || minval >= maxval) ){
        C = maxval - minval;
        Co = C;
    }
    

    if( print_me){
        printf("Mab[%d][%.1f][%.6f][%.5f]: ", number, redcoef, C, x[0].gamma );
        
        if( use_choices.size() >= number )
            for( int i=0; i< number; i++)
                printf("%d", (int)!!use_choices[i] );
        printf(" : ");
        for( int i=0; i< number; i++){
            if( i >= x.size() ) break;
            //printf("%4ld ", x[i].updates );
            printf("%.1f ", x[i].running_denom );
        }
        printf(" : ");
    }


    // pick index if not sampled 
    vector <uint32_t> indices;
    for( int i=0; i< mmin(number, x.size()); i++){

        if( use_choices.size() > i && !use_choices[i]  ) continue;
        //if( x[i].updates  <= 0  )
        if( x[i].running_denom  <= 0  )
            indices.push_back( i );
    }
    if( indices.size() > 0 ){
        int index = indices[ mtr.randInt() % indices.size() ];
        if( print_me) printf(" unsamp %d \n", index );fflush(stdout);
        return index;
    }


    vector <float> w;
    float cur, s = 0, beta;
    bool found_one = false;
    for( int i=0; i< mmin(number, x.size()); i++){
        if( i >= x.size() ) break;
        if( use_choices.size() > i && !use_choices[i]  ){ 
            w.push_back( 0 );
            continue;
        }

        found_one = true;

        beta    = 4 + 2 * ACTIVE ;
        if( x[i].allow_small > 0 )
            beta = x[i].allow_small;
        
        cur     = beta * ( redcoef * x[i].running_num / x[i].running_denom );
        cur     = pow( 2, cur );

        s += cur;
        w.push_back( cur );

    }


    if( !found_one ){
        index = -1;
    }    
    else{

        for( int i=0; i<w.size(); i++){
           
            if( use_choices.size() > i && !use_choices[i]  )
                w[i] = 0;
            
            else{
                if( print_me ){
                    printf( "[%d]%.5f %.1f ", i, redcoef * x[i].running_num/x[i].running_denom, w[i] );
                }
            }

        }

        index = discrete_distribution_sample( w );
    }


    if( print_me ) {
        printf("  ::  %d  \n", index );
    }

    return index;
}




