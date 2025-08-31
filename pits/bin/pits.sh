#!/bin/bash

# PITS - Picture Image Transfer System
# Unified management script for access point and FTP services
# Reads configuration from pits.conf

set -euo pipefail

# Script information
SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CONFIG_FILE="$PROJECT_ROOT/etc/pits.conf"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default configuration values
declare -A CONFIG

# Initialize default config
init_default_config() {
    CONFIG[project_name]="PITS"
    CONFIG[project_version]="1.0.0"
    CONFIG[network_interface]="wlan0"
    CONFIG[network_bridge]="br0"
    CONFIG[network_bridge_ip]="192.168.4.1"
    CONFIG[network_bridge_network]="192.168.4.0/24"
    CONFIG[network_dhcp_range_start]="192.168.4.10"
    CONFIG[network_dhcp_range_end]="192.168.4.100"
    CONFIG[network_ssid_prefix]="PITS-"
    CONFIG[ftp_user]="pftp"
    CONFIG[ftp_pass]="pftp123"
    CONFIG[ftp_root]="/var/pits/ftp"
    CONFIG[ftp_log]="/var/log/pits/pftp.log"
    CONFIG[services_ftp_control_port]="21"
    CONFIG[services_ftp_pasv_min]="50000"
    CONFIG[services_ftp_pasv_max]="50100"
    CONFIG[services_log_dir]="/var/log/pits"
    CONFIG[services_log_level]="info"
    CONFIG[security_chroot_users]="true"
    CONFIG[security_allow_writeable_chroot]="true"
    CONFIG[security_no_nat]="true"
    CONFIG[security_no_firewall]="true"
    CONFIG[security_no_tls]="true"
    CONFIG[paths_config_dir]="/etc/pits"
    CONFIG[paths_var_dir]="/var/pits"
    CONFIG[paths_www_dir]="/var/www/pits"
    CONFIG[paths_temp_dir]="/tmp/pits"
    CONFIG[monitoring_health_check_interval]="300"
    CONFIG[monitoring_max_log_size]="10M"
    CONFIG[monitoring_log_rotation]="daily"
}

