#!/bin/bash

# PITS HTTP Server (http.sh)
# Simple nginx web server setup for PITS machine
# No TLS, basic configuration, serves www directory

set -euo pipefail

# Configuration
SCRIPT_NAME=$(basename "$0")
NGINX_CONF="/etc/nginx/sites-available/pits"
NGINX_ENABLED="/etc/nginx/sites-enabled/pits"
WWW_ROOT="/var/www/pits"
LOG_DIR="/var/log/pits"
NGINX_LOG="/var/log/nginx"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging function
log() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "$LOG_DIR/http.log"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_DIR/http.log" >&2
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_DIR/http.log"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_DIR/http.log"
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
    
    # Check for nginx
    if ! command -v nginx &> /dev/null; then
        missing_packages+=("nginx")
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
    mkdir -p "$WWW_ROOT" "$LOG_DIR"
    log "Created necessary directories"
}

# Create sample web page
create_web_page() {
    cat > "$WWW_ROOT/index.html" << 'EOF'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PITS - Picture Image Transfer System</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            line-height: 1.6;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            min-height: 100vh;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: rgba(255, 255, 255, 0.1);
            padding: 30px;
            border-radius: 15px;
            backdrop-filter: blur(10px);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
        }
        h1 {
            text-align: center;
            color: #fff;
            margin-bottom: 30px;
            font-size: 2.5em;
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.5);
        }
        .status-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }
        .status-card {
            background: rgba(255, 255, 255, 0.2);
            padding: 20px;
            border-radius: 10px;
            text-align: center;
            border: 1px solid rgba(255, 255, 255, 0.3);
        }
        .status-card h3 {
            margin: 0 0 15px 0;
            color: #fff;
        }
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-online { background-color: #4CAF50; }
        .status-offline { background-color: #f44336; }
        .info-section {
            background: rgba(255, 255, 255, 0.1);
            padding: 20px;
            border-radius: 10px;
            margin: 20px 0;
        }
        .info-section h3 {
            margin-top: 0;
            color: #fff;
        }
        .info-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
        }
        .info-item {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid rgba(255, 255, 255, 0.2);
        }
        .info-label {
            font-weight: bold;
        }
        .info-value {
            color: #e0e0e0;
        }
        .footer {
            text-align: center;
            margin-top: 30px;
            padding-top: 20px;
            border-top: 1px solid rgba(255, 255, 255, 0.3);
            color: #e0e0e0;
        }
        @media (max-width: 600px) {
            .info-grid {
                grid-template-columns: 1fr;
            }
            .status-grid {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>📸 PITS</h1>
        <p style="text-align: center; font-size: 1.2em; margin-bottom: 30px;">
            Picture Image Transfer System - IoT Device for Photographers
        </p>

        <div class="status-grid">
            <div class="status-card">
                <h3>Access Point</h3>
                <div>
                    <span class="status-indicator status-online"></span>
                    <span id="ap-status">Online</span>
                </div>
                <p>WiFi Hotspot Active</p>
            </div>
            <div class="status-card">
                <h3>FTP Server</h3>
                <div>
                    <span class="status-indicator status-online"></span>
                    <span id="ftp-status">Online</span>
                </div>
                <p>Photo Upload Ready</p>
            </div>
            <div class="status-card">
                <h3>Web Server</h3>
                <div>
                    <span class="status-indicator status-online"></span>
                    <span id="web-status">Online</span>
                </div>
                <p>Management Interface</p>
            </div>
        </div>

        <div class="info-section">
            <h3>System Information</h3>
            <div class="info-grid">
                <div class="info-item">
                    <span class="info-label">Device Name:</span>
                    <span class="info-value" id="hostname">Loading...</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Uptime:</span>
                    <span class="info-value" id="uptime">Loading...</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Network:</span>
                    <span class="info-value" id="network">192.168.4.0/24</span>
                </div>
                <div class="info-item">
                    <span class="info-label">FTP User:</span>
                    <span class="info-value">pftp</span>
                </div>
            </div>
        </div>

        <div class="info-section">
            <h3>Quick Actions</h3>
            <div style="text-align: center;">
                <p>Use the PITS management script to control services:</p>
                <code style="background: rgba(0,0,0,0.3); padding: 10px; border-radius: 5px; display: block; margin: 10px 0;">
                    sudo ./bin/pits.sh status
                </code>
                <p>Connect your camera via FTP or phone via WiFi to get started!</p>
            </div>
        </div>

        <div class="footer">
            <p>PITS v1.0.0 - Field-Ready IoT Device</p>
            <p>No NAT • No Firewall • No TLS • Simple & Secure</p>
        </div>
    </div>

    <script>
        // Simple status update simulation
        function updateStatus() {
            // Update hostname
            document.getElementById('hostname').textContent = window.location.hostname || 'PITS-Device';
            
            // Update uptime (simplified)
            const startTime = Date.now();
            setInterval(() => {
                const uptime = Math.floor((Date.now() - startTime) / 1000);
                const hours = Math.floor(uptime / 3600);
                const minutes = Math.floor((uptime % 3600) / 60);
                const seconds = uptime % 60;
                document.getElementById('uptime').textContent = 
                    `${hours}h ${minutes}m ${seconds}s`;
            }, 1000);
        }
        
        // Initialize when page loads
        document.addEventListener('DOMContentLoaded', updateStatus);
    </script>
</body>
</html>
EOF
    
    log "Created sample web page: $WWW_ROOT/index.html"
}

# Generate nginx configuration
generate_nginx_conf() {
    cat > "$NGINX_CONF" << EOF
# PITS HTTP Server Configuration
# Generated by $SCRIPT_NAME on $(date)

server {
    listen 80;
    server_name _;
    root $WWW_ROOT;
    index index.html index.htm;
    
    # Logging
    access_log $NGINX_LOG/pits_access.log;
    error_log $NGINX_LOG/pits_error.log;
    
    # Security headers
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    
    # Serve static files
    location / {
        try_files \$uri \$uri/ =404;
    }
    
    # Handle 404 errors
    error_page 404 /404.html;
    
    # Basic security
    location ~ /\. {
        deny all;
    }
    
    # Gzip compression
    gzip on;
    gzip_vary on;
    gzip_min_length 1024;
    gzip_types text/plain text/css text/xml text/javascript application/javascript application/xml+rss application/json;
}
EOF
    
    log "Generated nginx configuration: $NGINX_CONF"
}

# Enable nginx site
enable_nginx_site() {
    # Remove default site if it exists
    if [[ -L /etc/nginx/sites-enabled/default ]]; then
        rm /etc/nginx/sites-enabled/default
        log "Removed default nginx site"
    fi
    
    # Create symlink to enable PITS site
    ln -sf "$NGINX_CONF" "$NGINX_ENABLED"
    log "Enabled PITS nginx site"
    
    # Test nginx configuration
    if nginx -t; then
        log "Nginx configuration test passed"
    else
        error "Nginx configuration test failed"
        exit 1
    fi
}

# Start nginx service
start_nginx_service() {
    # Enable and start nginx
    systemctl enable nginx
    systemctl restart nginx
    
    if systemctl is-active --quiet nginx; then
        log "Started nginx service"
    else
        error "Failed to start nginx service"
        exit 1
    fi
}

# Show status
show_status() {
    echo -e "\n${BLUE}=== PITS HTTP Server Status ===${NC}"
    echo -e "Web Root: $WWW_ROOT"
    echo -e "Nginx Config: $NGINX_CONF"
    echo -e "Log Directory: $LOG_DIR"
    
    echo -e "\n${BLUE}=== Service Status ===${NC}"
    echo -e "nginx: $(systemctl is-active nginx 2>/dev/null || echo 'inactive')"
    
    echo -e "\n${BLUE}=== Web Root Contents ===${NC}"
    if [[ -d "$WWW_ROOT" ]]; then
        ls -la "$WWW_ROOT" || echo "Cannot list web root directory"
    else
        echo "Web root directory not found"
    fi
    
    echo -e "\n${BLUE}=== Connection Info ===${NC}"
    echo -e "HTTP Port: 80"
    echo -e "Local URL: http://localhost"
    echo -e "Network URL: http://$(hostname -I | awk '{print $1}')"
    echo -e "No TLS/SSL encryption"
    
    echo -e "\n${BLUE}=== Nginx Configuration ===${NC}"
    echo -e "Main Config: /etc/nginx/nginx.conf"
    echo -e "Site Config: $NGINX_CONF"
    echo -e "Enabled Sites: /etc/nginx/sites-enabled/"
}

# Stop nginx service
stop_nginx() {
    log "Stopping PITS HTTP server..."
    
    # Stop service
    systemctl stop nginx 2>/dev/null || true
    systemctl disable nginx 2>/dev/null || true
    
    # Remove site configuration
    if [[ -L "$NGINX_ENABLED" ]]; then
        rm "$NGINX_ENABLED"
        log "Removed PITS nginx site"
    fi
    
    log "HTTP service stopped"
}

# Test HTTP connection
test_http() {
    echo -e "\n${BLUE}=== Testing HTTP Connection ===${NC}"
    echo -e "Testing local HTTP connection..."
    
    if command -v curl &> /dev/null; then
        echo -e "Using 'curl' command:"
        echo -e "  curl -I http://localhost"
        echo -e "  curl http://localhost"
    fi
    
    if command -v wget &> /dev/null; then
        echo -e "\nUsing 'wget' command:"
        echo -e "  wget -qO- http://localhost"
    fi
    
    echo -e "\n${YELLOW}Note: Test from another device using this machine's IP address${NC}"
    echo -e "  http://$(hostname -I | awk '{print $1}')"
}

# Show usage
show_usage() {
    cat << EOF
Usage: $SCRIPT_NAME <command>

Commands:
    start       Start HTTP server
    stop        Stop HTTP server
    status      Show current HTTP status
    restart     Restart HTTP server
    test        Test HTTP connection
    help        Show this help message

Examples:
    $SCRIPT_NAME start     # Start HTTP server
    $SCRIPT_NAME stop      # Stop HTTP server
    $SCRIPT_NAME status    # Show status
    $SCRIPT_NAME test      # Test connection

Requirements:
    - Must be run as root (use sudo)
    - Requires: nginx (will install if missing)

Configuration:
    - Web Root: $WWW_ROOT
    - HTTP Port: 80
    - Nginx Config: $NGINX_CONF
    - No TLS/SSL encryption
    - Simple, responsive web interface

Notes:
    - No firewall rules are configured
    - No TLS/SSL encryption
    - Direct HTTP access
    - Serves files from www directory
    - Responsive design for mobile devices

EOF
}

# Main function
main() {
    local command="${1:-}"
    
    case "$command" in
        start)
            check_root
            check_prerequisites
            create_directories
            create_web_page
            generate_nginx_conf
            enable_nginx_site
            start_nginx_service
            log "PITS HTTP server started successfully"
            show_status
            ;;
        stop)
            check_root
            stop_nginx
            ;;
        restart)
            check_root
            stop_nginx
            sleep 2
            $SCRIPT_NAME start
            ;;
        status)
            show_status
            ;;
        test)
            test_http
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
