#!/bin/bash

# SaaS Project Local API Testing Script (No Docker)

set -e

echo "🧪 Testing SaaS Project API (Local Installation)..."

# Base URL for the API
API_BASE="http://localhost:3000"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to test endpoint
test_endpoint() {
    local endpoint=$1
    local method=${2:-GET}
    local data=${3:-""}
    
    echo -n "Testing $method $endpoint... "
    
    if [ "$method" = "GET" ]; then
        response=$(curl -s -o /tmp/response -w "%{http_code}" "$API_BASE$endpoint")
    elif [ "$method" = "POST" ]; then
        response=$(curl -s -o /tmp/response -w "%{http_code}" -X POST \
            -H "Content-Type: application/json" \
            -d "$data" \
            "$API_BASE$endpoint")
    fi
    
    if [ "$response" = "200" ] || [ "$response" = "201" ]; then
        echo -e "${GREEN}✓ OK ($response)${NC}"
    else
        echo -e "${RED}✗ Failed ($response)${NC}"
        echo "Response body:"
        cat /tmp/response
        echo ""
    fi
}

# Function to check if service is running
check_service() {
    local service_name=$1
    local check_command=$2
    
    echo -n "Checking $service_name... "
    if eval "$check_command" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Running${NC}"
        return 0
    else
        echo -e "${RED}✗ Not running${NC}"
        return 1
    fi
}

echo -e "${BLUE}🔍 Checking services...${NC}"

# Check PostgreSQL
if ! check_service "PostgreSQL" "systemctl is-active --quiet postgresql"; then
    echo -e "${RED}❌ PostgreSQL is not running. Please start it first:${NC}"
    echo "   sudo systemctl start postgresql"
    exit 1
fi

# Check PostgREST
if ! check_service "PostgREST" "curl -s $API_BASE > /dev/null"; then
    echo -e "${RED}❌ PostgREST is not running. Please start it first:${NC}"
    echo "   postgrest config/postgrest.conf"
    echo ""
    echo "Or run the setup script:"
    echo "   ./scripts/setup-local.sh"
    exit 1
fi

echo ""
echo -e "${GREEN}✅ All services are running!${NC}"
echo ""

# Test basic endpoints
echo -e "${BLUE}📋 Testing API endpoints...${NC}"

# Test organizations endpoint (should return sample data)
test_endpoint "/organizations"

# Test projects endpoint
test_endpoint "/projects"

# Test users endpoint
test_endpoint "/users"

# Test with filters
echo ""
echo -e "${BLUE}🔍 Testing with filters...${NC}"
test_endpoint "/organizations?slug=eq.acme-corp"

# Test with limits
echo ""
echo -e "${BLUE}📏 Testing with limits...${NC}"
test_endpoint "/organizations?limit=1"

# Test with select
echo ""
echo -e "${BLUE}🎯 Testing with select...${NC}"
test_endpoint "/organizations?select=name,slug"

# Test database connection
echo ""
echo -e "${BLUE}🗄️  Testing database connection...${NC}"
if PGPASSWORD=rest_pass psql -h localhost -U rest_user -d saas_db -c "SELECT COUNT(*) FROM organizations;" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Database connection successful${NC}"
else
    echo -e "${RED}✗ Database connection failed${NC}"
fi

echo ""
echo -e "${GREEN}✅ API testing completed!${NC}"
echo ""
echo -e "${BLUE}📚 Next steps:${NC}"
echo "   1. Access PostgreSQL: psql -h localhost -U rest_user -d saas_db"
echo "   2. Check PostgREST logs: tail -f postgrest.log"
echo "   3. View the README.md for more API examples"
echo ""
echo -e "${BLUE}🔍 Troubleshooting:${NC}"
echo "   - PostgreSQL status: sudo systemctl status postgresql"
echo "   - PostgREST logs: cat postgrest.log"
echo "   - Restart PostgreSQL: sudo systemctl restart postgresql"
echo "   - Restart PostgREST: kill \$(cat postgrest.pid) && postgrest config/postgrest.conf &"
echo ""
echo -e "${BLUE}📊 Sample API calls:${NC}"
echo "   curl http://localhost:3000/organizations"
echo "   curl http://localhost:3000/organizations?select=name,slug"
echo "   curl http://localhost:3000/projects"
