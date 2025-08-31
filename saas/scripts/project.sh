#!/bin/bash

# SaaS Project Unified Manager
# Single script to manage all project operations

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Project configuration
PROJECT_NAME="SaaS Project"
PROJECT_VERSION="1.0.0"

# Function to show banner
show_banner() {
    echo -e "${CYAN}"
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║                    $PROJECT_NAME v$PROJECT_VERSION                    ║"
    echo "║                PostgreSQL + PostgREST Backend               ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 {command} [options]"
    echo ""
    echo "Commands:"
    echo "  setup     - Complete project setup (first time)"
    echo "  start     - Start all services"
    echo "  stop      - Stop all services"
    echo "  restart   - Restart all services"
    echo "  status    - Show service status"
    echo "  test      - Test API endpoints"
    echo "  logs      - Show service logs"
    echo "  env       - Environment management"
    echo "  secure    - Encrypt/decrypt environment files"
    echo "  backup    - Create/restore backups"
    echo "  clean     - Clean up project files"
    echo "  uninstall - Remove project completely"
    echo "  health    - Run health checks"
    echo "  help      - Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 setup              # First time setup"
    echo "  $0 start              # Start services"
    echo "  $0 env dev            # Switch to dev environment"
    echo "  $0 secure encrypt     # Encrypt environment files"
    echo "  $0 health             # Run health checks"
}

# Function to check prerequisites
check_prerequisites() {
    echo -e "${BLUE}🔍 Checking prerequisites...${NC}"
    
    local missing=()
    
    # Check PostgreSQL
    if ! command -v psql >/dev/null 2>&1; then
        missing+=("PostgreSQL")
    fi
    
    # Check PostgREST
    if ! command -v postgrest >/dev/null 2>&1; then
        missing+=("PostgREST")
    fi
    
    # Check GPG
    if ! command -v gpg >/dev/null 2>&1; then
        missing+=("GPG")
    fi
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo -e "${RED}❌ Missing prerequisites: ${missing[*]}${NC}"
        echo ""
        echo "Install missing components:"
        echo "  PostgreSQL: sudo apt install postgresql postgresql-contrib"
        echo "  PostgREST: Download from https://github.com/PostgREST/postgrest"
        echo "  GPG: sudo apt install gnupg"
        return 1
    fi
    
    echo -e "${GREEN}✅ All prerequisites are available${NC}"
    return 0
}

# Function to setup project
setup_project() {
    echo -e "${BLUE}🚀 Setting up $PROJECT_NAME...${NC}"
    
    if ! check_prerequisites; then
        exit 1
    fi
    
    # Run the setup script
    if [ -f "scripts/setup-local.sh" ]; then
        ./scripts/setup-local.sh
    else
        echo -e "${RED}❌ Setup script not found${NC}"
        exit 1
    fi
}

# Function to manage services
manage_services() {
    local action=$1
    
    case $action in
        start)
            echo -e "${BLUE}🚀 Starting services...${NC}"
            ./scripts/manage-services.sh start
            ;;
        stop)
            echo -e "${BLUE}🛑 Stopping services...${NC}"
            ./scripts/manage-services.sh stop
            ;;
        restart)
            echo -e "${BLUE}🔄 Restarting services...${NC}"
            ./scripts/manage-services.sh restart
            ;;
        status)
            ./scripts/manage-services.sh status
            ;;
        logs)
            ./scripts/manage-services.sh logs
            ;;
        *)
            echo -e "${RED}❌ Invalid service action: $action${NC}"
            echo "Valid actions: start, stop, restart, status, logs"
            exit 1
            ;;
    esac
}

# Function to test API
test_api() {
    echo -e "${BLUE}🧪 Testing API endpoints...${NC}"
    
    if [ -f "scripts/test-api-local.sh" ]; then
        ./scripts/test-api-local.sh
    else
        echo -e "${RED}❌ Test script not found${NC}"
        exit 1
    fi
}

# Function to manage environment
manage_environment() {
    local action=$1
    
    if [ -f "scripts/env-manager.sh" ]; then
        case $action in
            dev|tst|pro)
                ./scripts/env-manager.sh "$action"
                ;;
            show|validate|create)
                ./scripts/env-manager.sh "$action"
                ;;
            *)
                echo -e "${RED}❌ Invalid environment action: $action${NC}"
                echo "Valid actions: dev, tst, pro, show, validate, create"
                exit 1
                ;;
        esac
    else
        echo -e "${RED}❌ Environment manager not found${NC}"
        exit 1
    fi
}

