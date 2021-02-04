# SIVO: Refined gray-box fuzzer

The repository contains C++ implementation of gray-box fuzzer SIVO. 
The technical aspects of the fuzzer are described in [our paper](https://arxiv.org/pdf/xxx.pdf). 

## Installation and Dependencies

The installation contains makefile. This will build the fuzzer `sivo` and the dedicated compilers (`sivo-clang1` and `sivo-clang2`).
The code has been tested on Ubuntu 16.04 with LLVM 3.4.0 installed:

	apt-get install llvm-3.4-dev


## How it works

SIVO is a **gray**-box fuzzer. Gray means it needs to add extra code to the fuzzed program to track code coverage (and other stuff). This is done automatically, by compiling the source code of the program with the provided dedicated compilers. The produced binary is then fuzzed with SIVO. 

## How to compile source code

The source needs to be compiled twice, with two different compilers `sivo-clang1` and `sivo-clang2` and the produced binaries need to have consistent names with -1 and -2 suffixes, i.e. `bin-1` and `bin-2`. The second compilation will produce 3 additional files,  `llvm-cfg-bin`, `llvm-ifs-bin`, and `llvm-switches-bin`. For fuzzing, all 5 files (2 binaries + 3 llvm files) need to be in the same directory. 

Two environment vars  need to be set before calling the compilers:

* `__BINARY_COMPILE_NAME` to the name of the binary (e.g. `bin`)
* `__RUN_PATH` to the directory (absolute path!) where llvm files will be produced

Here is the whole compilation for the sample program from the repo:

	cd sample_program

	export __BINARY_COMPILE_NAME="myprog";
	export __RUN_PATH="$(realpath .)"

	~/Sivo-fuzzer/sivo-clang1 some_funcs.c -c -o some_funcs.o
	~/Sivo-fuzzer/sivo-clang1 myprog.c some_funcs.o -o myprog-1
	~/Sivo-fuzzer/sivo-clang2 some_funcs.c -c -o some_funcs.o
	~/Sivo-fuzzer/sivo-clang2 myprog.c some_funcs.o -o myprog-2

For `configure` scripts usually the compilation is 

	cd prog_source

	export __BINARY_COMPILE_NAME="mbin";
	export __RUN_PATH="/home/me/prog_source"

	./configure                             \
	CC=/home/me/Sivo-fuzzer/sivo-clang1     \
	CXX=/home/me/Sivo-fuzzer/sivo-clang1++
	make install 

	mv /home/me/prog_source/mbin /home/me/prog_source/mbin-1

	./configure                             \
	CC=/home/me/Sivo-fuzzer/sivo-clang2     \
	CXX=/home/me/Sivo-fuzzer/sivo-clang2++ 
	make install 

	mv /home/me/prog_source/mbin /home/me/prog_source/mbin-2

## How to fuzz

The fuzzer needs the directory containg the 5 files (2 bins + 3 llvm) and input params for the program. The syntax is as follow

	sivo output_dir path_to_binaries [input params] 

The initial input files (i.e. what the fuzzer can start with) should be copied in the directory `output_dir/init_seeds/queue`. At least one file should be in the folder. The input files in the input line are specified as  `@@`. 

For instance, the sample program from the repo (takes one input which is name of the input file) can be fuzzed:

	mkdir -p output_sivo/init_seeds
	cp -r ~/sample_program/seeds output_sivo/init_seeds/queue
	
	~/Sivo-fuzzer/sivo output_sivo ~/sample_program/myprog @@

## Results of fuzzing

SIVO produces inputs that increase code coverage and that the lead to crashes of the program. These inputs can be found in the specified output directory, i.e. coverage in `output_dir/outputs/queue` and  crashes in `output_dir/outputs/crashes`.

