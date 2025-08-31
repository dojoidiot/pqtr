#!/bin/bash

# PITS Test Suite (test.sh)
# Comprehensive testing for all PITS services
# Tests access point, FTP, and HTTP functionality

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
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "${CONFIG[services_log_dir]}/test.log"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "${CONFIG[services_log_dir]}/test.log" >&2
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "${CONFIG[services_log_dir]}/test.log"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "${CONFIG[services_log_dir]}/test.log"
}

# Test access point functionality
test_access_point() {
    echo -e "\n${BLUE}=== Testing Access Point ===${NC}"
    
    if [[ ! -f "$SCRIPT_DIR/apnt.sh" ]]; then
        error "apnt.sh script not found"
        return 1
    fi
    
    # Check if hostapd is running
    if systemctl is-active --quiet hostapd; then
        echo -e "✓ hostapd service is running"
    else
        echo -e "✗ hostapd service is not running"
        return 1
    fi
    
    # Check if dnsmasq is running
    if systemctl is-active --quiet dnsmasq; then
        echo -e "✓ dnsmasq service is running"
    else
        echo -e "✗ dnsmasq service is not running"
        return 1
    fi
    
    # Check bridge interface
    if ip link show "${CONFIG[network_bridge]}" &> /dev/null; then
        echo -e "✓ Bridge interface ${CONFIG[network_bridge]} exists"
        local bridge_ip=$(ip addr show "${CONFIG[network_bridge]}" | grep -oP 'inet \K\S+')
        if [[ -n "$bridge_ip" ]]; then
            echo -e "✓ Bridge IP: $bridge_ip"
        else
            echo -e "✗ Bridge has no IP address"
        fi
    else
        echo -e "✗ Bridge interface ${CONFIG[network_bridge]} not found"
        return 1
    fi
    
    # Check wireless interface
    if ip link show "${CONFIG[network_interface]}" &> /dev/null; then
        echo -e "✓ Wireless interface ${CONFIG[network_interface]} exists"
    else
        echo -e "✗ Wireless interface ${CONFIG[network_interface]} not found"
        return 1
    fi
    
    # Check hostname
    local hostname=$(hostname)
    if [[ -n "$hostname" ]]; then
        echo -e "✓ System hostname: $hostname"
    else
        echo -e "✗ System hostname not set"
    fi
    
    log "Access point test completed"
}

# Test FTP server functionality
test_ftp_server() {
    echo -e "\n${BLUE}=== Testing FTP Server ===${NC}"
    
    if [[ ! -f "$SCRIPT_DIR/pftp.sh" ]]; then
        error "pftp.sh script not found"
        return 1
    fi
    
    # Check if vsftpd is running
    if systemctl is-active --quiet vsftpd; then
        echo -e "✓ vsftpd service is running"
    else
        echo -e "✗ vsftpd service is not running"
        return 1
    fi
    
    # Check FTP user
    if id "${CONFIG[ftp_user]}" &> /dev/null; then
        echo -e "✓ FTP user ${CONFIG[ftp_user]} exists"
    else
        echo -e "✗ FTP user ${CONFIG[ftp_user]} not found"
        return 1
    fi
    
    # Check FTP root directory
    if [[ -d "${CONFIG[ftp_root]}" ]]; then
        echo -e "✓ FTP root directory exists: ${CONFIG[ftp_root]}"
        local permissions=$(ls -ld "${CONFIG[ftp_root]}" | awk '{print $1}')
        echo -e "  Permissions: $permissions"
    else
        echo -e "✗ FTP root directory not found: ${CONFIG[ftp_root]}"
        return 1
    fi
    
    # Check FTP ports
    local ftp_port="${CONFIG[services_ftp_control_port]}"
    if netstat -tlnp 2>/dev/null | grep -q ":$ftp_port "; then
        echo -e "✓ FTP control port $ftp_port is listening"
    else
        echo -e "✗ FTP control port $ftp_port is not listening"
        return 1
    fi
    
    # Test local FTP connection
    if command -v curl &> /dev/null; then
        echo -e "Testing local FTP connection..."
        if curl -s --connect-timeout 5 "ftp://localhost/" --user "${CONFIG[ftp_user]}:${CONFIG[ftp_pass]}" &> /dev/null; then
            echo -e "✓ Local FTP connection successful"
        else
            echo -e "✗ Local FTP connection failed"
        fi
    else
        echo -e "⚠ curl not available for FTP testing"
    fi
    
    log "FTP server test completed"
}

