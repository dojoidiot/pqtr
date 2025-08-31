#!/bin/bash

# PITS Firewall (wall.sh)
# UFW firewall setup for PITS machine
# Allows ports for PITS services + SSH access

set -euo pipefail

# Configuration
SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CONFIG_FILE="$PROJECT_ROOT/etc/pits.conf"
LOG_DIR="/var/log/pits"

# Default ports (can be overridden by config)
SSH_PORT="22"
HTTP_PORT="80"
FTP_CONTROL_PORT="21"
FTP_PASV_MIN="50000"
FTP_PASV_MAX="50100"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging function
log() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "$LOG_DIR/wall.log"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_DIR/wall.log" >&2
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_DIR/wall.log"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_DIR/wall.log"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Check prerequisites
check_prerequisites() {
    local missing_packages=()
    
    # Check for ufw
    if ! command -v ufw &> /dev/null; then
        missing_packages+=("ufw")
    fi
    
    if [[ ${#missing_packages[@]} -gt 0 ]]; then
        info "Installing required packages: ${missing_packages[*]}"
        apt-get update -y
        apt-get install -y "${missing_packages[@]}"
        log "Installed required packages"
    else
        log "Prerequisites check passed"
    fi
}

# Create necessary directories
create_directories() {
    mkdir -p "$LOG_DIR"
    log "Created necessary directories"
}

# Load configuration
load_config() {
    if [[ -f "$CONFIG_FILE" ]]; then
        # Parse config file for port settings
        while IFS= read -r line; do
            # Skip comments and empty lines
            [[ "$line" =~ ^[[:space:]]*# ]] && continue
            [[ -z "${line// }" ]] && continue
            
            # Parse key=value pairs
            if [[ "$line" =~ ^[[:space:]]*([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*=[[:space:]]*(.*)$ ]]; then
                local key="${BASH_REMATCH[1]}"
                local value="${BASH_REMATCH[2]}"
                
                # Remove quotes from value
                value="${value%\"}"
                value="${value#\"}"
                value="${value%\'}"
                value="${value#\'}"
                
                case "$key" in
                    "ftp_control_port")
                        FTP_CONTROL_PORT="$value"
                        ;;
                    "ftp_pasv_min")
                        FTP_PASV_MIN="$value"
                        ;;
                    "ftp_pasv_max")
                        FTP_PASV_MAX="$value"
                        ;;
                    "http_port")
                        HTTP_PORT="$value"
                        ;;
                esac
            fi
        done < "$CONFIG_FILE"
        
        log "Configuration loaded from: $CONFIG_FILE"
    else
        warn "Configuration file not found: $CONFIG_FILE"
        warn "Using default port values"
    fi
    
    # Display port configuration
    echo -e "\n${BLUE}=== Port Configuration ===${NC}"
    echo -e "SSH Port: $SSH_PORT"
    echo -e "HTTP Port: $HTTP_PORT"
    echo -e "FTP Control Port: $FTP_CONTROL_PORT"
    echo -e "FTP Passive Range: $FTP_PASV_MIN-$FTP_PASV_MAX"
}

# Reset UFW to default
reset_ufw() {
    info "Resetting UFW to default state"
    
    # Disable UFW
    ufw --force disable
    
    # Reset to defaults
    ufw --force reset
    
    log "UFW reset completed"
}

# Configure UFW rules
configure_ufw() {
    info "Configuring UFW firewall rules"
    
    # Set default policies
    ufw default deny incoming
    ufw default allow outgoing
    
    # Allow SSH (critical for remote access)
    ufw allow "$SSH_PORT/tcp" comment "SSH access"
    log "Allowed SSH port $SSH_PORT"
    
    # Allow HTTP
    ufw allow "$HTTP_PORT/tcp" comment "HTTP web interface"
    log "Allowed HTTP port $HTTP_PORT"
    
    # Allow FTP control port
    ufw allow "$FTP_CONTROL_PORT/tcp" comment "FTP control"
    log "Allowed FTP control port $FTP_CONTROL_PORT"
    
    # Allow FTP passive ports range
    ufw allow "$FTP_PASV_MIN:$FTP_PASV_MAX/tcp" comment "FTP passive ports"
    log "Allowed FTP passive ports $FTP_PASV_MIN-$FTP_PASV_MAX"
    
    # Allow loopback interface
    ufw allow in on lo comment "Loopback interface"
    log "Allowed loopback interface"
    
    # Allow established connections
    ufw allow established comment "Established connections"
    log "Allowed established connections"
    
    # Allow related connections
    ufw allow related comment "Related connections"
    log "Allowed related connections"
    
    log "UFW rules configuration completed"
}

# Enable UFW
enable_ufw() {
    info "Enabling UFW firewall"
    
    # Enable UFW
    ufw --force enable
    
    if ufw status | grep -q "Status: active"; then
        log "UFW firewall enabled successfully"
    else
        error "Failed to enable UFW firewall"
        exit 1
    fi
}

# Show UFW status
show_ufw_status() {
    echo -e "\n${BLUE}=== UFW Firewall Status ===${NC}"
    ufw status verbose
    
    echo -e "\n${BLUE}=== Active Rules ===${NC}"
    ufw status numbered
    
    echo -e "\n${BLUE}=== Port Summary ===${NC}"
    echo -e "SSH (TCP $SSH_PORT): $(ufw status | grep -q "$SSH_PORT" && echo "✓ ALLOWED" || echo "✗ BLOCKED")"
    echo -e "HTTP (TCP $HTTP_PORT): $(ufw status | grep -q "$HTTP_PORT" && echo "✓ ALLOWED" || echo "✗ BLOCKED")"
    echo -e "FTP Control (TCP $FTP_CONTROL_PORT): $(ufw status | grep -q "$FTP_CONTROL_PORT" && echo "✓ ALLOWED" || echo "✗ BLOCKED")"
    echo -e "FTP Passive ($FTP_PASV_MIN-$FTP_PASV_MAX): $(ufw status | grep -q "$FTP_PASV_MIN" && echo "✓ ALLOWED" || echo "✗ BLOCKED")"
}

# Test firewall connectivity
test_firewall() {
    echo -e "\n${BLUE}=== Testing Firewall Connectivity ===${NC}"
    
    # Test SSH port
    echo -e "Testing SSH port $SSH_PORT..."
    if timeout 5 bash -c "</dev/tcp/localhost/$SSH_PORT" 2>/dev/null; then
        echo -e "✓ SSH port $SSH_PORT is accessible"
    else
        echo -e "✗ SSH port $SSH_PORT is not accessible"
    fi
    
    # Test HTTP port
    echo -e "Testing HTTP port $HTTP_PORT..."
    if timeout 5 bash -c "</dev/tcp/localhost/$HTTP_PORT" 2>/dev/null; then
        echo -e "✓ HTTP port $HTTP_PORT is accessible"
    else
        echo -e "✗ HTTP port $HTTP_PORT is not accessible"
    fi
    
    # Test FTP control port
    echo -e "Testing FTP control port $FTP_CONTROL_PORT..."
    if timeout 5 bash -c "</dev/tcp/localhost/$FTP_CONTROL_PORT" 2>/dev/null; then
        echo -e "✓ FTP control port $FTP_CONTROL_PORT is accessible"
    else
        echo -e "✗ FTP control port $FTP_CONTROL_PORT is not accessible"
    fi
    
    # Test a sample passive port
    local test_pasv_port="$FTP_PASV_MIN"
    echo -e "Testing FTP passive port $test_pasv_port..."
    if timeout 5 bash -c "</dev/tcp/localhost/$test_pasv_port" 2>/dev/null; then
        echo -e "✓ FTP passive port $test_pasv_port is accessible"
    else
        echo -e "✗ FTP passive port $test_pasv_port is not accessible"
    fi
}

# Disable UFW
disable_ufw() {
    info "Disabling UFW firewall"
    
    ufw --force disable
    
    if ufw status | grep -q "Status: inactive"; then
        log "UFW firewall disabled successfully"
    else
        warn "UFW firewall may still be active"
    fi
}

# Show usage
show_usage() {
    cat << EOF
Usage: $SCRIPT_NAME <command>

Commands:
    setup           Setup UFW firewall with PITS rules
    reset           Reset UFW to default state
    status          Show UFW status and rules
    test            Test firewall connectivity
    disable         Disable UFW firewall
    help            Show this help message

Examples:
    $SCRIPT_NAME setup     # Setup firewall with PITS rules
    $SCRIPT_NAME status    # Show firewall status
    $SCRIPT_NAME test      # Test connectivity
    $SCRIPT_NAME disable   # Disable firewall

Requirements:
    - Must be run as root (use sudo)
    - Requires: ufw (will install if missing)

Configuration:
    - SSH Port: $SSH_PORT
    - HTTP Port: $HTTP_PORT
    - FTP Control Port: $FTP_CONTROL_PORT
    - FTP Passive Range: $FTP_PASV_MIN-$FTP_PASV_MAX

Notes:
    - Default policy: deny incoming, allow outgoing
    - SSH access is always allowed for remote management
    - PITS service ports are explicitly allowed
    - Loopback and established connections are allowed
    - Configuration can be customized via pits.conf

Security Features:
    - Stateful firewall with connection tracking
    - Explicit port allowlisting
    - Default deny policy for security
    - Logging of firewall events

EOF
}

# Main function
main() {
    local command="${1:-}"
    
    case "$command" in
        setup)
            check_root
            check_prerequisites
            create_directories
            load_config
            reset_ufw
            configure_ufw
            enable_ufw
            log "PITS firewall setup completed successfully"
            show_ufw_status
            ;;
        reset)
            check_root
            check_prerequisites
            create_directories
            reset_ufw
            log "UFW firewall reset completed"
            ;;
        status)
            show_ufw_status
            ;;
        test)
            test_firewall
            ;;
        disable)
            check_root
            disable_ufw
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
