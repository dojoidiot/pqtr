#!/bin/bash
# Source this to set up LABS build environment
# Usage: source env.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/emsdk/emsdk_env.sh"
