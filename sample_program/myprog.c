#include <stdio.h>
#include "some_funcs.h"

int main(int argc, char *argv[])
{
    if( argc < 2 ){
        printf("Provide input file\n");
        return 0;
    }
    
    FILE *input_file = fopen( argv[1], "r");
    if( NULL == input_file){
        printf("Cannot open input file %s \n", argv[1] );
        return 0;
    }

    int some_hash = 0;
    int c;
    while ((c = getc(input_file)) != EOF ){
        some_hash = (some_hash << 8 ) ^ (some_hash >> 24)  ^ c;
        f1( some_hash );
        some_hash += f2( some_hash );
    }
    
    fclose(input_file);
        
    return 0;
}

