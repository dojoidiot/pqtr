#!/bin/bash

# Test script for Gold RAW decoder
# Sony .ARW decoder with maximum manufacturer processing

set -e  # Exit on error

echo "╔════════════════════════════════════════════════════════╗"
echo "║  Gold Sony .ARW Decoder Test                           ║"
echo "║  Maximum Manufacturer Processing                       ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Default input file
INPUT_FILE="${1:-./var/sony.ARW}"

# Output is always sony.png
OUTPUT_FILE="./tmp/sony.png"

echo "Input file: $INPUT_FILE"
echo "Output file: $OUTPUT_FILE"
echo ""

if [ ! -f "$INPUT_FILE" ]; then
    echo "ERROR: Input file not found: $INPUT_FILE"
    echo "Usage: ./sony.sh [path/to/raw/file.ARW]"
    exit 1
fi

# Create output directory
mkdir -p ./tmp

# Build pipeline
echo "═══════════════════════════════════════════════════════"
echo "Building sony decoder..."
echo "═══════════════════════════════════════════════════════"
make -f Makefile.sony clean
make -f Makefile.sony
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi
echo ""

# Run sony from opt/raws directory
echo "═══════════════════════════════════════════════════════"
echo "Running sony decoder test..."
echo "═══════════════════════════════════════════════════════"
LD_LIBRARY_PATH=../../lib/opencv/build/lib ./tmp/sony/sony "$INPUT_FILE"
EXIT_CODE=$?
echo ""

if [ $EXIT_CODE -ne 0 ]; then
    echo "ERROR: Test failed"
    exit 1
fi

# Summary
echo "╔════════════════════════════════════════════════════════╗"
echo "║  Test Complete                                         ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""
echo "Output file: $OUTPUT_FILE"
echo ""

# Show file size
if [ -f "$OUTPUT_FILE" ]; then
    FILE_SIZE=$(stat -c%s "$OUTPUT_FILE")
    echo "File size: $(printf "%'d" $FILE_SIZE) bytes"
fi
echo ""

echo "✓ Sony decoder test completed successfully!"
echo ""
echo "Build artifacts: ./tmp/sony/"
echo "Output image: $OUTPUT_FILE"
