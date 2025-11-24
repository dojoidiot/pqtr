# Makefile for C++ RAW processing pipeline
# Uses production modules with custom GPL-free Sony decoder

# Compiler
CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2

# OpenCV paths (relative to src/main/)
OPENCV_ROOT ?= ../../../../lib/opencv
OPENCV_INCLUDE = -I$(OPENCV_ROOT)/build \
                 -I$(OPENCV_ROOT)/modules/core/include \
                 -I$(OPENCV_ROOT)/modules/imgproc/include \
                 -I$(OPENCV_ROOT)/modules/imgcodecs/include
OPENCV_LIB = $(OPENCV_ROOT)/build/lib

# Build output directory
TMP_DIR = ../../tmp
BUILD_DIR = $(TMP_DIR)/make
OBJ_DIR = $(BUILD_DIR)/obj

# Include and library flags
INCLUDES = $(OPENCV_INCLUDE) -I.
LDFLAGS = -L$(OPENCV_LIB)
LIBS = -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -Wl,-rpath,$(OPENCV_LIB)

# Source files (production modules)
SOURCES = main.cpp \
          sony_arw2.cpp \
          blc.cpp \
          wb_gain.cpp \
          demosaic.cpp \
          gamma_oetf.cpp

OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)
EXECUTABLE = $(BUILD_DIR)/pipeline

# Default target
all: $(EXECUTABLE)

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Link
$(EXECUTABLE): $(OBJECTS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Compile
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -rf $(TMP_DIR)

# Phony targets
.PHONY: all clean

# Dependencies (headers)
$(OBJ_DIR)/main.o: sony_arw2.h blc.h wb_gain.h demosaic.h gamma_oetf.h module.h
$(OBJ_DIR)/sony_arw2.o: sony_arw2.h module.h
$(OBJ_DIR)/blc.o: blc.h sony_arw2.h module.h
$(OBJ_DIR)/wb_gain.o: wb_gain.h sony_arw2.h module.h
$(OBJ_DIR)/demosaic.o: demosaic.h sony_arw2.h module.h
$(OBJ_DIR)/gamma_oetf.o: gamma_oetf.h module.h
