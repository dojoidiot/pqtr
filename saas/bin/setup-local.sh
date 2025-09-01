#!/bin/bash

# SaaS Project Local Setup Script (No Docker)

set -e

echo "🚀 Setting up SaaS Project locally..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to check command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if PostgreSQL is running
check_postgres_running() {
    if systemctl is-active --quiet postgresql; then
        return 0
    else
        return 1
    fi
}

echo -e "${BLUE}📋 Checking prerequisites...${NC}"

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}❌ Please don't run this script as root${NC}"
    exit 1
fi

# Check if PostgreSQL is installed
if ! command_exists psql; then
    echo -e "${YELLOW}⚠️  PostgreSQL is not installed. Installing...${NC}"
    
    # Detect OS and install PostgreSQL
    if command_exists apt; then
        echo "Installing PostgreSQL via apt..."
        sudo apt update
        sudo apt install -y postgresql postgresql-contrib
    elif command_exists yum; then
        echo "Installing PostgreSQL via yum..."
        sudo yum install -y postgresql postgresql-server postgresql-contrib
        sudo postgresql-setup initdb
    elif command_exists dnf; then
        echo "Installing PostgreSQL via dnf..."
        sudo dnf install -y postgresql postgresql-server postgresql-contrib
        sudo postgresql-setup initdb
    else
        echo -e "${RED}❌ Unsupported package manager. Please install PostgreSQL manually.${NC}"
        exit 1
    fi
else
    echo -e "${GREEN}✅ PostgreSQL is already installed${NC}"
fi

# Check if PostgREST is installed
if ! command_exists postgrest; then
    echo -e "${YELLOW}⚠️  PostgREST is not installed. Installing...${NC}"
    
    # Download and install PostgREST
    echo "Downloading PostgREST..."
    cd /tmp
    wget -q https://github.com/PostgREST/postgrest/releases/latest/download/postgrest-linux-x64-static.tar.xz
    
    if [ -f postgrest-linux-x64-static.tar.xz ]; then
        echo "Extracting PostgREST..."
        tar -xf postgrest-linux-x64-static.tar.xz
        sudo mv postgrest /usr/local/bin/
        sudo chmod +x /usr/local/bin/postgrest
        echo -e "${GREEN}✅ PostgREST installed successfully${NC}"
    else
        echo -e "${RED}❌ Failed to download PostgREST${NC}"
        exit 1
    fi
    
    # Return to project directory
    cd "$(dirname "$0")/.."
else
    echo -e "${GREEN}✅ PostgREST is already installed${NC}"
fi

echo -e "${BLUE}🔧 Starting PostgreSQL service...${NC}"

# Start PostgreSQL service
if command_exists systemctl; then
    sudo systemctl start postgresql
    sudo systemctl enable postgresql
    
    if check_postgres_running; then
        echo -e "${GREEN}✅ PostgreSQL service started${NC}"
    else
        echo -e "${RED}❌ Failed to start PostgreSQL service${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠️  systemctl not available. Please start PostgreSQL manually.${NC}"
fi

echo -e "${BLUE}🗄️  Setting up database...${NC}"

# Create database and user
echo "Creating database and user..."
sudo -u postgres psql -c "CREATE DATABASE saas_db;" 2>/dev/null || echo "Database saas_db already exists"
sudo -u postgres psql -c "CREATE USER rest_user WITH PASSWORD 'rest_pass';" 2>/dev/null || echo "User rest_user already exists"
sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE saas_db TO rest_user;"

# Initialize schema
echo "Initializing database schema..."
PGPASSWORD=rest_pass psql -h localhost -U rest_user -d saas_db -f config/init.sql

echo -e "${GREEN}✅ Database setup completed${NC}"

echo -e "${BLUE}⚙️  Setting up environment...${NC}"

# Create environment files if they don't exist
if [ -f scripts/env-manager.sh ]; then
    echo "Setting up environment files..."
    ./scripts/env-manager.sh create
    
    # Switch to development environment by default
    echo "Switching to development environment..."
    ./scripts/env-manager.sh dev
else
    echo -e "${YELLOW}⚠️  Environment manager not found, creating basic .env file${NC}"
    # Create basic .env file from template
    if [ -f env.example ]; then
        cp env.example .env
        echo -e "${GREEN}✅ Environment file created${NC}"
    else
        echo -e "${YELLOW}⚠️  env.example not found, creating basic .env file${NC}"
        cat > .env << EOF
# SaaS Project Environment Variables
POSTGRES_DB=saas_db
POSTGRES_USER=rest_user
POSTGRES_PASSWORD=rest_pass
PGRST_DB_URI=postgres://rest_user:rest_pass@localhost:5432/saas_db
PGRST_DB_SCHEMA=public
PGRST_DB_ANON_ROLE=anon
PGRST_JWT_SECRET=your-super-secret-jwt-token-with-at-least-32-characters-long
PGRST_JWT_AUD=saas
PGRST_JWT_EXP=1h
EOF
    fi
fi

echo -e "${BLUE}🚀 Starting PostgREST...${NC}"

# Start PostgREST in background
echo "Starting PostgREST service..."
nohup postgrest config/postgrest.conf > postgrest.log 2>&1 &
POSTGREST_PID=$!

# Wait a moment for PostgREST to start
sleep 3

# Check if PostgREST is running
if kill -0 $POSTGREST_PID 2>/dev/null; then
    echo -e "${GREEN}✅ PostgREST started successfully (PID: $POSTGREST_PID)${NC}"
    echo $POSTGREST_PID > postgrest.pid
else
    echo -e "${RED}❌ Failed to start PostgREST${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}🎉 Local setup completed successfully!${NC}"
echo ""
echo "🌐 Service Endpoints:"
echo "   PostgreSQL: localhost:5432"
echo "   PostgREST API: http://localhost:3000"
echo ""
echo "📚 Next steps:"
echo "   1. Test the API: ./scripts/test-api-local.sh"
echo "   2. Check PostgREST logs: tail -f postgrest.log"
echo "   3. Stop PostgREST: kill \$(cat postgrest.pid)"
echo ""
echo "🔧 Environment Management:"
echo "   - Switch environments: ./scripts/env-manager.sh [dev|tst|pro]"
echo "   - Show current config: ./scripts/env-manager.sh show"
echo "   - Validate config: ./scripts/env-manager.sh validate"
echo ""
echo "🔍 Troubleshooting:"
echo "   - Check PostgreSQL status: sudo systemctl status postgresql"
echo "   - Check PostgREST logs: cat postgrest.log"
echo "   - Restart PostgreSQL: sudo systemctl restart postgresql"
