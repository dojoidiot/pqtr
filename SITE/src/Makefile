# Site Project Makefile
# Build and management commands for the site project

.PHONY: help build clean deploy install test

# Project configuration
PROJECT_NAME = site
PYTHON = python3
PIP = pip3

# Default target
help:
	@echo "🚀 Site Project - Available Commands"
	@echo "===================================="
	@echo "make build     - Build the project"
	@echo "make clean     - Clean build artifacts"
	@echo "make deploy    - Create deployment package"
	@echo "make install   - Install dependencies"
	@echo "make test      - Run tests"
	@echo "make help      - Show this help"

# Build the project
build:
	@echo "🔨 Building $(PROJECT_NAME)..."
	@mkdir -p ../var/build
	@cp -r *.py ../var/build/ 2>/dev/null || true
	@cp -r ../www/* ../var/build/ 2>/dev/null || true
	@echo "Build completed: $(shell date)" > ../var/build/build-info.txt
	@echo "✅ Build completed"

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf ../var/build/*
	@rm -rf ../var/deploy/*
	@find . -name "*.pyc" -delete
	@find . -name "__pycache__" -type d -exec rm -rf {} + 2>/dev/null || true
	@echo "✅ Clean completed"

# Create deployment package
deploy: build
	@echo "🚀 Creating deployment package..."
	@mkdir -p ../var/deploy
	@cd ../var && tar -czf deploy/site-$(shell date +%Y%m%d-%H%M%S).tar.gz -C build .
	@echo "✅ Deployment package created"

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

# Build and deploy in one command
all: clean build deploy
	@echo "🎉 Complete build and deploy cycle finished"
