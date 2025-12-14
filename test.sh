#!/bin/bash
# test.sh - Build and run in test mode
# Use: bash test.sh        - incremental build
#      bash test.sh clean  - full rebuild
set -e

if [ "$1" = "clean" ]; then
    printf "Cleaning... "
    make tidy
    echo "done"
fi

printf "Building... "
make -s
echo "done"

mkdir -p BASE/var/BASE BASE/var/LABS

echo ""
echo "http://127.0.0.1:4040"
echo ""
echo "Test mode: Enter any email, OTP prints to console"
echo ""

BASE/tmp/base --info-file BASE/etc/test.json --data-area BASE/var --wasm-root LABS/tmp/wasm --test
