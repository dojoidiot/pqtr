#!/bin/bash
# Full test suite: tune + labs on all images in var/pics/
# Outputs to tmp/var/pics/<name>/ with tune.json, labs_output.png, and debug artifacts

set -e

cd "$(dirname "$0")/.."

export LD_LIBRARY_PATH=lib/opencv/build/lib:$LD_LIBRARY_PATH

echo "=== LABS Full Test Suite ==="
echo ""

# Full clean build
echo "=== BUILD ==="
make clean
make tune
echo ""

# Clean tmp/ and create output dir
rm -rf tmp/*
mkdir -p tmp/var/pics

# Tune all images
for img in var/pics/*.ARW; do
    name=$(basename "$img" .ARW)
    echo "=== TUNE: $name ==="
    mkdir -p "tmp/var/pics/$name"
    timeout 300 bin/tune "$img" preview --save-area "tmp/var/pics/$name" 2>&1 | tail -25
    echo ""
done

echo "=== TUNE COMPLETE ==="
echo ""

# Apply labs on each
for img in var/pics/*.ARW; do
    name=$(basename "$img" .ARW)
    tune_json="tmp/var/pics/$name/tune.json"
    if [ -f "$tune_json" ]; then
        echo "=== LABS: $name ==="
        bin/labs "$img" --tune "$tune_json" --output "tmp/var/pics/$name/tail.png" --size 1080 --debug 2>&1 | tail -10
        echo ""
    else
        echo "=== LABS: $name - SKIPPED (no tune.json) ==="
    fi
done

echo "=== ALL COMPLETE ==="
echo ""
echo "Results summary:"
for dir in tmp/var/pics/*/; do
    name=$(basename "$dir")
    files=$(ls "$dir" 2>/dev/null | wc -l)
    echo "  $name: $files files"
done
echo ""
echo "Output: tmp/var/pics/"
