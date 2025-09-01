#!/bin/bash
# PQTR Project Test Runner
# Provides easy testing commands for all projects

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project directories
PROJECTS=("saas" "pits" "host" "site")

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if Python is available
check_python() {
    if ! command -v python3 &> /dev/null; then
        print_error "Python 3 is not installed or not in PATH"
        exit 1
    fi
    
    # Check if pytest is available
    if ! python3 -c "import pytest" &> /dev/null; then
        print_warning "pytest is not installed. Installing dependencies first..."
        install_deps
    fi
}

# Function to install dependencies
install_deps() {
    print_status "Installing Python dependencies..."
    
    # Try different pip installation methods
    if command -v pip3 &> /dev/null; then
        pip3 install -r requirements.txt
        print_success "Dependencies installed successfully using pip3!"
    elif python3 -m pip --version &> /dev/null; then
        python3 -m pip install -r requirements.txt
        print_success "Dependencies installed successfully using python3 -m pip!"
    else
        print_error "No pip installation method found. Please install pip first:"
        echo "  Ubuntu/Debian: sudo apt install python3-pip"
        echo "  CentOS/RHEL: sudo yum install python3-pip"
        echo "  Or download from: https://pip.pypa.io/en/stable/installation/"
        exit 1
    fi
}

# Function to run tests for a specific project
run_project_tests() {
    local project=$1
    local test_dir="${project}/tests"
    
    if [ -d "$test_dir" ]; then
        print_status "Running tests for $project project..."
        if python3 -m pytest "$test_dir" -v --tb=short; then
            print_success "$project tests passed!"
        else
            print_error "$project tests failed!"
            return 1
        fi
    else
        print_warning "No tests directory found for $project project"
    fi
}

# Function to run all tests
run_all_tests() {
    print_status "Running all tests across all projects..."
    
    local failed_projects=()
    
    for project in "${PROJECTS[@]}"; do
        if ! run_project_tests "$project"; then
            failed_projects+=("$project")
        fi
    done
    
    echo
    if [ ${#failed_projects[@]} -eq 0 ]; then
        print_success "All project tests passed! 🎉"
    else
        print_error "Tests failed for the following projects: ${failed_projects[*]}"
        exit 1
    fi
}

# Function to show test status
show_status() {
    print_status "PQTR Project Test Status"
    echo "================================"
    echo
    
    for project in "${PROJECTS[@]}"; do
        local test_dir="${project}/tests"
        if [ -d "$test_dir" ]; then
            local test_count=$(find "$test_dir" -name "test_*.py" | wc -l)
            print_success "$project: $test_count test files found"
        else
            print_warning "$project: No tests directory"
        fi
    done
    
    echo
    print_status "Total projects with tests directories: $(find . -name "tests" -type d | wc -l)/${#PROJECTS[@]}"
}

# Function to show help
show_help() {
    echo "PQTR Project Test Runner"
    echo "========================"
    echo
    echo "Usage: $0 [COMMAND]"
    echo
    echo "Commands:"
    echo "  all           Run tests for all projects"
    echo "  saas          Run tests for SaaS project only"
    echo "  pits          Run tests for PITS project only"
    echo "  host          Run tests for Host project only"
    echo "  site          Run tests for Site project only"
    echo "  install       Install Python dependencies"
    echo "  status        Show test status for all projects"
    echo "  help          Show this help message"
    echo
    echo "Examples:"
    echo "  $0 all        # Run all tests"
    echo "  $0 saas       # Run SaaS tests only"
    echo "  $0 install    # Install dependencies"
    echo "  $0 status     # Show test status"
}

# Main script logic
main() {
    case "${1:-help}" in
        "all")
            check_python
            run_all_tests
            ;;
        "saas")
            check_python
            run_project_tests "saas"
            ;;
        "pits")
            check_python
            run_project_tests "pits"
            ;;
        "host")
            check_python
            run_project_tests "host"
            ;;
        "site")
            check_python
            run_project_tests "site"
            ;;
        "install")
            check_python
            install_deps
            ;;
        "status")
            show_status
            ;;
        "help"|*)
            show_help
            ;;
    esac
}

# Run main function with all arguments
main "$@"
