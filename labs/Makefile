# Makefile for labs - C++ flow model and future GPU work
#
# Usage:
#   make        - build labs CLI
#   make clean  - remove build artifacts

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
LDFLAGS := -lz

# Source files
SRCS := src/labs.cpp \
        $(wildcard src/part/*.cpp) \
        $(wildcard src/step/*.cpp) \
        $(wildcard src/plug/*.cpp)

BUILD := tmp/build

OBJS := $(SRCS:%.cpp=$(BUILD)/%.o)

INCLUDES := -I inc -I src

.PHONY: all labs diff clean

all: $(BUILD)/labs

$(BUILD)/labs: $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

labs: $(BUILD)/labs
	@echo "Built: tmp/build/labs"

$(BUILD)/diff: src/diff.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $<

diff: $(BUILD)/diff
	@echo "Built: tmp/build/diff"

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -rf tmp/build
