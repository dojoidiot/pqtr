#!/bin/bash

# OpenCV Build Script
# This script configures and builds OpenCV from the submodule
# git submodule update --init bind/lib/opencv
set -e  # Exit on error

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OPENCV_DIR="$SCRIPT_DIR/opencv"
BUILD_DIR="$OPENCV_DIR/build"

if [ ! -d "$OPENCV_DIR/cmake" ]; then
    echo "OpenCV source directory not found at $OPENCV_DIR"
    echo "Fetching OpenCV submodule..."
    cd "$(git rev-parse --show-toplevel)"
    git submodule update --init --depth 1 bind/lib/opencv

    if [ ! -d "$OPENCV_DIR/cmake" ]; then
        echo "Failed to fetch OpenCV submodule"
        exit 1
    fi
    cd "$SCRIPT_DIR"
fi

# Create build directory
echo "Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring OpenCV with CMake (with OpenCL support)..."
cmake \
  -D CMAKE_BUILD_TYPE=Release \
  -D CMAKE_INSTALL_PREFIX=/usr/local \
  -D OPENCV_GENERATE_PKGCONFIG=YES \
  -D BUILD_EXAMPLES=OFF \
  -D BUILD_TESTS=OFF \
  -D BUILD_PERF_TESTS=OFF \
  -D BUILD_DOCS=OFF \
  -D WITH_TBB=ON \
  -D WITH_EIGEN=ON \
  -D WITH_V4L=ON \
  -D WITH_OPENGL=ON \
  -D WITH_OPENCL=ON \
  "$OPENCV_DIR"

# Build using all available cores
echo "Building OpenCV..."
make -j$(nproc)

echo "Build complete!"
echo "To install system-wide, run: sudo make install"
echo "Build artifacts are in: $BUILD_DIR"
