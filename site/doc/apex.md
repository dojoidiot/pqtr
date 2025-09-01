# Site Project - Website and Management Interface

The site project provides the website and management interface for the PQTR system. It serves as the public-facing web presence and administrative interface for the overall PQTR ecosystem.

## Overview

The site project is a simple, efficient web deployment system that provides:
- **Public Website**: The main PQTR landing page and branding
- **Management Interface**: Administrative tools and system status
- **Deployment System**: Automated build and deployment to production servers
- **Configuration Management**: Nginx configuration and SSL setup

## Project Structure

```
site/
├── bin/          # Build and deployment scripts
├── etc/          # Nginx configuration and SSL certificates
├── src/          # Python build tools and Makefile
├── var/          # Build artifacts and deployment packages
├── www/          # Static web content
└── doc/          # Project documentation
```

### Directory Details

- **bin/** - Contains `make.sh` for creating deployment packages and `send.sh` for deploying to production servers
- **etc/** - Nginx configuration (`pqtr.ai.conf`) with SSL setup and domain routing
- **src/** - Python build tools (`main.py`) and Makefile for development workflow
- **var/** - Generated build artifacts and deployment packages
- **www/** - Static web content including the main HTML page with PQTR branding
- **doc/** - Project documentation and guides

## Key Features

### Website
- **PQTR Branding**: Clean, modern landing page with hero text
- **Responsive Design**: Works on all screen sizes from mobile to desktop
- **Background Image**: Full-screen background with centered content
- **Modern Typography**: Roboto font for clean, professional appearance

### Deployment System
- **Automated Build**: Creates deployment packages with timestamps
- **Production Deployment**: Automated deployment to production servers
- **SSL Configuration**: Automatic HTTPS setup with Let's Encrypt
- **Nginx Integration**: Optimized web server configuration

### Configuration Management
- **Domain Setup**: pqtr.ai domain configuration
- **SSL Certificates**: Automatic HTTPS with Let's Encrypt
- **Gzip Compression**: Optimized content delivery
- **Security Headers**: Hardened server configuration

## Quick Start

### Build Deployment Package
```bash
./bin/make.sh
```

### Deploy to Production
```bash
./bin/send.sh
```

### Deploy to Specific Host
```bash
./bin/send.sh your-host.com
```

## Python Commands

### Show Project Status
```bash
python3 src/main.py status
```

### Create Build
```bash
python3 src/main.py build
```

### Create Deployment Package
```bash
python3 src/main.py deploy
```

## Make Commands

### Build Project
```bash
cd src && make build
```

### Clean Build Artifacts
```bash
cd src && make clean
```

### Create Deployment Package
```bash
cd src && make deploy
```

### Show Help
```bash
cd src && make help
```

## Web Content

The `www/` directory contains the main web content:

- **main.html** - Primary landing page featuring:
  - Full-screen background image (`main.png`)
  - Centered hero text with "PQTR" title
  - "/picture/" subtitle with matching width
  - Responsive design for all screen sizes
  - Modern typography using Roboto font

## Configuration

### pqtr.ai.conf
Located in `etc/pqtr.ai.conf`, this Nginx configuration file includes:
- Domain setup for pqtr.ai and www.pqtr.ai
- SSL certificate configuration with Let's Encrypt
- Gzip compression for optimized delivery
- Security headers and server hardening
- Automatic HTTPS redirect

## Deployment Process

1. **Build**: Create deployment package with `make.sh`
2. **Package**: Tar.gz archive with configuration and web files
3. **Transfer**: Upload to production server via SSH
4. **Deploy**: Extract files and restart Nginx service
5. **Verify**: Check SSL and domain accessibility

## Server Requirements

- **Nginx**: Web server with SSL support
- **Let's Encrypt**: SSL certificate management
- **SSH Access**: For automated deployment
- **Sudo Privileges**: For service management

## Development Workflow

1. **Local Development**: Edit HTML/CSS files in `www/`
2. **Testing**: Preview changes locally
3. **Build**: Create deployment package
4. **Deploy**: Push to production server
5. **Verify**: Check live site functionality

## Architecture

The project follows a simple, efficient architecture:

- **Static Content**: HTML/CSS/JS files served directly
- **Build System**: Python scripts for automation
- **Deployment**: Shell scripts for server management
- **Configuration**: Nginx for web server setup
- **SSL**: Let's Encrypt for security

## Best Practices

- Keep web content simple and fast-loading
- Use automated deployment for consistency
- Maintain SSL certificates and security
- Test deployments on staging first
- Monitor server performance and uptime

## Integration

The site project integrates with the broader PQTR ecosystem:
- **Branding**: Consistent PQTR visual identity
- **Domain**: pqtr.ai as the main web presence
- **Infrastructure**: Host project for server management
- **Services**: SaaS backend for dynamic functionality
