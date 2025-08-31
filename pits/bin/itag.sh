#!/bin/bash

# PITS Identifier Tag Generator (itag.sh)
# Takes an identifier number (inum), applies mask, creates 32-bit integer, then base36 encodes
# Uses characters [a-z,0-9] for base36 encoding

set -euo pipefail

# Script information
SCRIPT_NAME=$(basename "$0")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Base36 character set [a-z,0-9]
BASE36_CHARS="abcdefghijklmnopqrstuvwxyz0123456789"

# Default mask value (can be customized)
DEFAULT_MASK="0xDEADBEEF"

# Logging function
log() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

# Show usage
show_usage() {
    cat << EOF
Usage: $SCRIPT_NAME <inum> [options]

Arguments:
    inum             Identifier number to encode (e.g., "00000001")

Options:
    -m, --mask       Custom mask value (hex, default: $DEFAULT_MASK)
    -v, --verbose    Verbose output showing intermediate steps
    -h, --help       Show this help message

Examples:
    $SCRIPT_NAME "00000001"                    # Basic usage
    $SCRIPT_NAME "00000002" --verbose         # Verbose output
    $SCRIPT_NAME "00000003" -m 0x12345678    # Custom mask

Process:
    1. Take identifier number (inum)
    2. Apply mask operation
    3. Ensure result fits in 32-bit integer
    4. Base36 encode using [a-z,0-9] characters

Output:
    Base36 encoded identifier tag (lowercase letters + digits)

EOF
}

# Convert decimal to base36
decimal_to_base36() {
    local decimal="$1"
    local result=""
    
    # Handle zero case
    if [[ "$decimal" -eq 0 ]]; then
        echo "a"
        return
    fi
    
    # Convert to base36
    while [[ "$decimal" -gt 0 ]]; do
        local remainder=$((decimal % 36))
        result="${BASE36_CHARS:$remainder:1}$result"
        decimal=$((decimal / 36))
    done
    
    echo "$result"
}

# Apply mask operation
apply_mask() {
    local input="$1"
    local mask="$2"
    
    # Convert input string to a numeric value
    # Use advanced hash function with multiple scrambling techniques
    local hash=0
    local i
    for ((i=0; i<${#input}; i++)); do
        local char="${input:$i:1}"
        local ascii=$(printf '%d' "'$char")
        
        # Multiple scrambling operations for better distribution
        # 1. Multiply by prime number and add
        hash=$((hash * 31 + ascii))
        
        # 2. Bit rotation (left and right)
        hash=$((hash ^ (hash << 13)))
        hash=$((hash ^ (hash >> 17)))
        
        # 3. Additional bit manipulation
        hash=$((hash + (hash << 5)))
        hash=$((hash ^ (hash >> 3)))
        
        # 4. Ensure 32-bit bounds
        hash=$((hash & 0xFFFFFFFF))
    done
    
    # Apply additional final scrambling
    hash=$((hash ^ (hash >> 15)))
    hash=$((hash * 0x85ebca6b))
    hash=$((hash ^ (hash >> 13)))
    hash=$((hash * 0xc2b2ae35))
    hash=$((hash ^ (hash >> 16)))
    
    # Apply mask operation
    local masked=$((hash & mask))
    
    # Ensure result fits in 32-bit signed integer
    local max_int=2147483647  # 2^31 - 1
    local result=$((masked % (max_int + 1)))
    
    # Ensure positive (base36 doesn't handle negative well)
    if [[ $result -lt 0 ]]; then
        result=$((-result))
    fi
    
    echo "$result"
}

# Main encoding function
encode_inum() {
    local inum="$1"
    local mask="$2"
    local verbose="$3"
    
    if [[ "$verbose" == "true" ]]; then
        info "Identifier number (inum): '$inum'"
        info "Mask: $mask"
        echo
    fi
    
    # Apply mask
    local numeric_result
    numeric_result=$(apply_mask "$inum" "$mask")
    
    if [[ "$verbose" == "true" ]]; then
        info "Numeric result after mask: $numeric_result"
        echo
    fi
    
    # Convert to base36
    local base36_result
    base36_result=$(decimal_to_base36 "$numeric_result")
    
    if [[ "$verbose" == "true" ]]; then
        info "Base36 encoded result: $base36_result"
        echo
    fi
    
    echo "$base36_result"
}

# Parse command line arguments
parse_arguments() {
    local inum=""
    local mask="$DEFAULT_MASK"
    local verbose="false"
    
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -m|--mask)
                mask="$2"
                shift 2
                ;;
            -v|--verbose)
                verbose="true"
                shift
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            -*)
                error "Unknown option: $1"
                show_usage
                exit 1
                ;;
            *)
                if [[ -z "$inum" ]]; then
                    inum="$1"
                else
                    error "Multiple identifier numbers provided. Only one allowed."
                    exit 1
                fi
                shift
                ;;
        esac
    done
    
    # Validate inum
    if [[ -z "$inum" ]]; then
        error "Identifier number (inum) is required"
        show_usage
        exit 1
    fi
    
    # Validate inum format (8 digits)
    if ! [[ "$inum" =~ ^[0-9]{8}$ ]]; then
        error "Identifier number must be exactly 8 digits (e.g., 00000001)"
        exit 1
    fi
    
    # Validate mask is hex
    if ! [[ "$mask" =~ ^0x[0-9A-Fa-f]+$ ]]; then
        error "Mask must be a valid hex value (e.g., 0xDEADBEEF)"
        exit 1
    fi
    
    # Convert hex to decimal for processing
    mask=$((mask))
    
    echo "$inum|$mask|$verbose"
}

# Main function
main() {
    # Parse arguments
    local parsed_args
    parsed_args=$(parse_arguments "$@")
    
    if [[ $? -ne 0 ]]; then
        exit 1
    fi
    
    # Split parsed arguments
    IFS='|' read -r inum mask verbose <<< "$parsed_args"
    
    # Encode the identifier number
    local result
    result=$(encode_inum "$inum" "$mask" "$verbose")
    
    # Output result
    if [[ "$verbose" == "true" ]]; then
        echo -e "\n${CYAN}=== Final Result ===${NC}"
        echo -e "Identifier Number (inum): '${inum}'"
        echo -e "Identifier Tag: $result"
    else
        echo "$result"
    fi
}

# Run main function with all arguments
main "$@"
