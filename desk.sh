#!/bin/bash
# desk.sh - Build and launch DESK GUI
#
# Usage: ./desk.sh [image.ARW]

HERE="$(cd "$(dirname "$0")" && pwd -P)"
cd "$HERE"

echo "=== Clean Build ==="
make -C DESK -f Makefile.desk clean
make || exit 1

echo ""
echo "=== Launching DESK ==="
exec "$HERE/DESK/bin/desk" "$@"