# Parse configuration file
parse_config() {
    if [[ ! -f "$CONFIG_FILE" ]]; then
        warn "Configuration file not found: $CONFIG_FILE"
        warn "Using default configuration values"
        return
    fi
    
    local current_section=""
    local line_num=0
    
    while IFS= read -r line; do
        line_num=$((line_num + 1))
        
        # Skip comments and empty lines
        [[ "$line" =~ ^[[:space:]]*# ]] && continue
        [[ -z "${line// }" ]] && continue
        
        # Check for section headers
        if [[ "$line" =~ ^\[([a-zA-Z_][a-zA-Z0-9_]*)\] ]]; then
            current_section="${BASH_REMATCH[1]}"
            continue
        fi
        
        # Parse key=value pairs
        if [[ "$line" =~ ^[[:space:]]*([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*=[[:space:]]*(.*)$ ]]; then
            local key="${BASH_REMATCH[1]}"
            local value="${BASH_REMATCH[2]}"
            
            # Remove quotes from value
            value="${value%\"}"
            value="${value#\"}"
            value="${value%\'}"
            value="${value#\'}"
            
            if [[ -n "$current_section" ]]; then
                CONFIG["${current_section}_${key}"]="$value"
            else
                CONFIG["$key"]="$value"
            fi
        fi
    done < "$CONFIG_FILE"
    
    log "Configuration loaded from: $CONFIG_FILE"
}

# Logging function
log() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "${CONFIG[services_log_dir]}/pits.log"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "${CONFIG[services_log_dir]}/pits.log" >&2
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "${CONFIG[services_log_dir]}/pits.log"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "${CONFIG[services_log_dir]}/pits.log"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Create necessary directories
create_directories() {
    mkdir -p "${CONFIG[services_log_dir]}" "${CONFIG[paths_var_dir]}" "${CONFIG[paths_config_dir]}"
    log "Created necessary directories"
}

# Start access point
start_access_point() {
    local hostname="${1:-}"
    
    if [[ -z "$hostname" ]]; then
        error "Hostname is required for access point"
        return 1
    fi
    
    info "Starting access point with hostname: $hostname"
    
    if [[ -f "$SCRIPT_DIR/apnt.sh" ]]; then
        "$SCRIPT_DIR/apnt.sh" start "$hostname"
        log "Access point started successfully"
    else
        error "apnt.sh script not found: $SCRIPT_DIR/apnt.sh"
        return 1
    fi
}

# Start FTP server
start_ftp_server() {
    info "Starting FTP server"
    
    if [[ -f "$SCRIPT_DIR/pftp.sh" ]]; then
        "$SCRIPT_DIR/pftp.sh" start
        log "FTP server started successfully"
    else
        error "pftp.sh script not found: $SCRIPT_DIR/pftp.sh"
        return 1
    fi
}

# Start HTTP server
start_http_server() {
    info "Starting HTTP server"
    
    if [[ -f "$SCRIPT_DIR/http.sh" ]]; then
        "$SCRIPT_DIR/http.sh" start
        log "HTTP server started successfully"
    else
        error "http.sh script not found: $SCRIPT_DIR/http.sh"
        return 1
    fi
}

# Stop access point
stop_access_point() {
    info "Stopping access point"
    
    if [[ -f "$SCRIPT_DIR/apnt.sh" ]]; then
        "$SCRIPT_DIR/apnt.sh" stop
        log "Access point stopped"
    else
        error "apnt.sh script not found: $SCRIPT_DIR/apnt.sh"
        return 1
    fi
}

# Stop FTP server
stop_ftp_server() {
    info "Stopping FTP server"
    
    if [[ -f "$SCRIPT_DIR/pftp.sh" ]]; then
        "$SCRIPT_DIR/pftp.sh" stop
        log "FTP server stopped"
    else
        error "pftp.sh script not found: $SCRIPT_DIR/pftp.sh"
        return 1
    fi
}

# Stop HTTP server
stop_http_server() {
    info "Stopping HTTP server"
    
    if [[ -f "$SCRIPT_DIR/http.sh" ]]; then
        "$SCRIPT_DIR/http.sh" stop
        log "HTTP server stopped"
    else
        error "http.sh script not found: $SCRIPT_DIR/http.sh"
        return 1
    fi
}

# Start all services
start_all() {
    local hostname="${1:-}"
    
    if [[ -z "$hostname" ]]; then
        error "Hostname is required for start command"
        show_usage
        exit 1
    fi
    
    check_root
    create_directories
    
    log "Starting PITS services with hostname: $hostname"
    
    # Start access point first
    if start_access_point "$hostname"; then
        # Start FTP server
        if start_ftp_server; then
            # Start HTTP server
            if start_http_server; then
                log "All PITS services started successfully"
                show_status
            else
                error "Failed to start HTTP server"
                stop_ftp_server
                stop_access_point
                exit 1
            fi
        else
            error "Failed to start FTP server"
            stop_access_point
            exit 1
        fi
    else
        error "Failed to start access point"
        exit 1
    fi
}

# Stop all services
stop_all() {
    check_root
    log "Stopping all PITS services"
    
    stop_http_server
    stop_ftp_server
    stop_access_point
    
    log "All PITS services stopped"
}

# Show status of all services
show_status() {
    echo -e "\n${CYAN}=== PITS System Status ===${NC}"
    echo -e "Project: ${CONFIG[project_name]} v${CONFIG[project_version]}"
    echo -e "Configuration: $CONFIG_FILE"
    echo -e "Log Directory: ${CONFIG[services_log_dir]}"
    
    echo -e "\n${BLUE}=== Access Point Status ===${NC}"
    if [[ -f "$SCRIPT_DIR/apnt.sh" ]]; then
        "$SCRIPT_DIR/apnt.sh" status
    else
        echo "apnt.sh script not found"
    fi
    
    echo -e "\n${BLUE}=== FTP Server Status ===${NC}"
    if [[ -f "$SCRIPT_DIR/pftp.sh" ]]; then
        "$SCRIPT_DIR/pftp.sh" status
    else
        echo "pftp.sh script not found"
    fi
    
    echo -e "\n${BLUE}=== HTTP Server Status ===${NC}"
    if [[ -f "$SCRIPT_DIR/http.sh" ]]; then
        "$SCRIPT_DIR/http.sh" status
    else
        echo "http.sh script not found"
    fi
    
    echo -e "\n${BLUE}=== System Information ===${NC}"
    echo -e "Hostname: $(hostname)"
    echo -e "Uptime: $(uptime -p 2>/dev/null || echo 'unknown')"
    echo -e "Memory: $(free -h | grep '^Mem:' | awk '{print $3 "/" $2}')"
    echo -e "Disk: $(df -h / | tail -1 | awk '{print $3 "/" $2 " (" $5 " used)"}')"
}

# Test services
test_services() {
    echo -e "\n${CYAN}=== Testing PITS Services ===${NC}"
    
    if [[ -f "$SCRIPT_DIR/test.sh" ]]; then
        "$SCRIPT_DIR/test.sh" all
    else
        echo -e "${RED}test.sh script not found${NC}"
        echo -e "Falling back to basic service status checks..."
        
        echo -e "\n${BLUE}Testing Access Point:${NC}"
        if [[ -f "$SCRIPT_DIR/apnt.sh" ]]; then
            "$SCRIPT_DIR/apnt.sh" status
        fi
        
        echo -e "\n${BLUE}Testing FTP Server:${NC}"
        if [[ -f "$SCRIPT_DIR/pftp.sh" ]]; then
            "$SCRIPT_DIR/pftp.sh" test
        fi
        
        echo -e "\n${BLUE}Testing HTTP Server:${NC}"
        if [[ -f "$SCRIPT_DIR/http.sh" ]]; then
            "$SCRIPT_DIR/http.sh" test
        fi
    fi
}

# Show configuration
show_config() {
    echo -e "\n${CYAN}=== PITS Configuration ===${NC}"
    echo -e "Configuration file: $CONFIG_FILE"
    echo -e ""
    
    for key in "${!CONFIG[@]}"; do
        echo -e "${key}=${CONFIG[$key]}"
    done | sort
}

# Validate configuration
validate_config() {
    echo -e "\n${CYAN}=== Validating PITS Configuration ===${NC}"
    
    local errors=0
    local warnings=0
    
    # Check required files
    if [[ ! -f "$SCRIPT_DIR/apnt.sh" ]]; then
        error "Required script not found: apnt.sh"
        ((errors++))
    fi
    
    if [[ ! -f "$SCRIPT_DIR/pftp.sh" ]]; then
        error "Required script not found: pftp.sh"
        ((errors++))
    fi
    
    if [[ ! -f "$SCRIPT_DIR/http.sh" ]]; then
        error "Required script not found: http.sh"
        ((errors++))
    fi
    
    if [[ ! -f "$SCRIPT_DIR/test.sh" ]]; then
        warn "Test script not found: test.sh"
        ((warnings++))
    fi
    
    if [[ ! -f "$SCRIPT_DIR/wall.sh" ]]; then
        warn "Firewall script not found: wall.sh"
        ((warnings++))
    fi
    
    # Check network configuration
    if [[ -z "${CONFIG[network_interface]}" ]]; then
        warn "Network interface not configured"
        ((warnings++))
    fi
    
    if [[ -z "${CONFIG[network_bridge_ip]}" ]]; then
        warn "Bridge IP not configured"
        ((warnings++))
    fi
    
    # Check FTP configuration
    if [[ -z "${CONFIG[ftp_user]}" ]]; then
        warn "FTP user not configured"
        ((warnings++))
    fi
    
    if [[ -z "${CONFIG[ftp_root]}" ]]; then
        warn "FTP root directory not configured"
        ((warnings++))
    fi
    
    # Summary
    if [[ $errors -eq 0 && $warnings -eq 0 ]]; then
        echo -e "${GREEN}✓ Configuration validation passed${NC}"
    elif [[ $errors -eq 0 ]]; then
        echo -e "${YELLOW}⚠ Configuration validation passed with $warnings warnings${NC}"
    else
        echo -e "${RED}✗ Configuration validation failed with $errors errors and $warnings warnings${NC}"
        return 1
    fi
}

# Show usage
show_usage() {
    cat << EOF
Usage: $SCRIPT_NAME <command> [options]

Commands:
    start <hostname>    Start all PITS services (AP + FTP + HTTP)
    stop               Stop all PITS services
    restart <hostname> Restart all PITS services
    status             Show status of all services
    test               Test all services
    config             Show current configuration
    validate           Validate configuration
    help               Show this help message

Examples:
    $SCRIPT_NAME start PITS-Field01    # Start with hostname PITS-Field01
    $SCRIPT_NAME stop                  # Stop all services
    $SCRIPT_NAME status                # Show status
    $SCRIPT_NAME test                  # Test services
    $SCRIPT_NAME config                # Show configuration
    $SCRIPT_NAME validate              # Validate configuration

Requirements:
    - Must be run as root (use sudo)
    - Requires: apnt.sh, pftp.sh, pits.conf
    - Network interface: ${CONFIG[network_interface]}
    - Bridge IP: ${CONFIG[network_bridge_ip]}

Configuration:
    - Config file: $CONFIG_FILE
    - Log directory: ${CONFIG[services_log_dir]}
    - FTP user: ${CONFIG[ftp_user]}
    - FTP root: ${CONFIG[ftp_root]}

Notes:
    - Hostname is required for start/restart commands
    - SSID will be: ${CONFIG[network_ssid_prefix]}<hostname>
    - Network: ${CONFIG[network_bridge_network]}
    - No NAT, no firewall, no TLS (field-ready)
    - Services: Access Point + FTP + HTTP Web Interface

EOF
}

# Main function
main() {
    local command="${1:-}"
    local hostname="${2:-}"
    
    # Initialize configuration
    init_default_config
    parse_config
    
    case "$command" in
        start)
            start_all "$hostname"
            ;;
        stop)
            stop_all
            ;;
        restart)
            if [[ -z "$hostname" ]]; then
                error "Hostname is required for restart command"
                show_usage
                exit 1
            fi
            stop_all
            sleep 2
            start_all "$hostname"
            ;;
        status)
            show_status
            ;;
        test)
            test_services
            ;;
        config)
            show_config
            ;;
        validate)
            validate_config
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            error "Unknown command: $command"
            show_usage
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"
