.PHONY: dirs

# important directories / files
SRC	   = src
BIN	   = bin
OBJ    = obj
INC    = include

TARGET = woffy

# compilation related parameters
CXX-1  = sivo-clang1++
CXX-2  = sivo-clang2++

# export sivo-clang env variables
export __BINARY_COMPILE_NAME = $(TARGET)
export __RUN_PATH            = $(shell pwd)/$(BIN)

# identify sources and create object file targets
SOURCES_WOF = $(wildcard $(SRC)/wof/*.cpp)
SOURCES_DEC = $(wildcard $(SRC)/dec/*.cpp)
SOURCES_ENC = $(wildcard $(SRC)/enc/*.cpp)

OBJECTS-1   = $(patsubst $(SRC)/wof/%.cpp, $(OBJ)-1/%.o, $(SOURCES_WOF))
OBJECTS-1  += $(patsubst $(SRC)/dec/%.cpp, $(OBJ)-1/%.o, $(SOURCES_DEC))
OBJECTS-1  += $(patsubst $(SRC)/enc/%.cpp, $(OBJ)-1/%.o, $(SOURCES_ENC))
OBJECTS-1  += $(OBJ)-1/main.o $(OBJ)-1/target.o

OBJECTS-2   = $(patsubst $(SRC)/wof/%.cpp, $(OBJ)-2/%.o, $(SOURCES_WOF))
OBJECTS-2  += $(patsubst $(SRC)/dec/%.cpp, $(OBJ)-2/%.o, $(SOURCES_DEC))
OBJECTS-2  += $(patsubst $(SRC)/enc/%.cpp, $(OBJ)-2/%.o, $(SOURCES_ENC))
OBJECTS-2  += $(OBJ)-2/main.o $(OBJ)-2/target.o

# top level rule
build: dirs $(BIN)/$(TARGET)-1 $(BIN)/$(TARGET)-2

# non-persistent folder creation rule
dirs:
	@mkdir -p $(BIN) $(OBJ)-1 $(OBJ)-2

# final binary generation rules
$(BIN)/$(TARGET)-1: $(OBJECTS-1)
	$(CXX-1) -o $@ $^

$(BIN)/$(TARGET)-2: $(OBJECTS-2)
	$(CXX-2) -o $@ $^

# object -1 generation rules
$(OBJ)-1/%.o: $(SRC)/wof/%.cpp
	$(CXX-1) -std=c++11 -I $(INC)/wof -I $(INC)/dec -I $(INC)/enc -c -o $@ $<

$(OBJ)-1/%.o: $(SRC)/dec/%.cpp
	$(CXX-1) -I $(INC)/dec -I $(INC)/enc -c -o $@ $<

$(OBJ)-1/%.o: $(SRC)/enc/%.cpp
	$(CXX-1) -I $(INC)/enc -I $(INC)/dec -c -o $@ $<

$(OBJ)-1/%.o: $(SRC)/%.cpp
	$(CXX-1) -std=c++11 -I $(INC)/wof -I $(INC)/dec -I $(INC)/enc -c -o $@ $<

# ovject -2 generation rules
$(OBJ)-2/%.o: $(SRC)/wof/%.cpp
	$(CXX-2) -std=c++11 -I $(INC)/wof -I $(INC)/dec -I $(INC)/enc -c -o $@ $<

$(OBJ)-2/%.o: $(SRC)/dec/%.cpp
	$(CXX-2) -I $(INC)/dec -I $(INC)/enc -c -o $@ $<

$(OBJ)-2/%.o: $(SRC)/enc/%.cpp
	$(CXX-2) -I $(INC)/enc -I $(INC)/dec -c -o $@ $<

$(OBJ)-2/%.o: $(SRC)/%.cpp
	$(CXX-2) -std=c++11 -I $(INC)/wof -I $(INC)/dec -I $(INC)/enc -c -o $@ $<

# clean rule
clean:
	@rm -rf $(BIN) $(OBJ)-1 $(OBJ)-2 testcases/outputs

