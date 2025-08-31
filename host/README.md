# Host Project - Server Hardening & Management

A streamlined Linux server hardening and management system for secure, production-ready infrastructure deployment.

## 🎯 Overview

The Host Project provides automated tools for:
- **Server hardening** with security best practices
- **SSH infrastructure management** with zone-based authentication
- **Web service deployment** with SSL/TLS and reverse proxy
- **Standardized server configuration** across environments

## 📁 Project Structure

```
host/
├── bin/                    # 🛠️ Server management scripts
│   ├── zone-make.sh      # Create zone SSH keys
│   ├── user-make.sh      # Create user SSH keys  
│   ├── user-sign.sh      # Sign user keys for zone access
│   ├── node-make.sh      # Harden and configure new servers
│   ├── node-sign.sh      # Sign hardened nodes for zone membership
│   └── http-init.sh      # Deploy web services with SSL
├── etc/                   # ⚙️ Configuration storage
│   └── ssh/              # SSH keys and certificates
└── README.md              # This documentation
```

## 🚀 Quick Start

### 1. Create a Zone
```bash
# Create a new security zone
./bin/zone-make.sh production-zone
```

### 2. Create Users
```bash
# Create a user in the zone
./bin/user-make.sh admin production-zone

# Sign the user for zone access
./bin/user-sign.sh etc/ssh/production-zone.skey admin-role admin-production-zone.skey.pub
```

### 3. Harden a Server
```bash
# Harden a new server (run from management machine)
./bin/node-make.sh 192.168.1.100 server01 etc/ssh/production-zone.skey.pub
```

### 4. Deploy Web Services
```bash
# Set up Nginx + SSL on hardened server
./bin/http-init.sh server01 example.com admin@example.com
```

## 🔧 Core Scripts

### Zone Management

#### `zone-make.sh` - Create Zone Infrastructure
```bash
./bin/zone-make.sh <zone-name>
```
- Creates zone SSH key pair (`zone-name.skey`, `zone-name.skey.pub`)
- Establishes trust foundation for the zone
- **Usage**: `./bin/zone-make.sh production`

#### `user-make.sh` - Create User Keys
```bash
./bin/user-make.sh <user> <zone-name>
```
- Generates user SSH key pair for zone access
- **Usage**: `./bin/user-make.sh admin production`

#### `user-sign.sh` - Authorize Users
```bash
./bin/user-sign.sh <zone-skey> <role-name> <user-pkey>
```
- Signs user public key with zone private key
- Grants zone access with specific role
- **Usage**: `./bin/user-sign.sh etc/ssh/production.skey admin-role admin-production.skey.pub`

### Server Management

#### `node-make.sh` - Server Hardening
```bash
./bin/node-make.sh <host-ip> <host-name> <zone-pkey>
```
**What it does:**
- Sets hostname and hosts file
- Installs security packages (fail2ban, ufw, unattended-upgrades)
- Configures firewall (SSH only initially)
- Sets up SSH with ed25519 keys and certificates
- Creates standard user accounts (ops, svc, sdm, env)
- Configures sudo access for env user
- **Usage**: `./bin/node-make.sh 192.168.1.100 web01 etc/ssh/production.skey.pub`

#### `node-sign.sh` - Node Zone Membership
```bash
./bin/node-sign.sh <host> <zone-skey>
```
- Signs hardened node with zone private key
- Establishes trust relationship
- **Usage**: `./bin/node-sign.sh web01 etc/ssh/production.skey`

### Web Services

#### `http-init.sh` - Web Service Deployment
```bash
./bin/http-init.sh <host-name> <site-name> <site-email>
```
**What it deploys:**
- Nginx web server
- Let's Encrypt SSL certificates
- Reverse proxy configuration
- Failover/panic server setup
- **Usage**: `./bin/http-init.sh web01 example.com admin@example.com`

## 🏗️ Architecture

### Security Model
```
Zone Authority (zone-make.sh)
    ↓
User Management (user-make.sh + user-sign.sh)
    ↓
Server Hardening (node-make.sh)
    ↓
Zone Membership (node-sign.sh)
    ↓
Service Deployment (http-init.sh)
```

### User Accounts Created
- **`ops`** - Console management access
- **`svc`** - Web-facing services
- **`sdm`** - Secure data management
- **`env`** - Environment management (sudo access)

### SSH Configuration
- **Host Keys**: ed25519 with self-signed certificates
- **Zone Trust**: Centralized authority via zone keys
- **User Authentication**: Certificate-based with role restrictions
- **Security**: No password auth, strict key verification

## 🔒 Security Features

- **ed25519 SSH keys** for modern cryptography
- **Certificate-based authentication** for trust management
- **Automatic security updates** via unattended-upgrades
- **Firewall configuration** with ufw
- **Intrusion detection** via fail2ban
- **SSL/TLS termination** with Let's Encrypt
- **Reverse proxy** with failover capabilities

## 📋 Prerequisites

### Management Machine
- SSH access to target servers
- GPG/SSH key management tools
- Network access to target infrastructure

### Target Servers
- Ubuntu/Debian-based Linux
- Root SSH access (initially)
- Internet connectivity for package installation
- Valid hostname and DNS resolution

## 🚨 Important Notes

### Security Considerations
- **Zone keys are critical** - protect zone private keys
- **Initial access** requires root SSH (temporarily)
- **Firewall rules** start restrictive (SSH only)
- **User accounts** have specific, limited permissions

### Deployment Order
1. Create zone infrastructure first
2. Harden servers before service deployment
3. Sign nodes for zone membership
4. Deploy services on hardened infrastructure

### Backup Requirements
- Zone private keys (`*.skey`)
- User private keys
- Server configurations
- SSL certificates

## 🔍 Troubleshooting

### Common Issues
- **SSH connection failures**: Check firewall and SSH configuration
- **Certificate errors**: Verify zone key paths and permissions
- **Service failures**: Check systemd status and logs
- **Permission denied**: Verify user roles and sudo configuration

### Debug Commands
```bash
# Check SSH service status
sudo systemctl status sshd

# View SSH logs
sudo journalctl -u sshd

# Check firewall status
sudo ufw status

# Verify certificate validity
ssh-keygen -L -f /etc/ssh/host.cert
```

## 📚 Related Projects

This host project works alongside:
- **SaaS Project** - Application deployment
- **PITS Project** - Picture Image Transfer System
- **Site Project** - Website and management tools
- **PQTR Repository** - Overall infrastructure management

## 🤝 Contributing

1. Test scripts in isolated environments
2. Follow security best practices
3. Document any configuration changes
4. Maintain backward compatibility

## 📄 License

Part of the PQTR project - see repository license.

---

**Production Ready** - This system implements enterprise-grade security practices for Linux server management.
