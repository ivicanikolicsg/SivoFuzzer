

LLVM_CONFIG ?= llvm-config

CFLAGS      ?= -O3 -funroll-loops 
CFLAGS      += -Wall -g -Wno-pointer-sign  

CXXFLAGS    ?= -O3 -funroll-loops -std=c++11
CXXFLAGS    += -Wall -g -Wno-pointer-sign -Wno-variadic-macros -Wno-unknown-warning-option

CLANG_CFL    = `$(LLVM_CONFIG) --cxxflags` -fno-rtti -fpic $(CXXFLAGS)
CLANG_LFL    = `$(LLVM_CONFIG) --ldflags` $(LDFLAGS)

INC          = ../include

ifeq "$(origin CC)" "default"
  CC         = clang
  CXX        = clang++
endif


PROGS       = forkserver.o pass-coverage.o pass-branches.o 

all: test_deps $(PROGS) test_build all_done

test_deps:
	@echo "[*] Checking for working '$(CC)'..."



pass-coverage.o: pass-coverage.cc | test_deps
	$(CXX) $(CLANG_CFL) -I $(INC) -shared $< -o $@ $(CLANG_LFL)

pass-branches.o: pass-branches.cc | test_deps
	$(CXX) $(CLANG_CFL) -I $(INC) -shared $< -o $@ $(CLANG_LFL)

forkserver.o: forkserver.c | test_deps
	$(CC) $(CFLAGS) -I $(INC) -fPIC -c $< -o $@


test_build: $(PROGS)
	@echo "[*] Testing the CC wrapper and instrumentation output..."

all_done: test_build
	@echo "[+] All done! "

clean:
	rm -f *.o

.NOTPARALLEL: clean

