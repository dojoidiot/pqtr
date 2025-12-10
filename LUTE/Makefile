# LUTE Library Makefile
# Builds lib/lute.a - Camera profile LUT module
#
# Dependencies: OpenCV (for cv::UMat)

LUTE_A = lib/lute.a

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

make: $(LUTE_A)

$(LUTE_A): $(OBJS)
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
	rm -rf tmp/ lib/lute.a

info:
	@echo "Output: $(LUTE_A)"
	@echo "Sources: $(SRCS)"
	@echo "Objects: $(OBJS)"
