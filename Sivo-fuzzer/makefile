#.PHONY: dirs passes

# current directory
CDIR = $(shell pwd)

# important directories
OBJ       = obj
INC       = include
SRC_SIVO  = src
SRC_CLANG = sivo-clang
LLVM      = llvm

# compilation related parameters
CC        = gcc
CFLAGS    = -O3 -ggdb -no-pie -DSIVO_PATH=\"$(CDIR)\"
CXX       = g++
CXXFLAGS  = -O3 -std=c++14 -ggdb -no-pie 
LIBS      = -lcrypto

# fuzzer sources -> objects -> binary
SOURCES_SIVO = $(wildcard $(SRC_SIVO)/*.cpp)
OBJECTS_SIVO = $(patsubst $(SRC_SIVO)/%.cpp, $(OBJ)/%.o, $(SOURCES_SIVO))
BIN_SIVO     = sivo

# clang wrapper sources -> [objects] -> binaries
SOURCES_CLANG = $(wildcard $(SRC_CLANG)/*.c)
BIN_CLANG	  = $(patsubst $(SRC_CLANG)/%.c, %, $(SOURCES_CLANG))

# top level rule
build: dirs $(BIN_SIVO) $(BIN_CLANG) passes

# create obj/ directory
dirs:
	@mkdir -p $(OBJ)

# top fuzzer generation rule
$(BIN_SIVO): $(OBJECTS_SIVO)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# fuzzer's component object compilation rule
$(OBJ)/%.o: $(SRC_SIVO)/%.cpp|$(INC)/%.h
	$(CXX) $(CXXFLAGS) -I $(INC) -c -o $@ $< $(LIBS) 

# top clang wrapper generation rule
% : $(OBJ)/%.o
	$(CC) $(CFLAGS) -I $(INC) -o $@ $^
	ln -s $@ $@++

# wrapper's component object compilation rule
$(OBJ)/%.o: $(SRC_CLANG)/%.c|$(INC)/config_llvm.h
	$(CC) $(CFLAGS) -I $(INC) -c -o $@ $<

# yielding to llvm/ makefile to compile passes & forkserver
passes:
	$(MAKE) -C $(LLVM)


clean:
	rm -rf $(OBJ) $(BIN_SIVO) sivo-clang1* sivo-clang2*
	$(MAKE) -C $(LLVM) clean

