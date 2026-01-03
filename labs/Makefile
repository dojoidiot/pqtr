# Makefile for labs - C++ flow model and future GPU work
#
# Usage:
#   make            - build labs CLI
#   make test-unit  - run unit tests (no pipe/ linkage)
#   make test-int   - run integration tests (with pipe/)
#   make test-gold  - run gold pipeline test (labs steps)
#   make test-pipe  - run reference pipeline (pipe modules, produces dumps)
#   make test       - run all tests
#   make clean      - remove build artifacts

CXX := g++
CC := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
CFLAGS := -Wall -Wextra -O2
LDFLAGS := -lz -lm

# Pipe sources (pure C, never modify)
PIPE := ../pipe
PIPE_SRCS := $(PIPE)/src/main/labs/sony.c

# Source files
SRCS := src/main/labs.cpp \
        $(wildcard src/main/part/*.cpp) \
        $(wildcard src/main/step/*.cpp) \
        $(wildcard src/main/plug/*.cpp)

# Shared libs
PART_SRCS := $(wildcard src/main/part/*.cpp)
PLUG_SRCS := $(wildcard src/main/plug/*.cpp)
STEP_SRCS := $(wildcard src/main/step/*.cpp)

# Unit test (no pipe/ linkage)
UNIT_SRCS := src/test/unit/pipe.cpp $(PART_SRCS)

# Integration test - basic (with pipe/)
INT_CPP := src/test/labs/labs.cpp $(PART_SRCS) $(PLUG_SRCS)
INT_C := $(PIPE_SRCS)

# Gold pipeline test (full pipeline with labs steps)
GOLD_CPP := src/test/labs/gold.cpp $(PART_SRCS) $(PLUG_SRCS) $(STEP_SRCS)
GOLD_C := $(PIPE_SRCS)

# Reference pipeline test (pipe modules only, produces dumps)
PIPE_TEST := src/test/pipe/gold.cpp

BUILD := tmp/build

OBJS := $(SRCS:%.cpp=$(BUILD)/%.o)
UNIT_OBJS := $(UNIT_SRCS:%.cpp=$(BUILD)/%.o)
INT_CPP_OBJS := $(INT_CPP:%.cpp=$(BUILD)/%.o)
INT_C_OBJS := $(INT_C:%.c=$(BUILD)/%.o)
INT_OBJS := $(INT_CPP_OBJS) $(INT_C_OBJS)
GOLD_CPP_OBJS := $(GOLD_CPP:%.cpp=$(BUILD)/%.o)
GOLD_C_OBJS := $(GOLD_C:%.c=$(BUILD)/%.o)
GOLD_OBJS := $(GOLD_CPP_OBJS) $(GOLD_C_OBJS)

INCLUDES := -I inc -I src

.PHONY: all labs diff test test-unit test-int test-gold test-pipe clean

all: $(BUILD)/labs

$(BUILD)/labs: $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

labs: $(BUILD)/labs
	@echo "Built: tmp/build/labs"

$(BUILD)/diff: src/main/diff.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $<

diff: $(BUILD)/diff
	@echo "Built: tmp/build/diff"

# Unit test (no pipe/ linkage)
$(BUILD)/test-unit: $(UNIT_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^

test-unit: $(BUILD)/test-unit
	@$(BUILD)/test-unit

# Integration test (with pipe/)
$(BUILD)/test-int: $(INT_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^

test-int: $(BUILD)/test-int
	@$(BUILD)/test-int

# Gold pipeline test
$(BUILD)/test-gold: $(GOLD_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test-gold: $(BUILD)/test-gold
	@echo "=== integration test: gold pipeline ==="
	@echo ""
	@$(BUILD)/test-gold
	@echo ""
	@echo "=== pass ==="

# Reference pipeline test (links directly to pipe modules)
$(BUILD)/test-pipe: $(PIPE_TEST)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

test-pipe: $(BUILD)/test-pipe
	@echo "=== reference pipeline (pipe modules) ==="
	@echo ""
	@$(BUILD)/test-pipe
	@echo ""
	@echo "=== dumps in tmp/var/pipe/ ==="

# Run all tests
test: test-unit test-int

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf tmp/build
