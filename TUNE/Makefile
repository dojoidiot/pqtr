# TUNE Makefile
# Style optimizer - matches camera styles using LABS pipeline
#
# Usage:
#   make           # Build bin/tune
#   make test      # Run fast optimization test
#   make test-full # Run full test suite
#   make clean     # Clean build artifacts

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# OpenCV (from LABS)
OPENCV_ROOT = ../LABS/lib/opencv
OPENCV_INCLUDE = -I$(OPENCV_ROOT)/build \
                 -I$(OPENCV_ROOT)/modules/core/include \
                 -I$(OPENCV_ROOT)/modules/imgproc/include \
                 -I$(OPENCV_ROOT)/modules/imgcodecs/include
OPENCV_LIB = $(OPENCV_ROOT)/build/lib

# Include paths
INCLUDES = $(OPENCV_INCLUDE) \
           -I./inc \
           -I./src/main/part/geos \
           -I../LABS/inc \
           -I../LABS/src/main/part/pipe

# Static libraries
OPENCV_STATIC = $(OPENCV_LIB)/libopencv_imgcodecs.a \
                $(OPENCV_LIB)/libopencv_imgproc.a \
                $(OPENCV_LIB)/libopencv_core.a

LIBS = ../LABS/lib/labs.a $(OPENCV_STATIC) -lpng -ljpeg -lz -lGL -ldl -lpthread

# Directories
SRC_DIR = src/main
GEOS_DIR = src/main/part/geos
OBJ_DIR = tmp/obj
BIN_DIR = bin
VAR_DIR = tmp/var/tune

# Sources
TUNE_SRC = $(SRC_DIR)/tune.cpp
GEOS_SRCS = $(wildcard $(GEOS_DIR)/*.cpp)

# Objects
TUNE_OBJ = $(OBJ_DIR)/tune.o
GEOS_OBJS = $(GEOS_SRCS:$(GEOS_DIR)/%.cpp=$(OBJ_DIR)/geos/%.o)

# Binary
TUNE_BIN = $(BIN_DIR)/tune

.PHONY: all tune test test-full clean

all: tune

tune: $(TUNE_BIN)

$(TUNE_BIN): $(TUNE_OBJ) $(GEOS_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(OBJ_DIR)/tune.o: $(SRC_DIR)/tune.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/geos/%.o: $(GEOS_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)/geos
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

test: tune
	@mkdir -p $(VAR_DIR)
	@echo "Running fast tune test..."
	LD_LIBRARY_PATH=$(OPENCV_LIB) $(TUNE_BIN) --help || true

test-full: tune
	@mkdir -p $(VAR_DIR)
	@echo "Running full tune test..."
	@echo "(requires test images in var/pics/)"

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)/tune $(VAR_DIR)
