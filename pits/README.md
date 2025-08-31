# PITS Project - Picture Image Transfer System

An IoT device used by photographers in the field for picture image transfer from their camera using FTP to the PITS server where processing and storage is done, and then waits for their phone to provide a hotspot and then use the PQTR user app to manage the production process.

## 📁 Project Structure

```
roam/
├── bin/                    # 🛠️ Executable scripts and binaries
│   ├── roam              # Main project management script
│   └── init              # Project initialization script
├── etc/                   # ⚙️ Configuration files
│   └── config.conf       # Project configuration
├── src/                   # 🔨 Source code
│   ├── main.py           # Python main application
│   └── Makefile          # Build and management
├── var/                   # 📊 Variable data (logs, cache, etc.)
├── www/                   # 🌐 Web content
│   └── index.html        # Sample web page
└── README.md              # 📚 Project documentation
```

## 🎯 Purpose

This structure follows classic Linux filesystem organization principles and provides a standardized IoT workflow for the Picture Image Transfer System:

- **`bin/`** - Executable programs and scripts
- **`etc/`** - Configuration files and system settings
- **`src/`** - Source code and development files
- **`var/`** - Variable data that changes during operation
- **`www/`** - Web content, static files, and web applications

## 🚀 Quick Start

```bash
# Navigate to pits directory
cd pits

# Initialize the project structure
./bin/init

# Check project status
./bin/pits status

# Start the project
./bin/pits start

# Start FTP server
./bin/pits ftp

# Start hotspot
./bin/pits hotspot

# Python commands
python3 src/main.py status
cd src && make help
```

## 🎯 Key Features

- **IoT photo transfer device** for field photographers
- **FTP server integration** for camera photo uploads
- **Hotspot management** for phone connectivity
- **Photo processing pipeline** with storage management
- **Classic Linux structure** following FHS principles
- **Built-in project management** scripts for common operations
- **Python application framework** with Makefile support
- **Configuration management** with centralized settings
- **Logging and monitoring** infrastructure

## 🚀 Usage

```bash
# Navigate to pits directory
cd pits

# Run scripts from bin
./bin/script-name

# Edit configurations in etc
nano etc/config.conf

# Work with source code in src
cd src && make

# Check logs in var
tail -f var/logs/app.log

# Start FTP server for photo uploads
./bin/pits ftp

# Start hotspot for phone connectivity
./bin/pits hotspot
```

## 🔧 Customization

Each directory can be customized for your specific IoT photo transfer needs:

- **`bin/`** - Add your IoT device scripts and utilities
- **`etc/`** - Store configuration files and environment settings
- **`src/`** - Organize your source code by language or component
- **`var/`** - Store logs, temporary files, and photo data
- **`www/`** - Host management interface and status pages

## 🔧 Prerequisites

- **Linux/macOS** - Operating system support
- **Python 3** - For source code execution
- **Bash shell** - For script execution
- **Git** - For version control (optional)

## 📋 Best Practices

- Keep `bin/` scripts executable and well-documented
- Use `etc/` for all configuration, not hardcoded values
- Organize `src/` logically by component or feature
- Use `var/` for data that changes during operation
- Keep `www/` clean and organized for web deployment

## 📚 Related Projects

This PITS project works alongside:
- **SaaS Project** - Backend API and database services
- **Host Project** - Server hardening and infrastructure management
- **Site Project** - Website and management tools
- **User Project** - Mobile and web apps for production management
- **PQTR Repository** - Overall infrastructure management
