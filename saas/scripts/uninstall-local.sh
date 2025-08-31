#!/bin/bash

# SaaS Project Local Uninstall Script

set -e

echo "🗑️  Uninstalling SaaS Project (Local Installation)..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to confirm action
confirm() {
    read -p "$1 (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        return 0
    else
        return 1
    fi
}

echo -e "${YELLOW}⚠️  This will remove the SaaS project database and stop services.${NC}"
echo "This action cannot be undone!"

if ! confirm "Are you sure you want to continue?"; then
    echo "Uninstall cancelled."
    exit 0
fi

echo -e "${BLUE}🛑 Stopping services...${NC}"

# Stop PostgREST if running
if [ -f postgrest.pid ]; then
    echo "Stopping PostgREST..."
    kill $(cat postgrest.pid) 2>/dev/null || true
    rm -f postgrest.pid
    echo -e "${GREEN}✅ PostgREST stopped${NC}"
fi

# Remove log files
if [ -f postgrest.log ]; then
    rm -f postgrest.log
    echo -e "${GREEN}✅ Log files removed${NC}"
fi

echo -e "${BLUE}🗄️  Removing database...${NC}"

# Drop database
echo "Dropping saas_db database..."
sudo -u postgres psql -c "DROP DATABASE IF EXISTS saas_db;" 2>/dev/null || true

# Drop user
echo "Dropping rest_user..."
sudo -u postgres psql -c "DROP USER IF EXISTS rest_user;" 2>/dev/null || true

echo -e "${GREEN}✅ Database removed${NC}"

echo -e "${BLUE}🧹 Cleaning up files...${NC}"

# Remove environment file
if [ -f .env ]; then
    rm -f .env
    echo -e "${GREEN}✅ Environment file removed${NC}"
fi

echo ""
echo -e "${GREEN}🎉 Uninstall completed successfully!${NC}"
echo ""
echo -e "${BLUE}📋 What was removed:${NC}"
echo "   - saas_db database"
echo "   - rest_user database user"
echo "   - PostgREST service"
echo "   - Configuration files"
echo "   - Log files"
echo ""
echo -e "${BLUE}ℹ️  Note:${NC}"
echo "   - PostgreSQL service continues running as a system service"
echo "   - To remove PostgreSQL completely: sudo apt remove postgresql postgresql-contrib"
echo "   - To remove PostgREST: sudo rm /usr/local/bin/postgrest"
echo ""
echo -e "${BLUE}🔄 To reinstall:${NC}"
echo "   ./scripts/setup-local.sh"