# Function to manage security
manage_security() {
    local action=$1
    
    if [ -f "scripts/safe.sh" ]; then
        case $action in
            encrypt|decrypt|backup|restore|list|clean)
                ./scripts/safe.sh "$action"
                ;;
            *)
                echo -e "${RED}❌ Invalid security action: $action${NC}"
                echo "Valid actions: encrypt, decrypt, backup, restore, list, clean"
                exit 1
                ;;
        esac
    else
        echo -e "${RED}❌ Security script not found${NC}"
        exit 1
    fi
}

# Function to create backup
create_backup() {
    echo -e "${BLUE}💾 Creating project backup...${NC}"
    
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local backup_dir="backups/backup_$timestamp"
    
    mkdir -p "$backup_dir"
    
    # Backup configuration files
    if [ -d "config" ]; then
        cp -r config "$backup_dir/"
        echo "✅ Configuration files backed up"
    fi
    
    # Backup environment files (if decrypted)
    if [ -f ".env" ]; then
        cp .env "$backup_dir/"
        echo "✅ Current environment backed up"
    fi
    
    # Backup scripts
    if [ -d "scripts" ]; then
        cp -r scripts "$backup_dir/"
        echo "✅ Scripts backed up"
    fi
    
    # Create backup info
    cat > "$backup_dir/backup_info.txt" << EOF
Backup created: $(date)
Project: $PROJECT_NAME
Version: $PROJECT_VERSION
Backup type: Manual
EOF
    
    echo -e "${GREEN}✅ Backup created: $backup_dir${NC}"
}

# Function to restore backup
restore_backup() {
    echo -e "${BLUE}📥 Restoring from backup...${NC}"
    
    local backup_dir=$1
    
    if [ -z "$backup_dir" ]; then
        echo "Available backups:"
        if [ -d "backups" ]; then
            ls -1 backups/ | grep "^backup_" | sort -r
        else
            echo "No backups found"
            exit 1
        fi
        
        read -p "Enter backup directory name: " backup_dir
    fi
    
    if [ ! -d "backups/$backup_dir" ]; then
        echo -e "${RED}❌ Backup directory not found: backups/$backup_dir${NC}"
        exit 1
    fi
    
    echo "Restoring from: backups/$backup_dir"
    
    # Restore configuration
    if [ -d "backups/$backup_dir/config" ]; then
        cp -r "backups/$backup_dir/config" .
        echo "✅ Configuration restored"
    fi
    
    # Restore environment
    if [ -f "backups/$backup_dir/.env" ]; then
        cp "backups/$backup_dir/.env" .
        echo "✅ Environment restored"
    fi
    
    echo -e "${GREEN}✅ Backup restored successfully${NC}"
}

# Function to clean project
clean_project() {
    echo -e "${BLUE}🧹 Cleaning project files...${NC}"
    
    local removed=0
    
    # Remove log files
    if [ -f "postgrest.log" ]; then
        rm postgrest.log
        echo "✅ Removed postgrest.log"
        removed=$((removed + 1))
    fi
    
    # Remove PID files
    if [ -f "postgrest.pid" ]; then
        rm postgrest.pid
        echo "✅ Removed postgrest.pid"
        removed=$((removed + 1))
    fi
    
    # Remove temporary files
    for file in .env.backup .env.current; do
        if [ -f "$file" ]; then
            rm "$file"
            echo "✅ Removed $file"
            removed=$((removed + 1))
        fi
    done
    
    if [ $removed -eq 0 ]; then
        echo -e "${YELLOW}⚠️  No files to clean${NC}"
    else
        echo -e "${GREEN}✅ Cleaned $removed files${NC}"
    fi
}

