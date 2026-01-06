# Makefile for copy - C++ RAW processing pipeline

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I inc -I src -fopenmp
LDFLAGS := -lm -fopenmp

BUILD := build
BIN := $(BUILD)/copy

# Find all cpp files in src
SRCS := $(shell find src -name "*.cpp")
OBJS := $(SRCS:src/%.cpp=$(BUILD)/%.o)

.PHONY: all clean run test

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(BIN)
	@mkdir -p output
	./$(BIN) ../args/src/test/raws/DSC00458.ARW output/out.png

test: $(BIN)
	@mkdir -p output
	./$(BIN) ../args/src/test/raws/DSC00458.ARW output/out_458.png
	# ./$(BIN) ../args/src/test/raws/DSC00202.ARW output/out_202.png

clean:
	rm -rf $(BUILD) output
