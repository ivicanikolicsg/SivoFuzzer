#!/bin/bash

# setup.sh - downloads clang-llvm-3.8.0 (if needed) and sets up PATH
#

URL='https://releases.llvm.org/3.8.0/clang+llvm-3.8.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz'
DIRNAME="clang_llvm-3.8.0"

# check script executor
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
	printf "\033[31mPlease source this file!\033[0m\n"
	exit 0
fi

# download archive if clang_llvm dir doesn't exist
if [[ ! -d ${DIRNAME} ]]; then
	printf "Looks like you are missing \033[33m${DIRNAME}\033[0m. "
	printf "Please wait ... "

	wget -q ${URL}
	tar xf $(basename ${URL})
	mv $(basename ${URL} .tar.xz) ${DIRNAME}
	rm -f $(basename ${URL})

	printf "\033[32mdone\033[0m\n"
else
	printf "You already have \033[33m${DIRNAME}\033[0m\n"
fi

# add clang path (if needed)
CLANGPATH="$(realpath ${DIRNAME}/bin)"
if [[ ":${PATH}:" == *":${CLANGPATH}:"* ]]; then
	printf "Your PATH already has \033[33mclang-3.8.0\033[0m!\n"
else
	export PATH="${CLANGPATH}:${PATH}"
	printf "Your PATH has been updated for \033[33mclang-3.8.0\033[0m!\n"
fi

# add sivo path (if needed)
# NOTE: still have to compile it
SIVOPATH="$(realpath Sivo-fuzzer)"
if [[ ":${PATH}:" == *":${SIVOPATH}:"* ]]; then
    printf "Your PATH already has \033[33msivo\033[0m!\n"
else
    export PATH="${SIVOPATH}:${PATH}"
    printf "Your PATH has been updated for \033[33msivo\033[0m!\n"
fi

