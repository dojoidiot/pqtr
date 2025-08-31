# SaaS Project with PostgreSQL and PostgREST

This project provides a complete SaaS backend setup using PostgreSQL as the database and PostgREST as the REST API layer.

## 🏗️ Project Structure

```
saas/
├── config/             # Configuration files
│   ├── init.sql        # Database schema and RLS policies
│   ├── postgrest.conf  # PostgREST configuration
│   └── health.sql      # Health check and monitoring functions
├── scripts/            # Management scripts
│   ├── project.sh      # Unified project manager
│   ├── setup-local.sh  # Local setup and initialization
│   ├── manage-services.sh # Service management (start/stop/status)
│   ├── env-manager.sh  # Environment management
│   ├── safe.sh         # Encryption/security management
│   ├── test-api-local.sh # API testing and validation
│   └── uninstall-local.sh # Clean uninstallation
├── .env-*              # Environment configurations
│   ├── .env-dev        # Development environment
│   ├── .env-tst        # Test environment
│   └── .env-pro        # Production environment
├── project.conf        # Project-wide configuration
├── quick-start.sh      # Simple command wrapper
├── DEPLOYMENT.md       # Production deployment guide
├── PROJECT_STATUS.md   # Project status and readiness
└── README.md           # This documentation
```

## 🏗️ Architecture

- **PostgreSQL 15**: Primary database with Row Level Security (RLS)
- **PostgREST**: Auto-generated REST API from PostgreSQL schema

## 🎯 Key Features

- **Multi-tenant SaaS architecture** with Row Level Security
- **Unified project management** - single script for all operations
- **Multi-environment support** (dev/test/production)
- **Secure configuration management** with GPG encryption
- **Production-ready** with health checks and monitoring
- **Comprehensive deployment guide** for all environments
- User management with organizations
- Role-based access control (RBAC)
- JWT authentication support
- RESTful API endpoints

## Prerequisites

- PostgreSQL 15+ installed and running
- PostgREST binary installed
- GPG (GNU Privacy Guard) for encryption
- Git

## Quick Start

1. **Install PostgreSQL:**
   ```bash
   # Ubuntu/Debian
   sudo apt update
   sudo apt install postgresql postgresql-contrib
   
   # Start PostgreSQL service
   sudo systemctl start postgresql
   sudo systemctl enable postgresql
   ```

2. **Install PostgREST:**
   ```bash
   # Download latest release
   wget https://github.com/PostgREST/postgrest/releases/latest/download/postgrest-linux-x64-static.tar.xz
   tar -xf postgrest-linux-x64-static.tar.xz
   sudo mv postgrest /usr/local/bin/
   
   # Or use package manager if available
   ```

3. **Install GPG (for encryption):**
   ```bash
   # Ubuntu/Debian
   sudo apt install gnupg
   
   # CentOS/RHEL
   sudo yum install gnupg
   
   # macOS
   brew install gnupg
   ```

4. **Set up the project:**
   ```bash
   cd saas
   
   # Run the setup script
   ./scripts/setup-local.sh
   ```

5. **Manage services:**
   ```bash
   # Start services
   ./scripts/manage-services.sh start
   
   # Check status
   ./scripts/manage-services.sh status
   
   # View logs
   ./scripts/manage-services.sh logs
   ```

6. **Test the setup:**
   ```bash
   ./scripts/test-api-local.sh
   ```

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

The project supports multiple environment configurations:

### Environment Files

- **`.env-dev`** - Local development environment
- **`.env-tst`** - Shared test server environment  
- **`.env-pro`** - Production environment

### Environment Management Commands

```bash
# Switch environments
./scripts/project.sh env dev    # Development
./scripts/project.sh env tst    # Test
./scripts/project.sh env pro    # Production

# Secure environment files
./scripts/project.sh secure encrypt   # Encrypt
./scripts/project.sh secure decrypt   # Decrypt
```
# Switch between environments
./scripts/env-manager.sh dev    # Switch to development
./scripts/env-manager.sh tst    # Switch to test
./scripts/env-manager.sh pro    # Switch to production

# Show current configuration
./scripts/env-manager.sh show

# Validate environment files
./scripts/env-manager.sh validate

# Create missing environment files
./scripts/env-manager.sh create
```

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

### Secure Environment Storage

For secure storage and sharing of environment files:

```bash
# Encrypt all environment files
./scripts/safe.sh encrypt

# Decrypt environment files when needed
./scripts/safe.sh decrypt

# Create encrypted backup of current environment
./scripts/safe.sh backup

