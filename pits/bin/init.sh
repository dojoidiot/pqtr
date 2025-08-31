#!/bin/bash

# PITS Instance Initializer (init.sh)
# Takes an inum, reads var file, updates with instance data
# Leaves general configuration in pits.conf

set -euo pipefail

# Script information
SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CONFIG_FILE="$PROJECT_ROOT/etc/pits.conf"
VAR_DIR="$PROJECT_ROOT/var"
LOG_DIR="/var/log/pits"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Logging function
log() {
    # Ensure logging directory exists
    mkdir -p "$LOG_DIR"
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "$LOG_DIR/init.log"
}

error() {
    # Ensure logging directory exists
    mkdir -p "$LOG_DIR"
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_DIR/init.log" >&2
}

warn() {
    # Ensure logging directory exists
    mkdir -p "$LOG_DIR"
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_DIR/init.log"
}

info() {
    # Ensure logging directory exists
    mkdir -p "$LOG_DIR"
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_DIR/init.log"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Validate inum format
validate_inum() {
    local inum="$1"
    
    if [[ -z "$inum" ]]; then
        error "Identifier number (inum) is required"
        return 1
    fi
    
    if ! [[ "$inum" =~ ^[0-9]{8}$ ]]; then
        error "Identifier number must be exactly 8 digits (e.g., 00000001)"
        return 1
    fi
    
    return 0
}

# Generate identifier tag using itag.sh
generate_itag() {
    local inum="$1"
    
    if [[ ! -f "$SCRIPT_DIR/itag.sh" ]]; then
        error "itag.sh script not found: $SCRIPT_DIR/itag.sh"
        return 1
    fi
    
    local itag
    itag=$("$SCRIPT_DIR/itag.sh" "$inum")
    
    if [[ $? -eq 0 && -n "$itag" ]]; then
        echo "$itag"
        return 0
    else
        error "Failed to generate identifier tag for inum: $inum"
        return 1
    fi
}

# Create instance configuration file
create_instance_config() {
    local inum="$1"
    local itag="$2"
    local instance_file="$VAR_DIR/${inum}.conf"
    
    info "Creating instance configuration for inum: $inum"
    
    # Create var directory if it doesn't exist
    mkdir -p "$VAR_DIR"
    
    # Generate instance configuration
    cat > "$instance_file" << EOF
# PITS Instance Configuration
# Instance: $inum
# Identifier Tag: $itag
# Generated: $(date -u +'%Y-%m-%d %H:%M:%S UTC')
# Note: All server configuration is in pits.conf, only instance ID here

[instance]
inum = "$inum"
itag = "$itag"
hostname = "$itag"
description = "PITS-ITAG $itag"
created = "$(date -u +'%Y-%m-%d %H:%M:%S UTC')"
status = "active"
EOF
    
    log "Created instance configuration: $instance_file"
}

# Create instance directories
create_instance_directories() {
    local inum="$1"
    local itag="$2"
    
    info "Creating instance directories for inum: $inum (itag: $itag)"
    
    # Since there's only one user, create standard directories
    local dirs=(
        "/var/pits/ftp"
        "/var/pits/instances"
        "/tmp/pits"
        "/var/pits/backups"
        "/var/www/pits"
        "/var/log/pits"
    )
    
    for dir in "${dirs[@]}"; do
        mkdir -p "$dir"
        log "Created directory: $dir"
    done
    
    # Set appropriate permissions
    chown -R root:root "/var/pits/ftp"
    chmod 755 "/var/pits/ftp"
    
    log "Instance directories created successfully"
}



# Update pits.conf with instance-specific overrides
update_pits_conf() {
    local inum="$1"
    local itag="$2"
    
    info "Updating pits.conf with instance $inum overrides"
    
    # Check if pits.conf exists
    if [[ ! -f "$CONFIG_FILE" ]]; then
        warn "pits.conf not found, creating basic configuration"
        create_basic_pits_conf
    fi
    
    # Add instance section if it doesn't exist
    if ! grep -q "\[instance_$inum\]" "$CONFIG_FILE"; then
        cat >> "$CONFIG_FILE" << EOF

# Instance $inum specific overrides
[instance_$inum]
inum = "$inum"
itag = "$itag"
hostname = "PITS-$inum"
EOF
        log "Added instance section to pits.conf"
    fi
}

# Create basic pits.conf if it doesn't exist
create_basic_pits_conf() {
    info "Creating basic pits.conf configuration"
    
    mkdir -p "$(dirname "$CONFIG_FILE")"
    
    cat > "$CONFIG_FILE" << EOF
# PITS Configuration File
# Configuration for Picture Image Transfer System

[project]
name = "PITS"
version = "1.0.0"
description = "Picture Image Transfer System - IoT device for photographers"

[network]
# Access Point Configuration
interface = "wlan0"
bridge = "br0"
bridge_ip = "192.168.4.1"
bridge_network = "192.168.4.0/24"
dhcp_range_start = "192.168.4.10"
dhcp_range_end = "192.168.4.100"
ssid_prefix = "PITS-"

# FTP Configuration
ftp_user = "pftp"
ftp_pass = "pftp123"
ftp_root = "/var/pits/ftp"
ftp_log = "/var/log/pits/pftp.log"

[services]
# Service ports and ranges
ftp_control_port = 21
ftp_pasv_min = 50000
ftp_pasv_max = 50100

# Logging
log_dir = "/var/log/pits"
log_level = "info"

[security]
# User management
chroot_users = true
allow_writeable_chroot = true

# Network security (minimal for field use)
no_nat = true
no_firewall = true
no_tls = true

[paths]
# Directory structure
config_dir = "/etc/pits"
var_dir = "/var/pits"
www_dir = "/var/www/pits"
temp_dir = "/tmp/pits"

[monitoring]
# Health check settings
health_check_interval = 300
max_log_size = "10M"
log_rotation = "daily"
EOF
    
    log "Created basic pits.conf configuration"
}

# Show instance status
show_instance_status() {
    local inum="$1"
    local itag="$2"
    local instance_file="$VAR_DIR/${inum}.conf"
    
    echo -e "\n${CYAN}=== PITS Instance Status ===${NC}"
    echo -e "Instance Number (inum): $inum"
    echo -e "Identifier Tag (itag): $itag"
    echo -e "Instance Config: $instance_file"
    echo -e "Main Config: $CONFIG_FILE"
    
    if [[ -f "$instance_file" ]]; then
        echo -e "Instance Config: ✓ Created"
    else
        echo -e "Instance Config: ✗ Not found"
    fi
    
    if [[ -d "/var/pits/ftp/$itag" ]]; then
        echo -e "Instance Directories: ✓ Created"
    else
        echo -e "Instance Directories: ✗ Not found"
    fi
    
    echo -e "\n${BLUE}=== Quick Start ===${NC}"
    echo -e "Start instance: sudo ./bin/pits.sh start $inum"
    echo -e "Check status: sudo ./bin/pits.sh status"
    echo -e "Test services: sudo ./bin/pits.sh test"
}

# Show usage
show_usage() {
    cat << EOF
Usage: $SCRIPT_NAME <inum> [options]

Arguments:
    inum             Identifier number (8 digits, e.g., 00000001)

Options:
    -f, --force      Force recreation of existing instance
    -v, --verbose    Verbose output
    -h, --help       Show this help message

Examples:
    $SCRIPT_NAME 00000001              # Initialize instance 00000001
    $SCRIPT_NAME 00000002 --force     # Force recreate instance 00000002
    $SCRIPT_NAME 00000003 --verbose   # Verbose initialization

Process:
    1. Validate identifier number (inum)
    2. Generate identifier tag (itag) using itag.sh
    3. Create instance configuration file in var/
    4. Create instance-specific directories
    5. Update pits.conf with instance overrides
    6. Show initialization status

Requirements:
    - Must be run as root (use sudo)
    - Requires: itag.sh script
    - Creates var/ directory structure

Output:
    - Instance configuration file: var/{inum}.conf
    - Instance directories in /var/pits/
    - Updated pits.conf with instance section

EOF
}

# Main function
main() {
    local inum=""
    local force="false"
    local verbose="false"
    
    # Parse options
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -f|--force)
                force="true"
                shift
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
                    error "Multiple inum values provided. Only one allowed."
                    exit 1
                fi
                shift
                ;;
        esac
    done
    
    # Validate inum
    if ! validate_inum "$inum"; then
        show_usage
        exit 1
    fi
    
    # Check if instance already exists
    local instance_file="$VAR_DIR/${inum}.conf"
    if [[ -f "$instance_file" && "$force" != "true" ]]; then
        warn "Instance $inum already exists. Use --force to recreate."
        echo -e "Instance file: $instance_file"
        exit 1
    fi
    
    # Initialize instance
    info "Initializing PITS instance: $inum"
    
    # Generate identifier tag
    local itag
    itag=$(generate_itag "$inum")
    if [[ $? -ne 0 ]]; then
        exit 1
    fi
    
    info "Generated identifier tag: $itag"
    
    # Create instance configuration
    create_instance_config "$inum" "$itag"
    
    # Create instance directories
    create_instance_directories "$inum" "$itag"
    
    # Update pits.conf
    update_pits_conf "$inum" "$itag"
    
    # Show status
    show_instance_status "$inum" "$itag"
    
    log "PITS instance $inum initialized successfully"
    info "Instance $inum ready for use!"
}

# Run main function with all arguments
main "$@"
