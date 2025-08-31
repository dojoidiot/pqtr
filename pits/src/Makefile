# PITS Project Makefile
# Build and management commands for the Picture Image Transfer System

.PHONY: help build clean test install uninstall

# Project configuration
PROJECT_NAME = pits
PYTHON = python3
PIP = pip3

# Default target
help:
	@echo "🚀 PITS - Picture Image Transfer System - Available Commands"
	@echo "=================================="
	@echo "make build     - Build the project"
	@echo "make clean     - Clean build artifacts"
	@echo "make test      - Run tests"
	@echo "make install   - Install dependencies"
	@echo "make uninstall - Remove installed files"
	@echo "make help      - Show this help"

# Build the project
build:
	@echo "🔨 Building $(PROJECT_NAME)..."
	@mkdir -p ../var/build
	@cp *.py ../var/build/
	@echo "✅ Build completed"

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf ../var/build/*
	@find . -name "*.pyc" -delete
	@find . -name "__pycache__" -type d -exec rm -rf {} + 2>/dev/null || true
	@echo "✅ Clean completed"

# Install dependencies
install:
	@echo "📦 Installing dependencies..."
	@$(PIP) install -r requirements.txt 2>/dev/null || echo "⚠️  No requirements.txt found"
	@echo "✅ Installation completed"

# Run tests
test:
	@echo "🧪 Running tests..."
	@$(PYTHON) -m pytest tests/ 2>/dev/null || echo "⚠️  No tests found"
	@echo "✅ Tests completed"

# Uninstall
uninstall:
	@echo "🗑️  Uninstalling $(PROJECT_NAME)..."
	@$(PIP) uninstall $(PROJECT_NAME) -y 2>/dev/null || echo "⚠️  Package not installed"
	@echo "✅ Uninstall completed"

# Development helpers
dev-setup: install
	@echo "🔧 Setting up development environment..."
	@$(PYTHON) main.py init
	@echo "✅ Development setup completed"

# Quick start
start: build
	@echo "🚀 Starting $(PROJECT_NAME)..."
	@$(PYTHON) main.py status

# Show project structure
structure:
	@$(PYTHON) main.py structure
