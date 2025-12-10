# VIBE Library Makefile
# Builds lib/vibe.a - Style processing module (45 dials + LUTs)
#
# Dependencies: OpenCV (for cv::UMat)

VIBE_A = lib/vibe.a

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fPIC

# OpenCV (assumes LABS provides it)
OPENCV_ROOT = ../LABS/lib/opencv
OPENCV_INC = -I$(OPENCV_ROOT)/build \
             -I$(OPENCV_ROOT)/modules/core/include \
             -I$(OPENCV_ROOT)/modules/imgproc/include \
             -I$(OPENCV_ROOT)/modules/imgcodecs/include

INCLUDES = -I./inc $(OPENCV_INC)

SRC_DIR = src/main
OBJ_DIR = tmp/obj

# Source files (add as implemented)
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

.PHONY: make tidy

make: $(VIBE_A)

$(VIBE_A): $(OBJS)
	@mkdir -p lib
	@if [ -n "$(OBJS)" ]; then \
		ar rcs $@ $(OBJS); \
		echo "Built: $@"; \
	else \
		echo "No source files yet - creating empty archive"; \
		ar rcs $@; \
	fi

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

tidy:
	rm -rf tmp/ lib/vibe.a

info:
	@echo "Output: $(VIBE_A)"
	@echo "Sources: $(SRCS)"
	@echo "Objects: $(OBJS)"
