# Makefile for pipe - Darktable module matching
#
# Usage:
#   make        - build all tools
#   make test   - run step 0 test
#   make dark   - build XMP parser tool
#   make labs   - build labs pipeline CLI
#   make clean  - remove build artifacts

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
LDFLAGS := -lz

# Source files - auto-discovered via glob
# New modules added to src/main/flow/ are automatically included
SRCS := $(wildcard src/main/flow/*.cpp) \
        $(wildcard src/main/flow/sony/*.cpp) \
        $(wildcard src/main/flow/canon/*.cpp)

# Labs sources - auto-discovered via glob
LABS_SRCS := src/main/labs/labs.cpp \
             $(wildcard src/main/labs/part/*.cpp) \
             $(wildcard src/main/labs/step/*.cpp) \
             $(wildcard src/main/labs/plug/*.cpp) \
             $(wildcard src/main/labs/plug/sony/*.cpp) \
             $(wildcard src/main/labs/plug/canon/*.cpp)

# Build directory
BUILD := tmp/build

# Object files
OBJS := $(SRCS:%.cpp=$(BUILD)/%.o)
LABS_OBJS := $(LABS_SRCS:%.cpp=$(BUILD)/%.o)

# Include paths
INCLUDES := -I inc -I src/main/flow -I src/main/labs/plug

# Targets
.PHONY: all test dark labs clean tidy ref cli diff test-head-sony test-head-canon test-rawprepare test-phase1 test-phase2 gold test-gold

all: $(BUILD)/dark $(BUILD)/labs $(BUILD)/diff

# Diff tool for binary comparison
$(BUILD)/diff: src/main/labs/diff.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $<

diff: $(BUILD)/diff
	@echo "Built: tmp/build/diff"

# Head tests
$(BUILD)/head_sony_test: src/test/labs/head_sony_test.cpp \
                         $(wildcard src/main/labs/plug/sony/*.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

test-head-sony: $(BUILD)/diff $(BUILD)/head_sony_test
	@echo "=== Testing Sony ARW Head ==="
	./$(BUILD)/head_sony_test

$(BUILD)/head_canon_test: src/test/labs/head_canon_test.cpp \
                          $(wildcard src/main/labs/plug/canon/*.cpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

test-head-canon: $(BUILD)/diff $(BUILD)/head_canon_test
	@echo "=== Testing Canon CR2 Head ==="
	./$(BUILD)/head_canon_test

# Module tests (Phase 1: copy)
$(BUILD)/rawprepare_test: src/test/labs/mods/rawprepare_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I src/main/labs -o $@ $<

test-rawprepare: $(BUILD)/diff $(BUILD)/rawprepare_test
	@echo "=== Testing rawprepare ==="
	./$(BUILD)/rawprepare_test

# Phase 1 colorout test
$(BUILD)/phase1_test: src/test/labs/phase1.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I src/main/labs -o $@ $<

test-phase1: $(BUILD)/phase1_test
	@echo "=== Testing Phase 1 (colorout) ==="
	@mkdir -p tmp/var
	./$(BUILD)/phase1_test

# Phase 2 final output test
$(BUILD)/phase2_test: src/test/labs/phase2.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I src/main/labs -o $@ $<

test-phase2: $(BUILD)/phase2_test
	@echo "=== Phase 2 Final Output ==="
	@mkdir -p tmp/var
	./$(BUILD)/phase2_test

# Gold pipeline test (full end-to-end) - uses sony.c decoder, no LibRaw
$(BUILD)/gold: src/test/labs/gold.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $< -lm

gold: $(BUILD)/gold
	@echo "Built: tmp/build/gold"

test-gold: $(BUILD)/gold
	@echo "=== Gold Pipeline Test ==="
	@mkdir -p tmp/var
	./$(BUILD)/gold

# Module tests will be added as modules are transpiled via copy.md
# Template:
# $(BUILD)/MODULE: $(OBJS) $(BUILD)/src/test/MODULE.o
#	@mkdir -p $(dir $@)
#	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
#
# test-MODULE: $(BUILD)/MODULE
#	@./$(BUILD)/MODULE

# XMP parser tool (standalone, only needs tree.cpp for Tree)
$(BUILD)/dark: $(BUILD)/src/test/dark.o $(BUILD)/src/main/flow/tree.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

test: $(BUILD)/default
	@echo "=== Running Default Pipeline Test ==="
	@cd /home/z/base/code/pqtr/pipe && ./tmp/build/default

dark: $(BUILD)/dark
	@echo "Built: tmp/build/dark"

# Labs pipeline CLI
$(BUILD)/labs: $(LABS_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

labs: $(BUILD)/labs
	@echo "Built: tmp/build/labs"

# Test labs pipeline
test-labs: $(BUILD)/labs
	@echo "=== Testing Labs Pipeline ==="
	./$(BUILD)/labs --head src/test/labs/labs.ARW --pipe src/test/labs/labs.json

clean:
	rm -rf tmp/build tmp/var/pipe/*.png tmp/var/pipe/*.bin tmp/var/pipe/*.json tmp/var/pipe/*.tiff

tidy:
	rm -rf tmp