# Restore from encrypted backup
./scripts/safe.sh restore

# List available encrypted files
./scripts/safe.sh list

# Clean decrypted files (keep encrypted)
./scripts/safe.sh clean
```

### Environment Variables

Each environment file contains:

- **PostgreSQL Configuration**: Database connection details
- **PostgREST Configuration**: API settings and JWT configuration
- **Environment-Specific Settings**: Log levels, debugging, security features

## Service Endpoints

- **PostgreSQL**: `localhost:5432`
  - Database: `saas_db`
  - Username: `rest_user`
  - Password: `rest_pass`

- **PostgREST API**: `http://localhost:3000`
  - Auto-generated REST endpoints for all tables
  - JWT authentication support

## Database Schema

### Tables

- **users**: User accounts and profiles
- **organizations**: Multi-tenant organizations
- **user_organizations**: Many-to-many relationship with roles
- **projects**: Projects within organizations

### Security Features

- **Row Level Security (RLS)** for data isolation
- **JWT authentication** with configurable expiration
- **Environment encryption** using GPG
- **Secure backup system** with encrypted storage
- **Production hardening** guidelines
- Role-based access control policies
- Secure password hashing

## API Usage Examples

### Get all organizations (requires authentication)
```bash
curl -H "Authorization: Bearer YOUR_JWT_TOKEN" \
     http://localhost:3000/organizations
```

### Get user profile
```bash
curl -H "Authorization: Bearer YOUR_JWT_TOKEN" \
     http://localhost:3000/users?id=eq.YOUR_USER_ID
```

### Create a new project
```bash
curl -X POST \
     -H "Authorization: Bearer YOUR_JWT_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"name":"New Project","description":"Project description","organization_id":"ORG_UUID"}' \
     http://localhost:3000/projects
```

## Development

### Adding new tables

1. Add table definition to `config/init.sql`
2. Create appropriate RLS policies
3. Add indexes for performance
4. Restart the PostgreSQL service

### Modifying existing schema

1. Create migration scripts in `config/migrations/`
2. Apply migrations to running database
3. Update PostgREST configuration if needed

### Environment-Specific Development

- **Development**: Full debugging, verbose logging, local database
- **Test**: Moderate logging, test database, shared resources
- **Production**: Minimal logging, production database, security features

### Secure Development Workflow

1. **Setup**: Create environment files with `./scripts/env-manager.sh create`
2. **Development**: Work with decrypted `.env-*` files
3. **Secure**: Encrypt files with `./scripts/safe.sh encrypt` before committing
4. **Share**: Share encrypted `.env-*.gpg` files securely
5. **Deploy**: Decrypt files on target systems with `./scripts/safe.sh decrypt`

## Environment Variables

Key environment variables can be customized in each `.env-*` file:

- `POSTGRES_PASSWORD`: Database password
- `PGRST_JWT_SECRET`: JWT signing secret
- `NODE_ENV`: Environment type (development/test/production)
- `LOG_LEVEL`: Logging verbosity

## Troubleshooting

### Common Issues

1. **PostgreSQL connection refused**: Check if PostgreSQL service is running
   ```bash
   sudo systemctl status postgresql
   ```

2. **Permission denied**: Ensure proper user permissions
   ```bash
   sudo -u postgres psql -c "ALTER USER rest_user CREATEDB;"
   ```

3. **PostgREST not starting**: Check configuration file and database connection

4. **Environment issues**: Validate environment files
   ```bash
   ./scripts/env-manager.sh validate
   ```

5. **GPG not found**: Install GPG for encryption features
   ```bash
   sudo apt install gnupg  # Ubuntu/Debian
   ```

### Logs

View service logs:

```bash
# PostgreSQL logs
sudo journalctl -u postgresql

# PostgREST logs (if running as service)
sudo journalctl -u postgrest

# Or check the log file directly
tail -f postgrest.log
```

### Reset Database

To completely reset the database:
```bash
sudo -u postgres psql -c "DROP DATABASE saas_db;"
sudo -u postgres psql -c "CREATE DATABASE saas_db;"
psql -h localhost -U rest_user -d saas_db -f config/init.sql
```

## Security Notes

- Change default passwords in production
- Use strong JWT secrets
- Enable SSL in production
- Regularly update software
- Review and customize RLS policies
- Never commit `.env-*` files to version control
- Use `./scripts/safe.sh encrypt` to secure environment files
- Store encrypted files securely and share passwords separately
- Regularly rotate encryption passwords

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is licensed under the MIT License.
