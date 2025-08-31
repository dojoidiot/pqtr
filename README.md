# PQTR - PostgreSQL + PostgREST SaaS Template

A production-ready SaaS backend template using PostgreSQL and PostgREST, designed for rapid development and deployment of multi-tenant applications.

## 🚀 Quick Start

```bash
# Clone the repository
git clone <your-repo> pqtr
cd pqtr/saas

# First time setup
./quick-start.sh setup

# Start services
./quick-start.sh start

# Test the API
./quick-start.sh test
```

## 🏗️ Project Structure

```
pqtr/
├── saas/                    # Main SaaS project
│   ├── config/             # Configuration files
│   │   ├── init.sql        # Database schema
│   │   └── postgrest.conf  # PostgREST config
│   ├── scripts/            # Management scripts
│   │   ├── project.sh      # Unified project manager
│   │   ├── setup-local.sh  # Local setup
│   │   ├── env-manager.sh  # Environment management
│   │   ├── safe.sh         # Encryption/security
│   │   └── ...             # Other utilities
│   ├── .env-*              # Environment configurations
│   ├── project.conf        # Project settings
│   ├── quick-start.sh      # Simple commands
│   ├── DEPLOYMENT.md       # Deployment guide
│   └── README.md           # Detailed documentation
└── README.md               # This file
```

## 🎯 Key Features

- **Multi-tenant SaaS architecture** with Row Level Security
- **Unified project management** - single script for all operations
- **Multi-environment support** (dev/test/production)
- **Secure configuration management** with GPG encryption
- **Production-ready** with health checks and monitoring
- **Comprehensive deployment guide** for all environments

## 🛠️ Management Commands

### Simple Commands (Quick Start)
```bash
./quick-start.sh setup    # First time setup
./quick-start.sh start    # Start services
./quick-start.sh test     # Test API
./quick-start.sh status   # Check status
./quick-start.sh stop     # Stop services
```

### Advanced Commands (Full Control)
```bash
./scripts/project.sh setup      # Complete setup
./scripts/project.sh start      # Start services
./scripts/project.sh env dev    # Switch environment
./scripts/project.sh secure encrypt  # Encrypt configs
./scripts/project.sh health     # Health checks
./scripts/project.sh backup     # Create backup
```

## 🌍 Environment Management

```bash
# Switch environments
./scripts/project.sh env dev    # Development
./scripts/project.sh env tst    # Test
./scripts/project.sh env pro    # Production

# Secure environment files
./scripts/project.sh secure encrypt   # Encrypt
./scripts/project.sh secure decrypt   # Decrypt
```

## 🔒 Security Features

- **Row Level Security (RLS)** for data isolation
- **JWT authentication** with configurable expiration
- **Environment encryption** using GPG
- **Secure backup system** with encrypted storage
- **Production hardening** guidelines

## 📚 Documentation

- **[SaaS Project README](saas/README.md)** - Complete project documentation
- **[Deployment Guide](saas/DEPLOYMENT.md)** - Production deployment instructions
- **[API Examples](saas/README.md#api-usage-examples)** - Usage examples
- **[Database Schema](saas/README.md#database-schema)** - Table structure

## 🚀 Deployment

### Development
```bash
./quick-start.sh setup
./quick-start.sh start
```

### Production
```bash
# Follow DEPLOYMENT.md for production setup
./scripts/project.sh env pro
./scripts/project.sh secure encrypt
./scripts/project.sh start
./scripts/project.sh health
```

## 🔧 Prerequisites

- **PostgreSQL 15+** - Database server
- **PostgREST** - REST API generator
- **GPG** - For encryption (optional)
- **Linux/macOS** - Operating system

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License.

## 🆘 Support

- **Documentation**: Check the [SaaS project README](saas/README.md)
- **Deployment**: See [DEPLOYMENT.md](saas/DEPLOYMENT.md)
- **Issues**: Open an issue in the repository
- **Health Checks**: Run `./scripts/project.sh health`

---

**Ready for production use** - This template includes security best practices, monitoring, and deployment guides for enterprise environments.
