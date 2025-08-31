# PQTR - Multi-Project Infrastructure Repository

A comprehensive repository containing multiple production-ready projects for modern infrastructure and application development.

## 🏗️ Project Overview

This repository contains three main project areas, each designed for specific infrastructure and development needs:

### 🚀 [SaaS Project](saas/) - Backend API Template
- **PostgreSQL + PostgREST** SaaS backend
- **Multi-tenant architecture** with Row Level Security
- **Production-ready** with health checks and monitoring
- **Multi-environment support** (dev/test/production)
- **Secure configuration management** with GPG encryption

### 🏠 [Host Project](host/) - Server Hardening & Management
- **Linux server hardening** with security best practices
- **SSH infrastructure management** with zone-based authentication
- **Web service deployment** with SSL/TLS and reverse proxy
- **Standardized server configuration** across environments
- **Enterprise-grade security** practices

### 🗂️ [PITS Project](pits/) - Picture Image Transfer System
- **IoT device for photographers** in the field
- **FTP server integration** for camera image uploads
- **Hotspot management** for phone connectivity
- **Image processing pipeline** with storage management
- **Classic Unix structure** for reliable operation

### 🌐 [Site Project](site/) - Web Development Framework
- **Modern web development** with classic Unix structure
- **Build automation** and deployment packaging
- **Local development server** for testing and preview
- **Production-ready deployment** with versioning
- **Web-focused workflow** and tools

## 🚀 Quick Start

### SaaS Backend
```bash
cd saas
./quick-start.sh setup
./quick-start.sh start
```

### Server Management
```bash
cd host
./bin/zone-make.sh production-zone
./bin/node-make.sh <host-ip> <host-name> <zone-pkey>
```

### IoT Photo Transfer Device
```bash
cd pits
./bin/init
./bin/pits status
```

### Web Development
```bash
cd site
./bin/init
./bin/site serve
```

## 🎯 Use Cases

- **SaaS Applications** - Multi-tenant backend services
- **Infrastructure Management** - Secure server deployment and hardening
- **IoT Devices** - Photo transfer and field management
- **Web Development** - Modern frameworks with classic Unix organization
- **Production Environments** - Enterprise-grade security and monitoring

## 🔧 Prerequisites

- **Linux/macOS** - Operating system support
- **PostgreSQL 15+** - For SaaS project
- **PostgREST** - For SaaS project
- **GPG** - For encryption (optional)
- **SSH** - For host project
- **Python 3** - For roam project

## 📚 Documentation

Each project contains comprehensive documentation:
- **[SaaS Project](saas/README.md)** - Complete backend documentation
- **[Host Project](host/README.md)** - Server management guide
- **[PITS Project](pits/README.md)** - Picture Image Transfer System guide
- **[Site Project](site/README.md)** - Web development framework guide

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License.

## 🆘 Support

- **SaaS Issues**: Check [SaaS README](saas/README.md)
- **Host Issues**: Check [Host README](host/README.md)
- **PITS Issues**: Check [PITS README](pits/README.md)
- **Site Issues**: Check [Site README](site/README.md)
- **General Issues**: Open an issue in the repository

---

**Production Ready** - All projects include security best practices, monitoring, and deployment guides for enterprise environments.
