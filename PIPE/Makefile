# PIPE Makefile - Core pipeline infrastructure
#
# Targets:
#   make      Build pipe.a library
#   make test Run WGSL shader tests (Dawn)
#   make tidy Clean build artifacts
#
# GPU compute via WGPU (WebGPU)

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# Include paths
INCLUDES = -I./inc

# Directories
SRC_DIR = src/main
OBJ_DIR = tmp/obj
LIB_DIR = lib

# Sources
SRCS = $(SRC_DIR)/pipe.cpp \
       $(SRC_DIR)/part/pipe/node.cpp

# Objects
OBJS = $(OBJ_DIR)/pipe.o \
       $(OBJ_DIR)/node.o

# Target
PIPE_A = $(LIB_DIR)/pipe.a

# ============================================================
# WGPU Test Configuration (Dawn backend)
# ============================================================

WGPU_DIR = ../WGPU
WGPU_SRC = $(WGPU_DIR)/lib/dawn
WGPU_BUILD = $(WGPU_SRC)/out/Release

WGPU_INC = -I$(WGPU_DIR)/inc \
           -I$(WGPU_SRC)/include \
           -I$(WGPU_BUILD)/gen/include

TEST_DIR = src/test/dawn
TEST_BIN = tmp/test/test_wgsl
TEST_OBJ_DIR = tmp/obj/test

TEST_SRCS = $(TEST_DIR)/test_wgsl.cpp \
            $(TEST_DIR)/dawn_pipe.cpp

TEST_OBJS = $(TEST_OBJ_DIR)/test_wgsl.o \
            $(TEST_OBJ_DIR)/dawn_pipe.o

WGPU_WRAPPER = $(WGPU_DIR)/tmp/dawn.o
WGPU_ALL_LIBS = $(shell find $(WGPU_BUILD) -name "*.a" 2>/dev/null)
WGPU_SYS_LIBS = -lvulkan -ldl -lpthread -lX11

# ============================================================
# Targets
# ============================================================

.PHONY: all test tidy help

help:
	@echo "PIPE Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make      Build pipe.a library"
	@echo "  make test Run WGSL shader tests"
	@echo "  make tidy Clean build artifacts"

all: $(PIPE_A)

$(PIPE_A): $(OBJS) | $(LIB_DIR)
	ar rcs $@ $^
	@echo "Built: $@"

$(OBJ_DIR)/pipe.o: $(SRC_DIR)/pipe.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/node.o: $(SRC_DIR)/part/pipe/node.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# ============================================================
# Test target (WGPU/Dawn)
# ============================================================

test: $(TEST_BIN)
	cd $(CURDIR) && $(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) | tmp/test
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) \
		$(WGPU_WRAPPER) \
		-Wl,--start-group $(WGPU_ALL_LIBS) -Wl,--end-group \
		$(WGPU_SYS_LIBS) \
		-o $@

$(TEST_OBJ_DIR)/test_wgsl.o: $(TEST_DIR)/test_wgsl.cpp | $(TEST_OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(WGPU_INC) -I$(TEST_DIR) -I./inc -c $< -o $@

$(TEST_OBJ_DIR)/dawn_pipe.o: $(TEST_DIR)/dawn_pipe.cpp | $(TEST_OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(WGPU_INC) -I$(TEST_DIR) -I./inc -c $< -o $@

$(TEST_OBJ_DIR):
	mkdir -p $(TEST_OBJ_DIR)

tmp/test:
	mkdir -p tmp/test

# ============================================================
# Tidy target
# ============================================================

tidy:
	rm -rf tmp $(PIPE_A)
