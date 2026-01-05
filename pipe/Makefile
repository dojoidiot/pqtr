# Makefile for pipe - Darktable module copy and test
#
# Pure C modules copied from darktable for verification.
# See copy.md for the copy process documentation.
#
# Usage:
#   make gold      - build gold pipeline (mods/)
#   make args      - build args pipeline (args/)
#   make test      - run all tests
#   make clean     - remove build artifacts

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

BUILD := tmp/build
VAR := tmp/var

.PHONY: all gold args test test-gold test-args clean tidy

all: $(BUILD)/gold $(BUILD)/args

# Gold pipeline (uses mods/ modules)
$(BUILD)/gold: src/main/gold.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $< -lm

gold: $(BUILD)/gold
	@echo "Built: $(BUILD)/gold"

# Algo pipeline (uses args/ modules)
$(BUILD)/args: src/main/args.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $< -lm

args: $(BUILD)/args
	@echo "Built: $(BUILD)/args"

# Test targets
test: test-gold test-args
	@echo ""
	@echo "=== All tests complete ==="

test-gold: test-gold-458 test-gold-202

test-args: test-args-458 test-args-202

test-gold-458: $(BUILD)/gold
	@mkdir -p $(VAR)
	./$(BUILD)/gold src/test/raws/DSC00458.ARW $(VAR)/gold_458.png

test-gold-202: $(BUILD)/gold
	@mkdir -p $(VAR)
	./$(BUILD)/gold src/test/raws/DSC00202.ARW $(VAR)/gold_202.png

test-args-458: $(BUILD)/args
	@mkdir -p $(VAR)
	./$(BUILD)/args src/test/raws/DSC00458.ARW $(VAR)/args_458.png

test-args-202: $(BUILD)/args
	@mkdir -p $(VAR)
	./$(BUILD)/args src/test/raws/DSC00202.ARW $(VAR)/args_202.png

clean:
	rm -rf tmp/build tmp/var

tidy:
	rm -rf tmp
