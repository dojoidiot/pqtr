#!/bin/bash

# SaaS Project Quick Start Script
# Simple commands for common operations

set -e

echo "🚀 SaaS Project Quick Start"
echo "=========================="
echo ""

# Check if we're in the right directory
if [ ! -f "scripts/project.sh" ]; then
    echo "❌ Please run this script from the saas/ directory"
    exit 1
fi

# Show available commands
echo "Available commands:"
echo "  ./quick-start.sh setup    - First time setup"
echo "  ./quick-start.sh start    - Start services"
echo "  ./quick-start.sh test     - Test API"
echo "  ./quick-start.sh status   - Check status"
echo "  ./quick-start.sh stop     - Stop services"
echo ""

# Get command from user
if [ -z "$1" ]; then
    echo "What would you like to do?"
    read -p "Command (setup/start/test/status/stop): " command
else
    command="$1"
fi

# Execute command
case "$command" in
    setup)
        echo "🔧 Setting up project..."
        ./scripts/project.sh setup
        ;;
    start)
        echo "🚀 Starting services..."
        ./scripts/project.sh start
        ;;
    test)
        echo "🧪 Testing API..."
        ./scripts/project.sh test
        ;;
    status)
        echo "📊 Checking status..."
        ./scripts/project.sh status
        ;;
    stop)
        echo "🛑 Stopping services..."
        ./scripts/project.sh stop
        ;;
    *)
        echo "❌ Unknown command: $command"
        echo "Valid commands: setup, start, test, status, stop"
        exit 1
        ;;
esac

echo ""
echo "✅ Done! Run './quick-start.sh help' for more options"
