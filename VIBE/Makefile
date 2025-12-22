# VIBE Makefile - Sony RAW decoder with Dawn/WebGPU
.PHONY: all test tune tidy

# === Compiler ===
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# === Directories ===
SRC_DIR = src/main
TEST_DIR = tmp/test
OUT_DIR = tmp/var/flow

# === Dawn (use LABS build) ===
LABS_DIR = ../LABS
DAWN_DIR = $(LABS_DIR)/lib/dawn/install
DAWN_BUILD = $(LABS_DIR)/lib/dawn/build
DAWN_INC = -I$(DAWN_DIR)/include -I$(LABS_DIR)/lib/dawn/include
DAWN_LIB = $(DAWN_DIR)/lib/libwebgpu_dawn.a
DAWN_NATIVE_LIBS = $(DAWN_BUILD)/src/dawn/native/libdawn_native.a \
                   $(DAWN_BUILD)/src/dawn/libdawn_proc.a

# === Includes ===
INC = -Iinc \
      -Isrc/main/flow/part \
      $(DAWN_INC)

# === Core source files (shared by all targets) ===
CORE_SRCS = src/main/flow/flow.cpp \
            src/main/flow/part/tree.cpp \
            src/main/flow/part/load.cpp \
            src/main/flow/part/swap.cpp \
            src/main/flow/part/copy.cpp \
            src/main/flow/part/png.cpp \
            src/main/flow/part/jpg.cpp \
            src/main/flow/part/head.cpp \
            src/main/flow/part/vibe.cpp \
            src/main/flow/part/tune.cpp \
            src/main/flow/part/warp.cpp \
            src/main/flow/part/sony/sony.cpp \
            src/main/flow/part/sony/prepare.cpp \
            src/main/flow/part/sony/process.cpp

# === Test executables ===
TEST_FLOW = $(TEST_DIR)/test_flow
TEST_TUNE = $(TEST_DIR)/test_tune

# === Targets ===
all: $(TEST_FLOW) $(TEST_TUNE)

$(TEST_FLOW): src/test/flow/flow.cpp $(CORE_SRCS) | $(DAWN_LIB)
	@mkdir -p $(TEST_DIR)
	@mkdir -p $(OUT_DIR)
	$(CXX) $(CXXFLAGS) $(INC) $^ $(DAWN_NATIVE_LIBS) $(DAWN_LIB) -lpthread -ldl -lz -o $@

$(TEST_TUNE): src/test/flow/test_tune.cpp $(CORE_SRCS) | $(DAWN_LIB)
	@mkdir -p $(TEST_DIR)
	@mkdir -p $(OUT_DIR)
	$(CXX) $(CXXFLAGS) $(INC) $^ $(DAWN_NATIVE_LIBS) $(DAWN_LIB) -lpthread -ldl -lz -o $@

# Check Dawn is built
$(DAWN_LIB):
	@echo "Dawn not built. Run: cd ../LABS/lib && ./dawn.sh"
	@exit 1

test: $(TEST_FLOW)
	@echo "=== VIBE Flow Test ==="
	@cd $(shell pwd) && $(TEST_FLOW) || true

tune: $(TEST_TUNE)
	@echo "=== VIBE Tune Test ==="
	@cd $(shell pwd) && $(TEST_TUNE) || true

tidy:
	rm -rf tmp
