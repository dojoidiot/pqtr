#!/bin/sh
#
# Build Dawn WebGPU library
# Run from lib directory: ./dawn.sh
#
# Prerequisites (Ubuntu/Debian):
#   sudo apt-get install libxrandr-dev libxinerama-dev libxcursor-dev \
#                        mesa-common-dev libx11-xcb-dev pkg-config python3

set -e

cd "$(dirname "$0")"

echo "=== Building Dawn ==="
echo "Working directory: $(pwd)"

if [ ! -d "dawn" ]; then
    echo "Error: dawn not found. Run: git clone --depth 1 https://dawn.googlesource.com/dawn"
    exit 1
fi

echo "Configuring..."
cmake -S dawn -B dawn/build \
    -DDAWN_FETCH_DEPENDENCIES=ON \
    -DDAWN_ENABLE_INSTALL=ON \
    -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build dawn/build -j$(nproc)

echo "Installing..."
cmake --install dawn/build --prefix dawn/install

echo "=== Done ==="
echo "Headers: dawn/install/include"
echo "Libraries: dawn/install/lib"
