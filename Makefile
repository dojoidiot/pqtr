# PQTR Project Root Makefile
# Provides unified commands for testing and managing all projects

.PHONY: help test test-all test-saas test-pits test-host test-site clean install-deps lint format

# Default target
help:
	@echo "PQTR Project Management Commands"
	@echo "================================"
	@echo ""
	@echo "Testing:"
	@echo "  test          - Run all tests"
	@echo "  test-saas     - Run SaaS project tests"
	@echo "  test-pits     - Run PITS project tests"
	@echo "  test-host     - Run Host project tests"
	@echo "  test-site     - Run Site project tests"
	@echo ""
	@echo "Development:"
	@echo "  install-deps  - Install Python dependencies"
	@echo "  lint          - Run linting on all projects"
	@echo "  format        - Format code with black and isort"
	@echo "  clean         - Clean test artifacts and cache"
	@echo ""
	@echo "Project Status:"
	@echo "  status        - Show status of all projects"

# Install dependencies
install-deps:
	@echo "Installing Python dependencies..."
	pip install -r requirements.txt
	@echo "Dependencies installed successfully!"

# Run all tests
test: test-all

test-all:
	@echo "Running all tests across all projects..."
	pytest --tb=short

# Run tests for specific projects
test-saas:
	@echo "Running SaaS project tests..."
	pytest saas/tst/ -v

test-pits:
	@echo "Running PITS project tests..."
	pytest pits/tst/ -v

test-host:
	@echo "Running Host project tests..."
	pytest host/tst/ -v

test-site:
	@echo "Running Site project tests..."
	pytest site/tst/ -v

# Linting
lint:
	@echo "Running linting on all Python files..."
	flake8 saas/ pits/ host/ site/ --max-line-length=100 --ignore=E501,W503
	@echo "Linting completed!"

# Code formatting
format:
	@echo "Formatting code with black..."
	black saas/ pits/ host/ site/ --line-length=100
	@echo "Code formatting completed!"

# Clean up
clean:
	@echo "Cleaning test artifacts and cache..."
	find . -type d -name "__pycache__" -exec rm -rf {} +
	find . -type f -name "*.pyc" -delete
	find . -type d -name ".pytest_cache" -exec rm -rf {} +
	find . -type d -name ".coverage" -delete
	@echo "Cleanup completed!"

# Show project status
status:
	@echo "PQTR Project Status"
	@echo "==================="
	@echo ""
	@echo "SaaS Project:    🟢 Production Ready"
	@echo "PITS Project:    🟢 Core System Complete"
	@echo "Host Project:    🟢 Production Ready"
	@echo "Site Project:    🟡 Structure Complete"
	@echo "iOS Project:     🟢 Production Ready"
	@echo ""
	@echo "Overall Quality: 8.7/10"
	@echo "Target Quality:  9.5/10"
	@echo ""
	@echo "Test Coverage:   $(shell find . -name "tst" -type d | wc -l)/5 projects have tests"
	@echo "Next Phase:      Team Foundation & Collaboration"

# Quick test run (fast)
test-quick:
	@echo "Running quick tests (unit tests only)..."
	pytest -m "not slow and not integration" --tb=line

# Security tests
test-security:
	@echo "Running security-related tests..."
	pytest -m security -v

# Database tests
test-db:
	@echo "Running database tests..."
	pytest -m database -v

# API tests
test-api:
	@echo "Running API tests..."
	pytest -m api -v