# Test HTTP server functionality
test_http_server() {
    echo -e "\n${BLUE}=== Testing HTTP Server ===${NC}"
    
    if [[ ! -f "$SCRIPT_DIR/http.sh" ]]; then
        error "http.sh script not found"
        return 1
    fi
    
    # Check if nginx is running
    if systemctl is-active --quiet nginx; then
        echo -e "✓ nginx service is running"
    else
        echo -e "✗ nginx service is not running"
        return 1
    fi
    
    # Check web root directory
    if [[ -d "${CONFIG[paths_www_dir]}" ]]; then
        echo -e "✓ Web root directory exists: ${CONFIG[paths_www_dir]}"
        if [[ -f "${CONFIG[paths_www_dir]}/index.html" ]]; then
            echo -e "✓ index.html file exists"
        else
            echo -e "✗ index.html file not found"
        fi
    else
        echo -e "✗ Web root directory not found: ${CONFIG[paths_www_dir]}"
        return 1
    fi
    
    # Check HTTP port
    if netstat -tlnp 2>/dev/null | grep -q ":80 "; then
        echo -e "✓ HTTP port 80 is listening"
    else
        echo -e "✗ HTTP port 80 is not listening"
        return 1
    fi
    
    # Test local HTTP connection
    if command -v curl &> /dev/null; then
        echo -e "Testing local HTTP connection..."
        local http_response=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost/")
        if [[ "$http_response" == "200" ]]; then
            echo -e "✓ Local HTTP connection successful (HTTP $http_response)"
        else
            echo -e "✗ Local HTTP connection failed (HTTP $http_response)"
        fi
    else
        echo -e "⚠ curl not available for HTTP testing"
    fi
    
    log "HTTP server test completed"
}

# Test network connectivity
test_network() {
    echo -e "\n${BLUE}=== Testing Network Connectivity ===${NC}"
    
    # Check IP forwarding
    local ip_forward=$(cat /proc/sys/net/ipv4/ip_forward 2>/dev/null || echo "0")
    if [[ "$ip_forward" == "1" ]]; then
        echo -e "✓ IP forwarding is enabled"
    else
        echo -e "✗ IP forwarding is disabled"
    fi
    
    # Check network interfaces
    echo -e "Network interfaces:"
    ip addr show | grep -E '^[0-9]+:' | while read -r line; do
        local iface=$(echo "$line" | cut -d: -f2 | tr -d ' ')
        local state=$(ip link show "$iface" 2>/dev/null | grep -o 'state [A-Z]\+' | cut -d' ' -f2)
        if [[ -n "$state" ]]; then
            echo -e "  $iface: $state"
        fi
    done
    
    # Check routing table
    echo -e "Routing table:"
    ip route show | head -5 | while read -r route; do
        echo -e "  $route"
    done
    
    log "Network connectivity test completed"
}

