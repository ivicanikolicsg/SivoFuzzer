#include <stdio.h>
#include <signal.h>
#include "some_funcs.h"

void f1( int v )
{
    if(  ( (v * 117 ) % (1<<20) )  == 1 ){
        printf("reached tricky crash condition 1\n");
        raise(SIGSEGV);
    }

    if(  ( (v * 79 ) % (1<<20) )  == 10 ){
        printf("reached tricky crash condition 2\n");
        raise(SIGSEGV);
    }
}

int f2( int v )
{
    switch( (v*19) % (1<<20)){
        case 1<<14  : printf("reached 0\n"); return 1;
        case 1<<15  : printf("reached 7\n"); return 7;
        default     : printf("default\n");   return 0;
    }

}