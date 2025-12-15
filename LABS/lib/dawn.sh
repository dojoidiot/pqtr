#!/bin/sh
#
# Build Dawn WebGPU library
# Run from LABS/lib directory: ./dawn.sh
#
# Prerequisites (Ubuntu/Debian):
#   sudo apt-get install libxrandr-dev libxinerama-dev libxcursor-dev \
#                        mesa-common-dev libx11-xcb-dev pkg-config python3
#
# See: https://github.com/google/dawn/blob/main/docs/quickstart-cmake.md

set -e

cd "$(dirname "$0")"

echo "=== Building Dawn ==="
echo "Working directory: $(pwd)"

# Verify dawn submodule exists
if [ ! -d "dawn" ]; then
    echo "Error: dawn submodule not found. Run: git submodule update --init"
    exit 1
fi

# Configure with CMake
# DAWN_FETCH_DEPENDENCIES=ON downloads deps via Python (no depot_tools needed)
# DAWN_ENABLE_INSTALL=ON allows cmake --install for use by other projects
echo "Configuring..."
cmake -S dawn -B dawn/build \
    -DDAWN_FETCH_DEPENDENCIES=ON \
    -DDAWN_ENABLE_INSTALL=ON \
    -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building..."
cmake --build dawn/build -j$(nproc)

# Install to dawn/install for linking
echo "Installing..."
cmake --install dawn/build --prefix dawn/install

echo "=== Done ==="
echo "Headers: dawn/install/include"
echo "Libraries: dawn/install/lib"
