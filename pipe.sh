#!/bin/bash
# labs.sh - Run PIPE processor
#
# Usage: ./labs.sh <source.ARW> [options]

HERE="$(cd "$(dirname "$0")" && pwd -P)"
exec "$HERE/PIPE/bin/labs" "$@"
