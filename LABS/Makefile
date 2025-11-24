# LABS Makefile
# Builds bin/labs executable

# Compiler
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# OpenCV paths (static linking to lib/opencv)
OPENCV_ROOT = ./lib/opencv
OPENCV_INCLUDE = -I$(OPENCV_ROOT)/build \
                 -I$(OPENCV_ROOT)/modules/core/include \
                 -I$(OPENCV_ROOT)/modules/imgproc/include \
                 -I$(OPENCV_ROOT)/modules/imgcodecs/include
OPENCV_LIB = $(OPENCV_ROOT)/build/lib

# Include and library flags
INCLUDES = $(OPENCV_INCLUDE) -I./inc -I./opt/raws/src/main/part
LDFLAGS = -L$(OPENCV_LIB)
LIBS = -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -Wl,-rpath,$(OPENCV_LIB)

# Directories
SRC_DIR = src/main
OPT_RAWS_DIR = opt/raws/src/main
TMP_DIR = tmp/labs
BIN_DIR = $(TMP_DIR)/bin
OBJ_DIR = $(TMP_DIR)/obj

# Source files
LABS_SRC = $(SRC_DIR)/labs.cpp
RAWS_SRCS = $(OPT_RAWS_DIR)/part/sony.cpp \
            $(OPT_RAWS_DIR)/part/sony/prepare.cpp \
            $(OPT_RAWS_DIR)/part/sony/process.cpp \
            $(OPT_RAWS_DIR)/part/sony/demosaic.cpp \
            $(OPT_RAWS_DIR)/part/sony/blc.cpp \
            $(OPT_RAWS_DIR)/part/sony/wb_gain.cpp \
            $(OPT_RAWS_DIR)/part/sony/gamma_oetf.cpp

# Object files
LABS_OBJ = $(OBJ_DIR)/labs.o
RAWS_OBJS = $(OBJ_DIR)/part/sony.o \
            $(OBJ_DIR)/part/sony/prepare.o \
            $(OBJ_DIR)/part/sony/process.o \
            $(OBJ_DIR)/part/sony/demosaic.o \
            $(OBJ_DIR)/part/sony/blc.o \
            $(OBJ_DIR)/part/sony/wb_gain.o \
            $(OBJ_DIR)/part/sony/gamma_oetf.o

# Target executable
LABS_BIN = $(BIN_DIR)/labs

.PHONY: all clean

all: $(LABS_BIN)

$(LABS_BIN): $(LABS_OBJ) $(RAWS_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/labs.o: $(LABS_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/part/sony.o: $(OPT_RAWS_DIR)/part/sony.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/part/sony/%.o: $(OPT_RAWS_DIR)/part/sony/%.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(TMP_DIR)
