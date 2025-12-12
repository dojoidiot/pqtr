#!/bin/bash
# init.sh - Initialize PQTR development environment
# Run once after cloning the repo
set -e

cd "$(dirname "$0")"
ROOT=$(pwd)

echo "=== PQTR Init ==="
echo ""

# 1. Required submodules for test workflow
echo "Initializing submodules..."
git submodule update --init --depth 1 -- \
    LABS/lib/emsdk \
    LABS/lib/imgui \
    BASE/lib/cpp-httplib \
    BASE/lib/libsodium
echo "Submodules ready"

# 2. Emscripten SDK (for LABS WASM build)
if [ ! -f "LABS/lib/emsdk/upstream/emscripten/emcc" ]; then
    echo ""
    echo "Installing Emscripten SDK (this takes a few minutes)..."
    cd LABS/lib/emsdk
    ./emsdk install latest
    ./emsdk activate latest
    cd "$ROOT"
fi
echo "Emscripten ready"

# 3. libsodium (for BASE crypto)
if [ ! -f "BASE/lib/libsodium/src/libsodium/.libs/libsodium.a" ]; then
    echo ""
    echo "Building libsodium..."
    cd BASE/lib/libsodium
    ./autogen.sh
    ./configure
    make -j$(nproc)
    cd "$ROOT"
fi
echo "libsodium ready"

echo ""
echo "=== Init Complete ==="
echo ""
echo "Run:  bash test.sh"