# Function to uninstall project
uninstall_project() {
    echo -e "${RED}🗑️  Uninstalling $PROJECT_NAME...${NC}"
    
    read -p "Are you sure you want to uninstall? This will remove all data! (y/N): " confirm
    if [[ ! $confirm =~ ^[Yy]$ ]]; then
        echo "Uninstall cancelled"
        exit 0
    fi
    
    if [ -f "scripts/uninstall-local.sh" ]; then
        ./scripts/uninstall-local.sh
    else
        echo -e "${RED}❌ Uninstall script not found${NC}"
        exit 1
    fi
}

# Function to run health checks
run_health_checks() {
    echo -e "${BLUE}🏥 Running health checks...${NC}"
    
    local checks_passed=0
    local total_checks=0
    
    # Check PostgreSQL
    total_checks=$((total_checks + 1))
    if systemctl is-active --quiet postgresql; then
        echo -e "${GREEN}✅ PostgreSQL: Running${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}❌ PostgreSQL: Not running${NC}"
    fi
    
    # Check PostgREST
    total_checks=$((total_checks + 1))
    if [ -f "postgrest.pid" ] && kill -0 $(cat postgrest.pid) 2>/dev/null; then
        echo -e "${GREEN}✅ PostgREST: Running${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}❌ PostgREST: Not running${NC}"
    fi
    
    # Check database connection
    total_checks=$((total_checks + 1))
    if [ -f ".env" ] && PGPASSWORD=$(grep "^POSTGRES_PASSWORD=" .env | cut -d'=' -f2) psql -h localhost -U $(grep "^POSTGRES_USER=" .env | cut -d'=' -f2) -d $(grep "^POSTGRES_DB=" .env | cut -d'=' -f2) -c "SELECT 1;" >/dev/null 2>&1; then
        echo -e "${GREEN}✅ Database: Connected${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}❌ Database: Connection failed${NC}"
    fi
    
    # Check API
    total_checks=$((total_checks + 1))
    if curl -s http://localhost:3000 >/dev/null 2>&1; then
        echo -e "${GREEN}✅ API: Responding${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}❌ API: Not responding${NC}"
    fi
    
    echo ""
    echo -e "${BLUE}📊 Health Check Results:${NC}"
    echo "Passed: $checks_passed/$total_checks"
    
    if [ $checks_passed -eq $total_checks ]; then
        echo -e "${GREEN}🎉 All health checks passed!${NC}"
    else
        echo -e "${YELLOW}⚠️  Some health checks failed${NC}"
        echo "Run '$0 status' for more details"
    fi
}

# Function to show project info
show_project_info() {
    echo -e "${BLUE}📋 Project Information:${NC}"
    echo "Name: $PROJECT_NAME"
    echo "Version: $PROJECT_VERSION"
    echo "Directory: $(pwd)"
    echo ""
    
    # Show current environment
    if [ -f ".env" ]; then
        echo -e "${BLUE}Current Environment:${NC}"
        if [ -f "scripts/env-manager.sh" ]; then
            ./scripts/env-manager.sh show 2>/dev/null || echo "   (Environment manager not available)"
        else
            echo "   .env file present"
        fi
    else
        echo -e "${YELLOW}No environment file found${NC}"
    fi
    
    echo ""
    echo -e "${BLUE}Quick Commands:${NC}"
    echo "  $0 start     - Start services"
    echo "  $0 status    - Check status"
    echo "  $0 test      - Test API"
    echo "  $0 health    - Run health checks"
}

# Main script logic
main() {
    # Show banner
    show_banner
    
    # Check if we're in the right directory
    if [ ! -f "scripts/setup-local.sh" ]; then
        echo -e "${RED}❌ Please run this script from the saas/ directory${NC}"
        exit 1
    fi
    
    case "$1" in
        setup)
            setup_project
            ;;
        start|stop|restart|status|logs)
            manage_services "$1"
            ;;
        test)
            test_api
            ;;
        env)
            manage_environment "$2"
            ;;
        secure)
            manage_security "$2"
            ;;
        backup)
            create_backup
            ;;
        restore)
            restore_backup "$2"
            ;;
        clean)
            clean_project
            ;;
        uninstall)
            uninstall_project
            ;;
        health)
            run_health_checks
            ;;
        info)
            show_project_info
            ;;
        help|--help|-h)
            show_usage
            ;;
        "")
            show_project_info
            ;;
        *)
            echo -e "${RED}❌ Unknown command: $1${NC}"
            echo ""
            show_usage
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"
