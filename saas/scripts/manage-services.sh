#!/bin/bash

# SaaS Project Service Management Script (Local Installation)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to show usage
show_usage() {
    echo "Usage: $0 {start|stop|restart|status|logs|install}"
    echo ""
    echo "Commands:"
    echo "  start     - Start PostgreSQL and PostgREST services"
    echo "  stop      - Stop PostgREST service (PostgreSQL continues running)"
    echo "  restart   - Restart both services"
    echo "  status    - Show status of all services"
    echo "  logs      - Show PostgREST logs"
    echo "  install   - Install and setup services (first time only)"
    echo ""
    echo "Examples:"
    echo "  $0 start"
    echo "  $0 status"
    echo "  $0 logs"
}

# Function to check if PostgreSQL is running
check_postgres() {
    if systemctl is-active --quiet postgresql; then
        return 0
    else
        return 1
    fi
}

# Function to check if PostgREST is running
check_postgrest() {
    if [ -f postgrest.pid ] && kill -0 $(cat postgrest.pid) 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

# Function to start services
start_services() {
    echo -e "${BLUE}🚀 Starting services...${NC}"
    
    # Start PostgreSQL if not running
    if ! check_postgres; then
        echo "Starting PostgreSQL..."
        sudo systemctl start postgresql
        sudo systemctl enable postgresql
        sleep 2
    else
        echo -e "${GREEN}✅ PostgreSQL is already running${NC}"
    fi
    
    # Start PostgREST if not running
    if ! check_postgrest; then
        echo "Starting PostgREST..."
        nohup postgrest config/postgrest.conf > postgrest.log 2>&1 &
        echo $! > postgrest.pid
        sleep 3
        
        if check_postgrest; then
            echo -e "${GREEN}✅ PostgREST started successfully${NC}"
        else
            echo -e "${RED}❌ Failed to start PostgREST${NC}"
            exit 1
        fi
    else
        echo -e "${GREEN}✅ PostgREST is already running${NC}"
    fi
    
    echo -e "${GREEN}🎉 All services started!${NC}"
}

# Function to stop services
stop_services() {
    echo -e "${BLUE}🛑 Stopping services...${NC}"
    
    # Stop PostgREST
    if check_postgrest; then
        echo "Stopping PostgREST..."
        kill $(cat postgrest.pid)
        rm -f postgrest.pid
        echo -e "${GREEN}✅ PostgREST stopped${NC}"
    else
        echo -e "${YELLOW}⚠️  PostgREST is not running${NC}"
    fi
    
    # Note: PostgreSQL continues running as a system service
    echo -e "${YELLOW}ℹ️  PostgreSQL continues running as a system service${NC}"
    echo "   To stop PostgreSQL: sudo systemctl stop postgresql"
}

# Function to restart services
restart_services() {
    echo -e "${BLUE}🔄 Restarting services...${NC}"
    stop_services
    sleep 2
    start_services
}

# Function to show status
show_status() {
    echo -e "${BLUE}📊 Service Status${NC}"
    echo "=================="
    
    # PostgreSQL status
    echo -n "PostgreSQL: "
    if check_postgres; then
        echo -e "${GREEN}✓ Running${NC}"
        echo "  Service: $(systemctl is-active postgresql)"
        echo "  Port: 5432"
    else
        echo -e "${RED}✗ Not running${NC}"
    fi
    
    # PostgREST status
    echo -n "PostgREST: "
    if check_postgrest; then
        echo -e "${GREEN}✓ Running${NC}"
        echo "  PID: $(cat postgrest.pid)"
        echo "  Port: 3000"
    else
        echo -e "${RED}✗ Not running${NC}"
    fi
    
    # Database connection test
    echo -n "Database: "
    if check_postgres && PGPASSWORD=rest_pass psql -h localhost -U rest_user -d saas_db -c "SELECT 1;" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Connected${NC}"
    else
        echo -e "${RED}✗ Connection failed${NC}"
    fi
    
    # API test
    echo -n "API: "
    if check_postgrest && curl -s http://localhost:3000 > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Responding${NC}"
    else
        echo -e "${RED}✗ Not responding${NC}"
    fi
}

# Function to show logs
show_logs() {
    echo -e "${BLUE}📋 PostgREST Logs${NC}"
    echo "=================="
    
    if [ -f postgrest.log ]; then
        tail -n 50 postgrest.log
    else
        echo -e "${YELLOW}⚠️  No log file found${NC}"
    fi
}

# Function to install services
install_services() {
    echo -e "${BLUE}🔧 Installing services...${NC}"
    
    if [ -f scripts/setup-local.sh ]; then
        echo "Running setup script..."
        ./scripts/setup-local.sh
    else
        echo -e "${RED}❌ Setup script not found${NC}"
        exit 1
    fi
}

# Main script logic
case "$1" in
    start)
        start_services
        ;;
    stop)
        stop_services
        ;;
    restart)
        restart_services
        ;;
    status)
        show_status
        ;;
    logs)
        show_logs
        ;;
    install)
        install_services
        ;;
    *)
        show_usage
        exit 1
        ;;
esac

exit 0
