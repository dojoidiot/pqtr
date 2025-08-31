# Site Project - Modern Web Development Framework

A modern web development framework with classic Unix structure for building and deploying production-ready websites.

## 🎯 Overview

The Site Project provides a standardized framework for web development that combines:
- **Classic Unix directory structure** for organization
- **Modern build and deployment tools** for efficiency
- **Web-focused development workflow** for productivity
- **Production-ready deployment** with packaging and versioning

## 📁 Project Structure

```
site/
├── bin/                    # 🛠️ Executable scripts and project management
│   ├── site              # Main project management script
│   └── init              # Project initialization script
├── etc/                   # ⚙️ Configuration files
│   └── site.conf         # Project configuration
├── src/                   # 🔨 Source code and build tools
│   ├── main.py           # Python main application
│   └── Makefile          # Build and deployment management
├── var/                   # 📊 Variable data and build artifacts
│   ├── logs/             # Application logs
│   ├── cache/            # Cache files
│   ├── tmp/              # Temporary files
│   ├── build/            # Build artifacts
│   └── deploy/           # Deployment packages
├── www/                   # 🌐 Web content and static files
│   └── index.html        # Sample web page
└── README.md              # 📚 Project documentation
```

## 🚀 Quick Start

```bash
# Navigate to site directory
cd site

# Initialize the project structure
./bin/init

# Check project status
./bin/site status

# Start the project
./bin/site start

# Build from source
./bin/site build

# Serve web content locally
./bin/site serve

# Create deployment package
./bin/site deploy
```

## 🎯 Key Features

- **Classic Unix structure** following FHS principles
- **Build automation** with source compilation and asset optimization
- **Deployment packaging** with timestamps and versioning
- **Web development tools** with local server and build system
- **Configuration management** with centralized settings
- **Python integration** with full Makefile support
- **Production readiness** with deployment packages

## 🔧 Core Scripts

### Project Management (`./bin/site`)

#### `start` - Initialize Project
```bash
./bin/site start
```
- Creates necessary directories (logs, cache, build, deploy)
- Sets up project environment
- Displays project information

#### `status` - Show Project Status
```bash
./bin/site status
```
- Displays project structure
- Shows configuration status
- Reports build artifact status

#### `build` - Build Project
```bash
./bin/site build
```
- Copies source files to build directory
- Copies web content to build directory
- Creates build timestamp and metadata

#### `serve` - Local Web Server
```bash
./bin/site serve
```
- Starts Python HTTP server on port 8080
- Serves content from www/ directory
- Perfect for local development and testing

#### `deploy` - Create Deployment Package
```bash
./bin/site deploy
```
- Creates timestamped tar.gz package
- Includes all build artifacts
- Ready for production deployment

### Initialization (`./bin/init`)
```bash
./bin/init
```
- Sets up complete project structure
- Creates necessary subdirectories
- Sets proper file permissions
- Generates .gitignore file

## 🐍 Python Integration

### Main Application (`src/main.py`)
```bash
# Show project status
python3 src/main.py status

# Initialize directories
python3 src/main.py init

# Create build artifacts
python3 src/main.py build

# Create deployment package
python3 src/main.py deploy

# Show project structure
python3 src/main.py structure
```

### Build and Deploy
```bash
cd src

# Build project
make build

# Create deployment package
make deploy

# Clean build artifacts
make clean

# Complete build and deploy cycle
make all
```

## ⚙️ Configuration

### Site Configuration (`etc/site.conf`)
The configuration file contains sections for:
- **Project settings** - Name, version, description
- **Path configuration** - Directory locations
- **Web settings** - Port, host, document root
- **Build options** - Build directory, compression
- **Deployment settings** - Package format, backup options
- **Logging configuration** - Log levels, retention
- **Development options** - Debug mode, auto-reload
- **Production settings** - Minification, security headers

## 🌐 Web Development

### Local Development
```bash
# Start local server
./bin/site serve

# Access at http://localhost:8080
# Edit files in www/ directory
# Changes are immediately visible
```

### Build Process
```bash
# Build project
./bin/site build

# Build artifacts created in var/build/
# Source files and web content copied
# Build metadata generated
```

### Deployment
```bash
# Create deployment package
./bin/site deploy

# Package created in var/deploy/
# Timestamped tar.gz format
# Ready for production server
```

## 🔧 Prerequisites

- **Linux/macOS** - Operating system support
- **Python 3** - For source code execution and web server
- **Bash shell** - For script execution
- **Make** - For build system (optional)
- **Git** - For version control (optional)

## 📋 Best Practices

- **Keep bin/ scripts executable** and well-documented
- **Use etc/ for configuration** - avoid hardcoded values
- **Organize src/ logically** by component or feature
- **Use var/ for build artifacts** and temporary data
- **Keep www/ clean** and organized for web deployment
- **Version your deployments** with timestamps
- **Test locally** before deploying

## 🚀 Deployment Workflow

### 1. Development
```bash
./bin/site start
./bin/site serve
# Edit files in www/ and src/
# Test changes locally
```

### 2. Build
```bash
./bin/site build
# Review build artifacts in var/build/
# Verify all content is included
```

### 3. Deploy
```bash
./bin/site deploy
# Upload package from var/deploy/ to production
# Extract and configure on target server
```

## 🔍 Troubleshooting

### Common Issues
- **Permission denied**: Run `./bin/init` to set proper permissions
- **Port already in use**: Change port in `etc/site.conf` or stop other services
- **Build failures**: Check that `src/` and `www/` directories exist
- **Deployment errors**: Ensure build artifacts exist before deploying

### Debug Commands
```bash
# Check project status
./bin/site status

# View configuration
./bin/site config

# Check Python application
python3 src/main.py status

# Verify build artifacts
ls -la var/build/
```

## 📚 Related Projects

This site project works alongside:
- **SaaS Project** - Backend API and database services
- **Host Project** - Server hardening and infrastructure management
- **PITS Project** - Picture Image Transfer System
- **PQTR Repository** - Overall infrastructure management

## 🤝 Contributing

1. Follow the established Unix structure
2. Test scripts in isolated environments
3. Document any configuration changes
4. Maintain backward compatibility
5. Use consistent naming conventions

## 📄 License

Part of the PQTR project - see repository license.

---

**Production Ready** - This framework provides enterprise-grade web development tools with classic Unix organization.
