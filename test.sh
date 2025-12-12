#!/bin/bash
# test.sh - Build and run in test mode
# Use: bash test.sh        - incremental build
#      bash test.sh clean  - full rebuild
set -e

printf "Setting up emsdk... "
source LABS/env.sh > /dev/null 2>&1
echo "done"

if [ "$1" = "clean" ]; then
    printf "Cleaning... "
    rm -rf LABS/tmp BASE/tmp
    echo "done"
fi

printf "Building LABS... "
make -s -C LABS -f Makefile.wasm
echo "done"

printf "Building BASE... "
make -s -C BASE
echo "done"

cp LABS/tmp/wasm/labs.* BASE/www/
mkdir -p tmp/test

echo ""
echo "http://127.0.0.1:4040"
echo ""

BASE/bin/base --info-file BASE/etc/test.json --data-area tmp/test --test
