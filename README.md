# SIVO: Refined gray-box fuzzer

This repository contains the C++ implementation of the SIVO gray-box fuzzer. 
The technical aspects of the fuzzer are described in [our paper](https://arxiv.org/pdf/xxx.pdf). 

## Installation and Dependencies

The code has been tested on Ubuntu 16.04 with LLVM 3.8.0. Follow these steps to set up the project:

	$ sudo apt update
	$ sudo apt install -y libssl-dev make gcc g++ wget xz-utils
	$ source setup.sh

	$ pushd Sivo-fuzzer
	$ make -j $(nproc)
	$ export PATH="${PATH}:$(realpath .)"
	$ popd

`setup.sh` will download clang+llvm-3.8.0 (if not present) and update your PATH. You may want to make the change to PATH permanent by editing your `~/.bashrc` or `~/.zshrc`. Alternatively, you can source `setup.sh` without redownloading clang+llvm at any time.

The makefile will build the fuzzer `sivo`, as well as the dedicated compilers `sivo-clang1` and `sivo-clang2`. These can be called by their basenames due to the updated PATH.

## How it works

SIVO is a **gray**-box fuzzer. Gray means it needs to add extra code to the fuzzed program to track code coverage (and other stuff). This is done automatically, by compiling the source code of the program with the provided dedicated compilers. The produced binary is then fuzzed with SIVO. 

## How to compile source code

The source needs to be compiled twice, with two different compilers `sivo-clang1` and `sivo-clang2`. The produced binaries need to have consistent names with `-1` and `-2` as suffixes (e.g.: `readelf-1` and `readelf-2`). The second compilation will produce 3 additional files: `llvm-cfg-bin`, `llvm-ifs-bin`, and `llvm-switches-bin`. For fuzzing, all 5 files (2 binaries + 3 llvm files) need to be in the same directory. 

Two environment variables need to be set before calling the compilers:

* `__BINARY_COMPILE_NAME`: to the name of the binary (e.g. `bin`)
* `__RUN_PATH`: to the directory (absolute path!) where llvm files will be produced

NOTE: in the case of binutils (for example), you can set `__BINARY_COMPILE_NAME` to anything and only one of each `llvm-*` files will be created. However, if you wish to fuzz multiple binaries generated as a result of the automake process, you will need to create copies of the `llvm-*` files for each binary `${BIN}` as follows: `llvm-{cfg,ifs,switches}-${BIN}`. 

Here is the whole compilation for the sample program from the repo:

	$ cd sample_program
	$ export __BINARY_COMPILE_NAME="myprog";
	$ export __RUN_PATH="$(realpath .)"

	$ sivo-clang1 -c -o some_funcs.o some_funcs.c
	$ sivo-clang1    -o myprog-1	 myprog.c some_funcs.o

	$ sivo-clang2 -c -o some_funcs.o some_funcs.c
	$ sivo-clang2    -o myprog-2     myprog.c some_funcs.o

For `configure` scripts usually the compilation is as follows:

	$ cd ${PROJECT_ROOT}
	$ export TMP_DIR="$(mktemp -d)"
	$ export OUT_DIR="$(mktemp -d)"

	$ export __BINARY_COMPILE_NAME="${BIN_NAME}"
	$ export __RUN_PATH="${OUT_DIR}"

	$ CC=sivo-clang1     \
	  CXX=sivo-clang1++  \
	  ./configure        \
	      --prefix ${TMP_DIR}
	$ make install -j $(nproc)
	$ mv ${TMP_DIR}/bin/${BIN_NAME} ${OUT_DIR}/${BIN_NAME}-1

	$ make distclean
	$ rm -rf ${TMP_DIR}/*

	$ CC=sivo-clang2     \
          CXX=sivo-clang2++  \
	  ./configure        \
	        --prefix ${TMP_DIR}
	$ make install -j $(nproc)
	$ mv ${TMP_DIR}/bin/${BIN_NAME} ${OUT_DIR}/${BIN_NAME}-2 

The process is similar for `cmake` based projects. Just check `CMakeLists.txt` for compilation related variables and set them accordingly:

	$ mkdir -p build
	$ cd build
	$ cmake                                     \
	      -D${COMPILER_OPTION_HERE}=sivo-clang1 \
	      ..

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

