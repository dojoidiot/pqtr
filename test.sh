#!/bin/bash
# test.sh - Build and run in test mode
#
# Builds all components (same as production), then runs with --test flag

set -e

echo "=== Building LABS ==="
make -C LABS -f Makefile.wasm

echo ""
echo "=== Building BASE ==="
make -C BASE

echo ""
echo "=== Deploying LABS to BASE/www ==="
cp LABS/tmp/wasm/labs.html LABS/tmp/wasm/labs.js LABS/tmp/wasm/labs.wasm BASE/www/

echo ""
echo "=== Setting up test environment ==="
mkdir -p tmp/test

echo ""
echo "=== Starting BASE (test mode) ==="
echo "Open http://127.0.0.1:8080/main.html"
echo "OTP codes will print to console"
echo "Press Ctrl+C to stop"
echo ""

BASE/bin/base --info-file BASE/etc/test.json --data-area tmp/test --test
