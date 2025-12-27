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
	src/main/flow/rawprepare.cpp \
	src/main/flow/demosaic.cpp \
	src/main/flow/temperature.cpp \
	src/main/flow/colorin.cpp \
	src/main/flow/colorout.cpp \
	src/main/flow/gamma.cpp \
	src/main/flow/sony/sony.cpp \
	src/main/flow/sony/prepare.cpp \
	src/main/flow/sony/stb_impl.cpp

# Build directory
BUILD := tmp/build

# Object files
OBJS := $(SRCS:%.cpp=$(BUILD)/%.o)

# Include paths
INCLUDES := -I inc -I src/main/flow

# Targets
.PHONY: all test dark clean tidy test-rawprepare test-temperature test-demosaic test-colorin ref

all: $(BUILD)/default $(BUILD)/dark

# Module tests
$(BUILD)/rawprepare: $(OBJS) $(BUILD)/src/test/rawprepare.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/temperature: $(OBJS) $(BUILD)/src/test/temperature.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/demosaic: $(OBJS) $(BUILD)/src/test/demosaic.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/colorin: $(OBJS) $(BUILD)/src/test/colorin.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test-rawprepare: $(BUILD)/rawprepare
	@./$(BUILD)/rawprepare

test-temperature: $(BUILD)/temperature
	@./$(BUILD)/temperature

test-demosaic: $(BUILD)/demosaic
	@./$(BUILD)/demosaic

test-colorin: $(BUILD)/colorin
	@./$(BUILD)/colorin

# Generate reference files
ref:
	python3 scripts/gen_ref.py all

# Default pipeline test
$(BUILD)/default: $(OBJS) $(BUILD)/src/test/default.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

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

clean:
	rm -rf tmp/build tmp/var/pipe/*.png tmp/var/pipe/*.bin tmp/var/pipe/*.json tmp/var/pipe/*.tiff

tidy:
	rm -rf tmp
