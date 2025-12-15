#!/bin/bash
# test.sh - Build, pack, and run in test mode
# Use: bash test.sh        - incremental build
#      bash test.sh clean  - full rebuild
set -e

if [ "$1" = "clean" ]; then
    printf "Cleaning... "
    make tidy
    echo "done"
fi

printf "Building and packing... "
make
echo "done"

echo ""
echo "http://127.0.0.1:4040"
echo ""
echo "Test mode: Enter any email, OTP prints to console"
echo ""

tmp/pack/bin/base --info-file tmp/pack/etc/test.json --data-area tmp/pack/var --wasm-root tmp/pack/www --test
