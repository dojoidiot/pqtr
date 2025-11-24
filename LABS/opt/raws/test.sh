#!/bin/bash

# Test script for RAW image processing pipeline
# Custom GPL-free Sony .ARW decoder with full manufacturer processing

set -e  # Exit on error

echo "╔════════════════════════════════════════════════════════╗"
echo "║  Sony .ARW Custom Decoder Test                         ║"
echo "║  Reference Implementation - Maximum Manufacturer       ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Default input file
INPUT_FILE="${1:-./var/sony_arw2.ARW}"

# Convert to absolute path
INPUT_FILE=$(readlink -f "$INPUT_FILE")

# Extract basename without extension for output
BASENAME=$(basename "$INPUT_FILE" .ARW)
OUTPUT_FILE="./tmp/${BASENAME}.jpg"

echo "Input file: $INPUT_FILE"
echo "Output file: $OUTPUT_FILE"
echo ""

if [ ! -f "$INPUT_FILE" ]; then
    echo "ERROR: Input file not found: $INPUT_FILE"
    echo "Usage: ./test.sh [path/to/raw/file.ARW]"
    exit 1
fi

# Create output directory
mkdir -p ./tmp

# Build pipeline
echo "═══════════════════════════════════════════════════════"
echo "Building custom Sony decoder pipeline..."
echo "═══════════════════════════════════════════════════════"
cd src/main
make clean
make
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi
cd ../..
echo ""

# Run pipeline
echo "═══════════════════════════════════════════════════════"
echo "Running Sony .ARW processing pipeline..."
echo "═══════════════════════════════════════════════════════"
LD_LIBRARY_PATH=../../lib/opencv/build/lib ./tmp/make/pipeline "$INPUT_FILE"
EXIT_CODE=$?
echo ""

if [ $EXIT_CODE -ne 0 ]; then
    echo "ERROR: Pipeline failed"
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

echo "✓ Pipeline completed successfully!"
echo ""
echo "Build artifacts: ./tmp/make/"
echo "Output image: $OUTPUT_FILE"
