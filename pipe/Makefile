# Makefile for pipe - Darktable module matching
#
# Usage:
#   make        - build all tools
#   make test   - run step 0 test
#   make dark   - build XMP parser tool
#   make clean  - remove build artifacts

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
LDFLAGS := -lz

# Source files (excluding process.cpp which needs Dawn)
SRCS := \
	src/main/flow/tree.cpp \
	src/main/flow/swap.cpp \
	src/main/flow/png.cpp \
	src/main/flow/jpg.cpp \
	src/main/flow/head.cpp \
	src/main/flow/sony/sony.cpp \
	src/main/flow/sony/prepare.cpp

# Build directory
BUILD := tmp/build

# Object files
OBJS := $(SRCS:%.cpp=$(BUILD)/%.o)

# Include paths
INCLUDES := -I inc -I src/main/flow

# Targets
.PHONY: all test dark clean tidy

all: $(BUILD)/pipe_test $(BUILD)/dark

# Step 0 test
$(BUILD)/pipe_test: $(OBJS) $(BUILD)/src/test/pipe.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# XMP parser tool (standalone, only needs tree.cpp for Tree)
$(BUILD)/dark: $(BUILD)/src/test/dark.o $(BUILD)/src/main/flow/tree.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

test: $(BUILD)/pipe_test
	@echo "=== Running Step 0 Test ==="
	@cd /home/z/base/code/pqtr/pipe && ./tmp/build/pipe_test

dark: $(BUILD)/dark
	@echo "Built: tmp/build/dark"

clean:
	rm -rf tmp/build tmp/var/pipe/*.png tmp/var/pipe/*.bin tmp/var/pipe/*.json tmp/var/pipe/*.tiff

tidy:
	rm -rf tmp
