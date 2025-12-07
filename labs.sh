#!/bin/bash
# labs.sh - Run LABS processor
#
# Usage: ./labs.sh <source.ARW> [options]

HERE="$(cd "$(dirname "$0")" && pwd -P)"
exec "$HERE/LABS/bin/labs" "$@"
