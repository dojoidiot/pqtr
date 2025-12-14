# GEAR Library Makefile
# Builds lib/GEAR.a static library
#
# Targets:
#   make      Build GEAR.a library
#   make test Run WGPU (Dawn) shader tests
#   make tidy Clean build artifacts
#
# GPU compute via WGPU (WebGPU)

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# Include paths
INCLUDES = -I./inc \
           -I./lib \
           -I./src/main/part \
           -I../PIPE/inc

# Directories
SRC_DIR = src/main
OBJ_DIR = tmp/obj
LIB_DIR = lib

# Sources
SRCS = $(SRC_DIR)/part/sony/prepare_pure.cpp \
       $(SRC_DIR)/sony_link.cpp

# Objects
OBJS = $(OBJ_DIR)/prepare_pure.o \
       $(OBJ_DIR)/sony_link.o

# Target
GEAR_A = $(LIB_DIR)/GEAR.a

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
TEST_BIN = tmp/test/test_dawn
TEST_OBJ_DIR = tmp/obj/dawn

TEST_SRCS = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.cpp=$(TEST_OBJ_DIR)/%.o)

WGPU_WRAPPER = $(WGPU_DIR)/tmp/dawn.o
WGPU_ALL_LIBS = $(shell find $(WGPU_BUILD) -name "*.a" 2>/dev/null)
WGPU_SYS_LIBS = -lvulkan -ldl -lpthread -lX11

# ============================================================
# Targets
# ============================================================

.PHONY: all test tidy help

help:
	@echo "GEAR Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make      Build GEAR.a library"
	@echo "  make test Run WGPU (Dawn) shader tests"
	@echo "  make tidy Clean build artifacts"

all: $(GEAR_A)

$(GEAR_A): $(OBJS) | $(LIB_DIR)
	ar rcs $@ $^
	@echo "Built: $@"

$(OBJ_DIR)/prepare_pure.o: $(SRC_DIR)/part/sony/prepare_pure.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/sony_link.o: $(SRC_DIR)/sony_link.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# ============================================================
# Test target (WGPU/Dawn)
# ============================================================

test: $(TEST_BIN)
	$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS)
	@mkdir -p tmp/test
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) \
		$(WGPU_WRAPPER) \
		-Wl,--start-group $(WGPU_ALL_LIBS) -Wl,--end-group \
		$(WGPU_SYS_LIBS) \
		-o $@

$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(TEST_OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(WGPU_INC) -I$(TEST_DIR) -c $< -o $@

# ============================================================
# Tidy target
# ============================================================

tidy:
	rm -rf tmp $(GEAR_A)
