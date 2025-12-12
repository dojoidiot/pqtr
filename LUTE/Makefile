# LUTE Library Makefile
# Builds lib/lute.a - Camera profile LUT module
#
# Dependencies: OpenCV (for cv::UMat)

LUTE_A = lib/lute.a
TEST_BIN = bin/test_lute

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fPIC

# OpenCV (assumes LABS provides it)
OPENCV_ROOT = ../LABS/lib/opencv
OPENCV_INC = -I$(OPENCV_ROOT)/build \
             -I$(OPENCV_ROOT)/modules/core/include \
             -I$(OPENCV_ROOT)/modules/imgproc/include \
             -I$(OPENCV_ROOT)/modules/imgcodecs/include
OPENCV_LIB = $(OPENCV_ROOT)/build/lib

INCLUDES = -I./inc $(OPENCV_INC)

SRC_DIR = src/main
MODS_DIR = src/main/mods
OBJ_DIR = tmp/obj

# Source files
MAIN_SRCS = $(wildcard $(SRC_DIR)/*.cpp)
MODS_SRCS = $(wildcard $(MODS_DIR)/*.cpp)
SRCS = $(MAIN_SRCS) $(MODS_SRCS)

# Object files (flatten into tmp/obj/)
MAIN_OBJS = $(MAIN_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
MODS_OBJS = $(MODS_SRCS:$(MODS_DIR)/%.cpp=$(OBJ_DIR)/mods_%.o)
OBJS = $(MAIN_OBJS) $(MODS_OBJS)

.PHONY: make tidy test lib

make: lib

lib: $(LUTE_A)

$(LUTE_A): $(OBJS)
	@mkdir -p lib
	ar rcs $@ $(OBJS)
	@echo "Built: $@"

# Main source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Mods source files (prefix with mods_ to avoid conflicts)
$(OBJ_DIR)/mods_%.o: $(MODS_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(MODS_DIR) -c $< -o $@

# Test binary
test: $(TEST_BIN)
	LD_LIBRARY_PATH=$(OPENCV_LIB) ./$(TEST_BIN)

$(TEST_BIN): src/test/test_lute.cpp $(LUTE_A)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ \
		$(LUTE_A) \
		$(OPENCV_LIB)/libopencv_imgcodecs.a \
		$(OPENCV_LIB)/libopencv_imgproc.a \
		$(OPENCV_LIB)/libopencv_core.a \
		-ldl -lpthread -lz -ljpeg -lpng -ltiff -lwebp -lsharpyuv \
		-lGL -lGLX

tidy:
	rm -rf tmp/ lib/lute.a bin/

info:
	@echo "Output: $(LUTE_A)"
	@echo "Test: $(TEST_BIN)"
	@echo "Sources: $(SRCS)"
	@echo "Objects: $(OBJS)"
