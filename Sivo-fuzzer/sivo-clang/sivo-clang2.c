

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

// parts of the code below is based on AFL's code
/*
   american fuzzy lop - LLVM-mode wrapper for clang
   ------------------------------------------------

   Written by Laszlo Szekeres <lszekeres@google.com> and
              Michal Zalewski <lcamtuf@google.com>

   LLVM integration design comes from Laszlo Szekeres.

   Copyright 2015, 2016 Google Inc. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   This program is a drop-in replacement for clang, similar in most respects
   to ../afl-gcc. It tries to figure out compilation mode, adds a bunch
   of flags, and then calls the real compiler.

 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/limits.h>
#include "config_llvm.h"



void show_how_to_compile()
{
      printf("\nFor compilation, first set environment variables "ENV_VAR_CALL_PATH" and "ENV_VAR_BINARY_NAME" and then compile twice, with both sivo-clang1 and sivo-clang2.\n");
      printf( "\t" ENV_VAR_CALL_PATH " is the folder where the compiled binary will be produced (make sure it exists!) \n");
      printf( "\t" ENV_VAR_BINARY_NAME " is the name of the binary that will be fuzzed \n");      
      printf("The end result of the compilation (among others) should be two binaries (BINNAME-1,BINAME-2) and 3 additional files (llvm-ifs-BINNAME,llvm-switches-BINNAME,llvm-cfg-BINNAME). All 5 files should be in the same directory.\n\n");

      printf( "To compile a single file ~/testprog/test.cpp into binary btest use\n"
            "\texport " ENV_VAR_CALL_PATH "=\"~/testprog\"; \n"
            "\texport " ENV_VAR_BINARY_NAME "=\"btest\"; \n"
            "\t%s/sivo-clang1 ~/testprog/test.cpp -o ~/testprog/btest-1\n"
            "\t%s/sivo-clang2 ~/testprog/test.cpp -o ~/testprog/btest-2\n\n"
            "To compile configure script (that produces binary mbin) use\n"        
            "\texport " ENV_VAR_CALL_PATH "=\"~/path\"; \n"
            "\texport " ENV_VAR_BINARY_NAME "=\"mbin\"; \n"
            "\t./configure      \\ \n"
            "\tCC=%s/sivo-clang1     \\ \n"
            "\tCXX=%s/sivo-clang1++ \n"
            "\tmake install \n"
            "\tmv ~/path/mbin ~/path/mbin-1\n"
            "\t./configure      \\ \n"
            "\tCC=%s/sivo-clang2     \\ \n"
            "\tCXX=%s/sivo-clang2++ \n"
            "\tmake install \n"
            "\tmv ~/path/mbin ~/path/mbin-2\n",
            SIVO_PATH, SIVO_PATH, SIVO_PATH, SIVO_PATH, SIVO_PATH, SIVO_PATH);

}

int main(int argc, char** argv) {

    uint8_t** cc_params;              
    uint32_t  cc_par_cnt = 1;         

    if (argc < 2) {

      printf( KERR "\nNo inputs provided for compilation \n" KNRM );
      show_how_to_compile();
      exit(1);
    }

    if( !getenv(ENV_VAR_BINARY_NAME) || !getenv(ENV_VAR_CALL_PATH) ){
      printf( KERR "\nSet env " ENV_VAR_CALL_PATH " and " ENV_VAR_BINARY_NAME "\n" KNRM );
      show_how_to_compile(  );
      exit(1);
    }

    DIR* dir = opendir( getenv(ENV_VAR_CALL_PATH) );
    if (!dir) {
      printf( KERR "\nDirectory %s does not exist. Make sure to use absolute path!\n" KNRM, getenv(ENV_VAR_CALL_PATH) );
      exit(1);
    } 
    closedir(dir);



    uint8_t x_set = 0;
    uint8_t *name;

    cc_params = (uint8_t **)malloc((argc + 128) * sizeof(uint8_t*));

    name = strrchr(argv[0], '/');
    if (!name) name = argv[0]; else name++;

    if (!strcmp(name, "sivo-clang1++") || !strcmp(name, "sivo-clang2++")  ) {
      cc_params[0] = (uint8_t*)"clang++";
    } else {
      cc_params[0] = (uint8_t*)"clang";
    }

    uint8_t *forkserver = (uint8_t *)malloc( strlen(SIVO_PATH) + 100 );
    sprintf(forkserver,"%s/llvm/forkserver.o",SIVO_PATH);
    cc_params[cc_par_cnt++] = forkserver;

    cc_params[cc_par_cnt++] = "-Xclang";
    cc_params[cc_par_cnt++] = "-load";
    cc_params[cc_par_cnt++] = "-Xclang";
    uint8_t *pass_branches = (uint8_t *)malloc( strlen(SIVO_PATH) + 100 );
    sprintf(pass_branches,"%s/llvm/pass-branches.o",SIVO_PATH);
    cc_params[cc_par_cnt++] = pass_branches;

    cc_params[cc_par_cnt++] = "-Qunused-arguments";

    while (--argc) {
      uint8_t* cur = *(++argv);

      if (!strcmp(cur, "-x")) x_set = 1;

      if (!strcmp(cur, "-Wl,-z,defs") ||
          !strcmp(cur, "-Wl,--no-undefined")) continue;

      cc_params[cc_par_cnt++] = cur;

    }

    cc_params[cc_par_cnt++] = "-g";
    cc_params[cc_par_cnt++] = "-O3";
    cc_params[cc_par_cnt++] = "-funroll-loops";

    cc_params[cc_par_cnt] = NULL;






    execvp(cc_params[0], (char**)cc_params);


    return 0;

}
