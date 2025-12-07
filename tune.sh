#!/bin/bash
# tune.sh - Run TUNE optimizer
#
# Usage: ./tune.sh <source.ARW> <target.png|preview> [options]

HERE="$(cd "$(dirname "$0")" && pwd -P)"
exec "$HERE/LABS/bin/tune" "$@"
