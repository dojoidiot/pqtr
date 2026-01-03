# Makefile for pipe - Darktable module copy and test
#
# Pure C modules copied from darktable for verification.
# See copy.md for the copy process documentation.
#
# Usage:
#   make gold      - build gold pipeline test
#   make test-gold - run gold pipeline test
#   make clean     - remove build artifacts

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

BUILD := tmp/build

.PHONY: all gold test-gold test-phase1 test-phase2 dt-dumps test-all clean tidy

DT_CLI := ./dark/lib/desk/build/bin/darktable-cli
DT_OPTS := --core --disable-opencl

all: $(BUILD)/gold

# Gold pipeline test (full end-to-end) - uses pure C modules
$(BUILD)/gold: src/test/labs/gold.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $< -lm

gold: $(BUILD)/gold
	@echo "Built: tmp/build/gold"

test-gold: $(BUILD)/gold
	@echo "=== Gold Pipeline Test ==="
	@mkdir -p tmp/var
	./$(BUILD)/gold

# Phase tests
$(BUILD)/phase1_test: src/test/labs/phase1.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I src/main/labs -o $@ $<

test-phase1: $(BUILD)/phase1_test
	@echo "=== Testing Phase 1 (colorout) ==="
	@mkdir -p tmp/var
	./$(BUILD)/phase1_test

$(BUILD)/phase2_test: src/test/labs/phase2.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I src/main/labs -o $@ $<

test-phase2: $(BUILD)/phase2_test
	@echo "=== Phase 2 Final Output ==="
	@mkdir -p tmp/var
	./$(BUILD)/phase2_test

# Generate DT dumps for phase tests
dt-dumps:
	@echo "=== Generating DT dumps ==="
	@mkdir -p /tmp/dtdump
	$(DT_CLI) src/test/raws/sony.ARW src/test/raws/phase1.xmp /tmp/out.png $(DT_OPTS) --dump-pipe colorout --dumpdir /tmp/dtdump
	$(DT_CLI) src/test/raws/sony.ARW src/test/raws/phase2.xmp /tmp/out.png $(DT_OPTS) --dump-pipe bilat --dumpdir /tmp/dtdump

# Run all tests
test-all: dt-dumps test-gold test-phase1 test-phase2
	@echo ""
	@echo "=== All tests complete ==="
	@ls -la tmp/var/*.png

clean:
	rm -rf tmp/build tmp/var/*.png tmp/var/*.bin

tidy:
	rm -rf tmp
