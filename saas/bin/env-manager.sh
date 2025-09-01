#!/bin/bash

# SaaS Project Environment Manager Script

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to show usage
show_usage() {
    echo "Usage: $0 {dev|tst|pro|show|validate|create}"
    echo ""
    echo "Commands:"
    echo "  dev      - Switch to development environment (.env-dev)"
    echo "  tst      - Switch to test environment (.env-tst)"
    echo "  pro      - Switch to production environment (.env-pro)"
    echo "  show     - Show current environment configuration"
    echo "  validate - Validate environment file syntax"
    echo "  create   - Create missing environment files from templates"
    echo ""
    echo "Examples:"
    echo "  $0 dev    # Switch to development"
    echo "  $0 show   # Show current config"
    echo "  $0 create # Create missing env files"
}

# Function to check if environment file exists
env_exists() {
    local env_file=".env-$1"
    [ -f "$env_file" ]
}

# Function to switch environment
switch_env() {
    local env_type=$1
    local env_file=".env-$env_type"
    
    if ! env_exists "$env_type"; then
        echo -e "${RED}❌ Environment file $env_file not found${NC}"
        echo "Run '$0 create' to create missing environment files"
        exit 1
    fi
    
    echo -e "${BLUE}🔄 Switching to $env_type environment...${NC}"
    
    # Backup existing .env if it exists
    if [ -f .env ]; then
        cp .env .env.backup
        echo "Backed up existing .env to .env.backup"
    fi
    
    # Copy the target environment file to .env
    cp "$env_file" .env
    
    echo -e "${GREEN}✅ Switched to $env_type environment${NC}"
    echo "Current environment: $env_type"
    
    # Show key differences
    show_env_summary "$env_type"
}

# Function to show environment summary
show_env_summary() {
    local env_type=$1
    echo ""
    echo -e "${BLUE}📋 $env_type Environment Summary:${NC}"
    echo "=================================="
    
    # Extract key values
    local db_uri=$(grep "^PGRST_DB_URI=" .env | cut -d'=' -f2-)
    local jwt_aud=$(grep "^PGRST_JWT_AUD=" .env | cut -d'=' -f2-)
    local node_env=$(grep "^NODE_ENV=" .env | cut -d'=' -f2-)
    local log_level=$(grep "^LOG_LEVEL=" .env | cut -d'=' -f2-)
    
    echo "Database URI: ${db_uri:0:50}..."
    echo "JWT Audience: $jwt_aud"
    echo "Node Environment: $node_env"
    echo "Log Level: $log_level"
    
    if [ "$env_type" = "pro" ]; then
        echo -e "${YELLOW}⚠️  PRODUCTION: Ensure all passwords are changed!${NC}"
    fi
}

# Function to show current environment
show_current_env() {
    if [ ! -f .env ]; then
        echo -e "${YELLOW}⚠️  No .env file found${NC}"
        return
    fi
    
    echo -e "${BLUE}📋 Current Environment Configuration:${NC}"
    echo "=========================================="
    
    # Determine which environment file this matches
    local current_env=""
    if [ -f .env-dev ] && diff .env .env-dev >/dev/null 2>&1; then
        current_env="dev"
    elif [ -f .env-tst ] && diff .env .env-tst >/dev/null 2>&1; then
        current_env="tst"
    elif [ -f .env-pro ] && diff .env .env-pro >/dev/null 2>&1; then
        current_env="pro"
    else
        current_env="custom"
    fi
    
    echo "Environment Type: $current_env"
    echo ""
    
    # Show key configuration values
    grep -E "^(POSTGRES_|PGRST_|NODE_ENV|LOG_LEVEL)" .env | while read line; do
        if [[ $line == *"PASSWORD"* ]] || [[ $line == *"SECRET"* ]]; then
            echo "${line%=*}=***HIDDEN***"
        else
            echo "$line"
        fi
    done
}

# Function to validate environment file
validate_env() {
    local env_file=${1:-.env}
    
    if [ ! -f "$env_file" ]; then
        echo -e "${RED}❌ Environment file $env_file not found${NC}"
        exit 1
    fi
    
    echo -e "${BLUE}🔍 Validating $env_file...${NC}"
    
    local errors=0
    
    # Check required variables
    local required_vars=("POSTGRES_DB" "POSTGRES_USER" "POSTGRES_PASSWORD" "PGRST_DB_URI" "PGRST_JWT_SECRET")
    
    for var in "${required_vars[@]}"; do
        if ! grep -q "^${var}=" "$env_file"; then
            echo -e "${RED}❌ Missing required variable: $var${NC}"
            ((errors++))
        fi
    done
    
    # Check for empty values
    if grep -q "^[A-Z_]*=$" "$env_file"; then
        echo -e "${YELLOW}⚠️  Found empty values in $env_file${NC}"
        grep -n "^[A-Z_]*=$" "$env_file"
    fi
    
    # Check for placeholder passwords in production
    if [[ "$env_file" == *".env-pro"* ]]; then
        if grep -q "CHANGE_THIS" "$env_file"; then
            echo -e "${RED}❌ Production file contains placeholder values!${NC}"
            ((errors++))
        fi
    fi
    
    if [ $errors -eq 0 ]; then
        echo -e "${GREEN}✅ Environment file $env_file is valid${NC}"
    else
        echo -e "${RED}❌ Found $errors validation errors${NC}"
        exit 1
    fi
}

# Function to create missing environment files
create_env_files() {
    echo -e "${BLUE}🔧 Creating missing environment files...${NC}"
    
    local created=0
    
    for env_type in dev tst pro; do
        local env_file=".env-$env_type"
        if [ ! -f "$env_file" ]; then
            echo "Creating $env_file..."
            cp env.example "$env_file"
            created=$((created + 1))
        else
            echo "$env_file already exists"
        fi
    done
    
    if [ $created -gt 0 ]; then
        echo -e "${GREEN}✅ Created $created environment files${NC}"
        echo ""
        echo "Next steps:"
        echo "1. Review and customize each .env-* file"
        echo "2. Use '$0 dev|tst|pro' to switch environments"
        echo "3. Ensure production passwords are changed!"
    else
        echo -e "${GREEN}✅ All environment files already exist${NC}"
    fi
}

# Main script logic
case "$1" in
    dev)
        switch_env "dev"
        ;;
    tst)
        switch_env "tst"
        ;;
    pro)
        switch_env "pro"
        ;;
    show)
        show_current_env
        ;;
    validate)
        if [ -n "$2" ]; then
            validate_env ".env-$2"
        else
            validate_env ".env"
        fi
        ;;
    create)
        create_env_files
        ;;
    *)
        show_usage
        exit 1
        ;;
esac

exit 0