# Test system resources
test_system() {
    echo -e "\n${BLUE}=== Testing System Resources ===${NC}"
    
    # Check disk space
    local disk_usage=$(df -h / | tail -1 | awk '{print $5}' | tr -d '%')
    if [[ "$disk_usage" -lt 90 ]]; then
        echo -e "✓ Disk usage: ${disk_usage}% (OK)"
    else
        echo -e "⚠ Disk usage: ${disk_usage}% (High)"
    fi
    
    # Check memory usage
    local mem_info=$(free -h | grep '^Mem:')
    local mem_total=$(echo "$mem_info" | awk '{print $2}')
    local mem_used=$(echo "$mem_info" | awk '{print $3}')
    local mem_available=$(echo "$mem_info" | awk '{print $7}')
    echo -e "Memory: $mem_used / $mem_total (Available: $mem_available)"
    
    # Check system load
    local load_avg=$(uptime | grep -o 'load average: [0-9.]*' | cut -d' ' -f3)
    echo -e "Load average: $load_avg"
    
    # Check uptime
    local uptime_info=$(uptime -p 2>/dev/null || echo "unknown")
    echo -e "System uptime: $uptime_info"
    
    log "System resources test completed"
}

# Run all tests
run_all_tests() {
    echo -e "\n${CYAN}=== PITS Comprehensive Test Suite ===${NC}"
    echo -e "Running all tests for PITS services..."
    
    local test_results=()
    
    # Test access point
    if test_access_point; then
        test_results+=("Access Point: ✓ PASS")
    else
        test_results+=("Access Point: ✗ FAIL")
    fi
    
    # Test FTP server
    if test_ftp_server; then
        test_results+=("FTP Server: ✓ PASS")
    else
        test_results+=("FTP Server: ✗ FAIL")
    fi
    
    # Test HTTP server
    if test_http_server; then
        test_results+=("HTTP Server: ✓ PASS")
    else
        test_results+=("HTTP Server: ✗ FAIL")
    fi
    
    # Test network
    if test_network; then
        test_results+=("Network: ✓ PASS")
    else
        test_results+=("Network: ✗ FAIL")
    fi
    
    # Test system
    if test_system; then
        test_results+=("System: ✓ PASS")
    else
        test_results+=("System: ✗ FAIL")
    fi
    
    # Summary
    echo -e "\n${CYAN}=== Test Summary ===${NC}"
    for result in "${test_results[@]}"; do
        echo -e "$result"
    done
    
    # Count results
    local passed=$(echo "${test_results[@]}" | grep -o "✓ PASS" | wc -l)
    local failed=$(echo "${test_results[@]}" | grep -o "✗ FAIL" | wc -l)
    local total=${#test_results[@]}
    
    echo -e "\nResults: $passed/$total tests passed"
    
    if [[ $failed -eq 0 ]]; then
        echo -e "${GREEN}🎉 All tests passed! PITS is ready for use.${NC}"
        log "All tests passed successfully"
        return 0
    else
        echo -e "${RED}⚠ $failed test(s) failed. Please check the issues above.${NC}"
        log "$failed test(s) failed"
        return 1
    fi
}

# Show usage
show_usage() {
    cat << EOF
Usage: $SCRIPT_NAME <command>

Commands:
    all              Run all tests (default)
    ap               Test access point only
    ftp              Test FTP server only
    http             Test HTTP server only
    network          Test network connectivity only
    system           Test system resources only
    help             Show this help message

Examples:
    $SCRIPT_NAME              # Run all tests
    $SCRIPT_NAME all         # Run all tests
    $SCRIPT_NAME ap          # Test access point only
    $SCRIPT_NAME ftp         # Test FTP server only
    $SCRIPT_NAME http        # Test HTTP server only

Requirements:
    - Requires: apnt.sh, pftp.sh, http.sh, pits.conf
    - Tests all PITS services and system resources

Notes:
    - Tests are comprehensive and check service status
    - Network tests verify connectivity and routing
    - System tests check resources and performance
    - Results are logged to test.log

EOF
}

# Main function
main() {
    local command="${1:-all}"
    
    # Initialize configuration
    init_default_config
    parse_config
    
    case "$command" in
        all|"")
            run_all_tests
            ;;
        ap)
            test_access_point
            ;;
        ftp)
            test_ftp_server
            ;;
        http)
            test_http_server
            ;;
        network)
            test_network
            ;;
        system)
            test_system
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
