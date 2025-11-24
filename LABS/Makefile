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
INCLUDES = $(OPENCV_INCLUDE) -I./inc -I./opt/raws/src/main
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
RAWS_SRCS = $(OPT_RAWS_DIR)/sony_arw2.cpp \
            $(OPT_RAWS_DIR)/blc.cpp \
            $(OPT_RAWS_DIR)/wb_gain.cpp \
            $(OPT_RAWS_DIR)/demosaic.cpp \
            $(OPT_RAWS_DIR)/gamma_oetf.cpp

# Object files
LABS_OBJ = $(OBJ_DIR)/labs.o
RAWS_OBJS = $(patsubst $(OPT_RAWS_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(RAWS_SRCS))

# Target executable
LABS_BIN = $(BIN_DIR)/labs

.PHONY: all clean

all: $(LABS_BIN)

$(LABS_BIN): $(LABS_OBJ) $(RAWS_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/labs.o: $(LABS_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.o: $(OPT_RAWS_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(TMP_DIR)
