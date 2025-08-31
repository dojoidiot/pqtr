#!/bin/bash

# PITS Hotspot Connection Script (hots.sh)
# Connects this machine to a PITS hotspot for testing
# Automatically detects and restores original network state

set -euo pipefail

# Configuration
SCRIPT_NAME=$(basename "$0")
VAR_DIR="../var"
LOG_DIR="/tmp/pits"
LOG_FILE="$LOG_DIR/hots.log"
STATE_FILE="/tmp/pits/network_state.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Logging functions
log() {
    mkdir -p "$LOG_DIR"
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "$LOG_FILE"
}

error() {
    mkdir -p "$LOG_DIR"
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_FILE" >&2
}

warn() {
    mkdir -p "$LOG_DIR"
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_FILE"
}

info() {
    mkdir -p "$LOG_DIR"
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

# Check prerequisites
check_prerequisites() {
    local missing_packages=()
    
    for package in nmcli ip; do
        if ! command -v "$package" &> /dev/null; then
            missing_packages+=("$package")
        fi
    done
    
    if [[ ${#missing_packages[@]} -gt 0 ]]; then
        error "Missing required packages: ${missing_packages[*]}"
        error "Install with: sudo apt-get install ${missing_packages[*]}"
        exit 1
    fi
    
    if ! systemctl is-active --quiet NetworkManager; then
        error "NetworkManager is not running. Start with: sudo systemctl start NetworkManager"
        exit 1
    fi
    
    log "Prerequisites check passed"
}

# Parse instance configuration
parse_instance_config() {
    local inum="$1"
    local config_file="$VAR_DIR/${inum}.conf"
    
    if [[ ! -f "$config_file" ]]; then
        error "Instance configuration file not found: $config_file"
        exit 1
    fi
    
    local hostname=$(grep "^hostname = " "$config_file" | cut -d'"' -f2)
    if [[ -z "$hostname" ]]; then
        error "Could not find hostname in configuration file"
        exit 1
    fi
    
    echo "PITS-${hostname}"
}

# Save current network state
save_network_state() {
    info "Saving current network state..."
    
    mkdir -p "$(dirname "$STATE_FILE")"
    
    # Save active connections
    nmcli connection show --active > "$STATE_FILE"
    
    # Detect connection type
    local wifi_conns=$(nmcli -t -f NAME,DEVICE,TYPE connection show --active | grep "wlan" | grep -v "PITS" | cut -d: -f1 | head -1)
    local wired_conns=$(nmcli -t -f NAME,DEVICE,TYPE connection show --active | grep "ethernet" | cut -d: -f1 | head -1)
    
    if [[ -n "$wifi_conns" ]]; then
        echo "WIFI:$wifi_conns" >> "$STATE_FILE"
    fi
    
    if [[ -n "$wired_conns" ]]; then
        echo "WIRED:$wired_conns" >> "$STATE_FILE"
    fi
    
    log "Network state saved to: $STATE_FILE"
}

# Connect to PITS hotspot
connect_to_hotspot() {
    local ssid="$1"
    
    info "Connecting to PITS hotspot: $ssid"
    
    # Scan for networks
    nmcli device wifi rescan
    sleep 2
    
    # Check if hotspot is available
    if ! nmcli -t -f SSID device wifi list | grep -q "$ssid"; then
        error "Hotspot $ssid not found. Ensure PITS device is running apnt.sh"
        return 1
    fi
    
    # Connect to open network
    if nmcli device wifi connect "$ssid"; then
        log "Connected to PITS hotspot: $ssid"
        return 0
    else
        error "Failed to connect to PITS hotspot"
        return 1
    fi
}

# Test connectivity to PITS device
test_connectivity() {
    local gateway="192.168.4.1"
    
    info "Testing connectivity to PITS device..."
    
    # Wait for IP assignment
    sleep 3
    
    # Show connection status
    echo -e "\n${BLUE}=== Connection Status ===${NC}"
    nmcli connection show --active | grep "PITS" || echo "PITS connection not found"
    
    # Show IP address
    local wifi_ip=$(ip addr show | grep "inet.*wlan" | head -1 | awk '{print $2}' | cut -d'/' -f1)
    if [[ -n "$wifi_ip" ]]; then
        log "WiFi IP: $wifi_ip"
    else
        error "No WiFi IP address found"
        return 1
    fi
    
    # Test connectivity
    if ping -c 2 -W 3 "$gateway" &> /dev/null; then
        log "Successfully pinged PITS device at $gateway"
    else
        warn "Could not ping PITS device (may not respond to pings)"
    fi
    
    # Test HTTP access
    if command -v curl &> /dev/null; then
        if curl -s --connect-timeout 5 "http://$gateway" &> /dev/null; then
            log "HTTP access successful"
        else
            warn "HTTP access failed (service may not be running)"
        fi
    fi
    
    echo -e "\n${GREEN}=== Ready for Testing ===${NC}"
    echo -e "PITS Device: $gateway"
    echo -e "Network: 192.168.4.0/24"
    echo -e "Use '$SCRIPT_NAME down' to restore original network"
}

# Restore original network state
restore_network_state() {
    info "Restoring original network state..."
    
    if [[ ! -f "$STATE_FILE" ]]; then
        error "No network state file found. Cannot restore."
        return 1
    fi
    
    # Disconnect PITS WiFi
    local pits_conns=$(nmcli -t -f NAME,DEVICE connection show --active | grep "PITS" | cut -d: -f1)
    if [[ -n "$pits_conns" ]]; then
        for conn in $pits_conns; do
            nmcli connection down "$conn" 2>/dev/null || true
        done
    fi
    
    # Restore original connections
    local wifi_conn=$(grep "^WIFI:" "$STATE_FILE" | cut -d: -f2)
    local wired_conn=$(grep "^WIRED:" "$STATE_FILE" | cut -d: -f2)
    
    if [[ -n "$wired_conn" ]]; then
        info "Restoring wired connection: $wired_conn"
        nmcli connection up "$wired_conn" 2>/dev/null || true
    fi
    
    if [[ -n "$wifi_conn" ]]; then
        info "Restoring WiFi connection: $wifi_conn"
        nmcli connection up "$wifi_conn" 2>/dev/null || true
    fi
    
    # Wait for stabilization
    sleep 3
    
    # Show restored status
    echo -e "\n${BLUE}=== Restored Network Status ===${NC}"
    nmcli connection show --active
    
    # Test connectivity
    if ping -c 2 -W 3 8.8.8.8 &> /dev/null; then
        log "Internet connectivity restored"
    else
        warn "Internet connectivity test failed"
    fi
    
    # Cleanup
    rm -f "$STATE_FILE"
    log "Network state restored and cleaned up"
}

# Show usage
show_usage() {
    cat << EOF
Usage: $SCRIPT_NAME <command> [inum]

Commands:
    connect <inum>    Connect to PITS hotspot for instance inum
    down              Restore original network state
    status            Show current network status
    test <inum>      Test connectivity to PITS device
    help              Show this help message

Examples:
    $SCRIPT_NAME connect 00000001    # Connect to PITS hotspot
    $SCRIPT_NAME down                # Restore original network
    $SCRIPT_NAME status              # Show network status
    $SCRIPT_NAME test 00000001       # Test PITS connectivity

Requirements:
    - NetworkManager must be running
    - Instance configuration file must exist in var/{inum}.conf
    - This machine must have WiFi capability

Notes:
    - Automatically detects and saves your current network state
    - PITS hotspot must be running (apnt.sh) on target device
    - Use 'down' command to restore your original network when done

EOF
}

# Main function
main() {
    local command="${1:-}"
    local inum="${2:-}"
    
    case "$command" in
        connect)
            if [[ -z "$inum" ]]; then
                error "Instance number (inum) is required for connect command"
                show_usage
                exit 1
            fi
            
            check_prerequisites
            save_network_state
            
            local ssid=$(parse_instance_config "$inum")
            if connect_to_hotspot "$ssid"; then
                test_connectivity
            else
                exit 1
            fi
            ;;
            
        down)
            check_prerequisites
            restore_network_state
            ;;
            
        status)
            check_prerequisites
            echo -e "\n${BLUE}=== Network Status ===${NC}"
            nmcli connection show --active
            echo -e "\n${BLUE}=== Device Status ===${NC}"
            nmcli device status
            ;;
            
        test)
            if [[ -z "$inum" ]]; then
                error "Instance number (inum) is required for test command"
                show_usage
                exit 1
            fi
            
            check_prerequisites
            local ssid=$(parse_instance_config "$inum")
            test_connectivity
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

# Run main function
main "$@"
