# VIBE Library Makefile
# Builds lib/vibe.a - Style processing module (45 dials + LUTs)
#
# Dependencies: OpenCV (for cv::UMat)

VIBE_A = lib/vibe.a

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fPIC

# OpenCV (assumes LABS provides it)
OPENCV_ROOT = ../PIPE/lib/opencv
OPENCV_INC = -I$(OPENCV_ROOT)/build \
             -I$(OPENCV_ROOT)/modules/core/include \
             -I$(OPENCV_ROOT)/modules/imgproc/include \
             -I$(OPENCV_ROOT)/modules/imgcodecs/include

INCLUDES = -I./inc $(OPENCV_INC)

SRC_DIR = src/main
MODS_DIR = src/main/mods
OBJ_DIR = tmp/obj
MODS_OBJ_DIR = tmp/obj/mods

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
MODS_SRCS = $(wildcard $(MODS_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
MODS_OBJS = $(MODS_SRCS:$(MODS_DIR)/%.cpp=$(MODS_OBJ_DIR)/%.o)
ALL_OBJS = $(OBJS) $(MODS_OBJS)

.PHONY: make tidy

make: $(VIBE_A)

$(VIBE_A): $(ALL_OBJS)
	@mkdir -p lib
	@if [ -n "$(ALL_OBJS)" ]; then \
		ar rcs $@ $(ALL_OBJS); \
		echo "Built: $@ ($(words $(ALL_OBJS)) objects)"; \
	else \
		echo "No source files yet - creating empty archive"; \
		ar rcs $@; \
	fi

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(MODS_OBJ_DIR)/%.o: $(MODS_DIR)/%.cpp
	@mkdir -p $(MODS_OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(MODS_DIR) -c $< -o $@

tidy:
	rm -rf tmp/ lib/vibe.a

# Test binary
TEST_DIR = src/test
TEST_MODS_DIR = src/test/mods
TEST_OBJ_DIR = tmp/obj/test
TEST_MODS_OBJ_DIR = tmp/obj/test/mods
TEST_BIN = tmp/test/test
LABS_A = ../PIPE/lib/labs.a
OPENCV_LIB = $(OPENCV_ROOT)/build/lib

TEST_SRCS = $(wildcard $(TEST_DIR)/*.cpp)
TEST_MODS_SRCS = $(wildcard $(TEST_MODS_DIR)/*.cpp)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.cpp=$(TEST_OBJ_DIR)/%.o)
TEST_MODS_OBJS = $(TEST_MODS_SRCS:$(TEST_MODS_DIR)/%.cpp=$(TEST_MODS_OBJ_DIR)/%.o)
ALL_TEST_OBJS = $(TEST_OBJS) $(TEST_MODS_OBJS)

TEST_INC = $(INCLUDES) -I$(TEST_DIR) -I../PIPE/src/main/part/pipe

test: $(VIBE_A) $(TEST_BIN)
	LD_LIBRARY_PATH=$(OPENCV_LIB) $(TEST_BIN)

$(TEST_BIN): $(ALL_TEST_OBJS) $(VIBE_A)
	@mkdir -p tmp/test
	$(CXX) $(CXXFLAGS) $(ALL_TEST_OBJS) \
		$(VIBE_A) $(LABS_A) \
		$(OPENCV_LIB)/libopencv_imgcodecs.a \
		$(OPENCV_LIB)/libopencv_imgproc.a \
		$(OPENCV_LIB)/libopencv_core.a \
		-lpthread -ldl -lz -lpng -ljpeg -ltiff -lwebp -lsharpyuv \
		-lGL -lGLX \
		-o $@

$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(TEST_OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_INC) -c $< -o $@

$(TEST_MODS_OBJ_DIR)/%.o: $(TEST_MODS_DIR)/%.cpp
	@mkdir -p $(TEST_MODS_OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_INC) -c $< -o $@

# Gold reference generator (pure theory - no VIBE dependency)
GOLD_GEN = tmp/test/gold_gen
GOLD_SRC = src/test/gold/gen.cpp
GOLD_INC = -I$(TEST_DIR) $(OPENCV_INC)

gold: $(GOLD_GEN)
	LD_LIBRARY_PATH=$(OPENCV_LIB) $(GOLD_GEN)

$(GOLD_GEN): $(GOLD_SRC) $(TEST_DIR)/theory.h
	@mkdir -p tmp/test
	$(CXX) $(CXXFLAGS) $(GOLD_INC) $< \
		$(OPENCV_LIB)/libopencv_imgcodecs.a \
		$(OPENCV_LIB)/libopencv_imgproc.a \
		$(OPENCV_LIB)/libopencv_core.a \
		-lpthread -ldl -lz -lpng -ljpeg -ltiff -lwebp -lsharpyuv \
		-lGL -lGLX \
		-o $@

# WGPU shader tests (using Dawn backend)
WGPU_DIR = ../WGPU
WGPU_SRC = $(WGPU_DIR)/lib/dawn
WGPU_BUILD = $(WGPU_SRC)/out/Release

WGPU_INC = -I$(WGPU_DIR)/inc \
           -I$(WGPU_SRC)/include \
           -I$(WGPU_BUILD)/gen/include

WGPU_TEST_DIR = src/test/dawn
WGPU_TEST_BIN = tmp/test/test_dawn
WGPU_TEST_OBJ_DIR = tmp/obj/test/dawn

WGPU_TEST_SRCS = $(wildcard $(WGPU_TEST_DIR)/*.cpp)
WGPU_TEST_OBJS = $(WGPU_TEST_SRCS:$(WGPU_TEST_DIR)/%.cpp=$(WGPU_TEST_OBJ_DIR)/%.o)

# Dawn static libs (all of them)
WGPU_WRAPPER = $(WGPU_DIR)/tmp/dawn.o
WGPU_ALL_LIBS = $(shell find $(WGPU_BUILD) -name "*.a" 2>/dev/null)

# Dawn requires these system libs
WGPU_SYS_LIBS = -lvulkan -ldl -lpthread -lX11

test-dawn: $(WGPU_TEST_BIN)
	$(WGPU_TEST_BIN)

$(WGPU_TEST_BIN): $(WGPU_TEST_OBJS)
	@mkdir -p tmp/test
	$(CXX) $(CXXFLAGS) $(WGPU_TEST_OBJS) \
		$(WGPU_WRAPPER) \
		-Wl,--start-group $(WGPU_ALL_LIBS) -Wl,--end-group \
		$(WGPU_SYS_LIBS) \
		-o $@

$(WGPU_TEST_OBJ_DIR)/%.o: $(WGPU_TEST_DIR)/%.cpp
	@mkdir -p $(WGPU_TEST_OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(WGPU_INC) -I$(WGPU_TEST_DIR) -I$(TEST_DIR) -c $< -o $@

info:
	@echo "Output: $(VIBE_A)"
	@echo "Main sources: $(words $(SRCS))"
	@echo "Mods sources: $(words $(MODS_SRCS))"
	@echo "Total objects: $(words $(ALL_OBJS))"
