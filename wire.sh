#!/bin/bash
# wire.sh - Dependency wiring for PQTR projects
#
# Creates symlinks between projects for headers (inc), source (src), and libraries (lib).
# Run from repository root. Idempotent - safe to run multiple times.
#
# Usage: ./wire.sh [--unwire]
#
# Model:
#   WIRE <FROM> <type> <INTO>
#   - FROM: source project (provides the artifact)
#   - type: inc|src|lib
#   - INTO: target project (consumes the artifact)
#
# Assumptions:
#   - Projects use UPPERCASE names (PIPE, DESK, GEAR)
#   - Each project has standard subdirs: inc/, src/, lib/
#   - Libraries are static archives named <project>.a (lowercase)
#   - Script runs from repository root

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd -P)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

log_ok()    { echo -e "${GREEN}✓${NC} $1"; }
log_warn()  { echo -e "${YELLOW}!${NC} $1"; }
log_err()   { echo -e "${RED}✗${NC} $1" >&2; }

# Validate project exists
check_project() {
    local proj="$1"
    if [ ! -d "$HERE/$proj" ]; then
        log_err "Project not found: $proj"
        return 1
    fi
}

# inc: link headers - INTO/inc/FROM -> FROM/inc
inc() {
    local from="$1" into="$2"
    echo "$HERE/$into/inc/$from" "$HERE/$from/inc"
}

# src: link source - INTO/src/FROM -> FROM/src
src() {
    local from="$1" into="$2"
    echo "$HERE/$into/src/$from" "$HERE/$from/src"
}

# lib: link library - INTO/lib/FROM.a -> FROM/lib/from.a
lib() {
    local from="$1" into="$2"
    local name="${from,,}"  # lowercase for .a file
    echo "$HERE/$into/lib/$from.a" "$HERE/$from/lib/$name.a"
}

WIRE() {
    local from="$1" type="$2" into="$3"

    # Validate projects exist
    check_project "$from" || return 1
    check_project "$into" || return 1

    # Validate type
    if ! declare -f "$type" > /dev/null; then
        log_err "Unknown wire type: $type (expected: inc, src, lib)"
        return 1
    fi

    read -r link_path source_path <<< "$($type "$from" "$into")"

    # For lib type, warn if source doesn't exist but create symlink anyway
    # (symlink will work once the library is built)
    local lib_missing=false
    if [ "$type" = "lib" ] && [ ! -f "$source_path" ]; then
        lib_missing=true
    fi

    # For inc/src, check source dir exists
    if [ "$type" != "lib" ] && [ ! -d "$source_path" ]; then
        log_warn "Source dir not found: $source_path"
        return 0
    fi

    local base="$(dirname "$link_path")"
    mkdir -p "$base"

    if [ -L "$link_path" ]; then
        if [ "$lib_missing" = true ]; then
            log_warn "exists (target missing, build first): $from -> $into ($type)"
        else
            log_ok "exists: $from -> $into ($type)"
        fi
    else
        ln -s "$source_path" "$link_path"
        local ignr="${link_path#$HERE}"
        grep -qxF "$ignr" "$HERE/.gitignore" 2>/dev/null || echo "$ignr" >> "$HERE/.gitignore"
        if [ "$lib_missing" = true ]; then
            log_warn "linked (target missing, build first): $from -> $into ($type)"
        else
            log_ok "linked: $from -> $into ($type)"
        fi
    fi
}

UNWIRE() {
    local from="$1" type="$2" into="$3"

    if ! declare -f "$type" > /dev/null; then
        return 1
    fi

    read -r link_path _ <<< "$($type "$from" "$into")"

    if [ -L "$link_path" ]; then
        rm "$link_path"
        log_ok "removed: $link_path"
    fi
}

# --- Wiring Rules ---
# Format: WIRE <FROM> <type> <INTO>
#   FROM provides, INTO consumes

run_wires() {
    local cmd="${1:-WIRE}"

    $cmd GEAR inc PIPE   # PIPE includes GEAR headers (API)
    $cmd GEAR lib PIPE   # PIPE links GEAR library
    $cmd PIPE lib DESK   # DESK links PIPE library
}

# --- Main ---

if [ "${1:-}" = "--unwire" ]; then
    echo "Removing wires..."
    run_wires UNWIRE
else
    echo "Wiring projects..."
    run_wires WIRE
fi
