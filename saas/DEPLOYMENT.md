# Deployment Guide

This guide covers deploying the SaaS project to different environments.

## Prerequisites

- Linux server (Ubuntu 20.04+ recommended)
- PostgreSQL 15+
- PostgREST binary
- GPG for encryption
- Basic Linux administration knowledge

## Quick Deployment

### 1. Server Setup

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install PostgreSQL
sudo apt install postgresql postgresql-contrib

# Install GPG
sudo apt install gnupg

# Install curl for testing
sudo apt install curl
```

### 2. Download PostgREST

```bash
# Download latest PostgREST
cd /tmp
wget https://github.com/PostgREST/postgrest/releases/latest/download/postgrest-linux-x64-static.tar.xz
tar -xf postgrest-linux-x64-static.tar.xz
sudo mv postgrest /usr/local/bin/
sudo chmod +x /usr/local/bin/postgrest
```

### 3. Deploy Project

```bash
# Clone or copy project to server
cd /opt
sudo git clone <your-repo> saas
sudo chown -R $USER:$USER saas
cd saas

# Run setup
./scripts/project.sh setup
```

## Environment-Specific Deployment

### Development Environment

```bash
# Switch to development
./scripts/project.sh env dev

# Start services
./scripts/project.sh start

# Test deployment
./scripts/project.sh test
```

### Test Environment

```bash
# Switch to test
./scripts/project.sh env tst

# Customize test settings
# Edit .env-tst with test database details

# Start services
./scripts/project.sh start

# Run health checks
./scripts/project.sh health
```

### Production Environment

```bash
# Switch to production
./scripts/project.sh env pro

# IMPORTANT: Customize production settings
# Edit .env-pro with:
# - Strong passwords
# - Production database details
# - SSL certificates
# - Security settings

# Encrypt environment files
./scripts/project.sh secure encrypt

# Start services
./scripts/project.sh start

# Verify deployment
./scripts/project.sh health
```

## Security Hardening

### 1. Database Security

```bash
# Change default passwords
sudo -u postgres psql -c "ALTER USER rest_user WITH PASSWORD 'strong_password_here';"

# Restrict network access
sudo nano /etc/postgresql/*/main/postgresql.conf
# Set: listen_addresses = 'localhost'

# Restart PostgreSQL
sudo systemctl restart postgresql
```

### 2. Firewall Configuration

```bash
# Install UFW
sudo apt install ufw

# Configure firewall
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow ssh
sudo ufw allow 5432/tcp  # PostgreSQL (if external access needed)
sudo ufw allow 3000/tcp  # PostgREST API
sudo ufw enable
```

### 3. SSL/TLS Configuration

```bash
# Install Certbot
sudo apt install certbot

# Get SSL certificate
sudo certbot certonly --standalone -d yourdomain.com

# Configure PostgREST with SSL
# Edit postgrest.conf to include SSL settings
```

## Monitoring & Maintenance

### 1. Service Management

```bash
# Check service status
./scripts/project.sh status

# View logs
./scripts/project.sh logs

# Health checks
./scripts/project.sh health
```

### 2. Backup Strategy

```bash
# Create encrypted backup
./scripts/project.sh secure backup

# Schedule automatic backups
crontab -e
# Add: 0 2 * * * cd /opt/saas && ./scripts/project.sh secure backup
```

### 3. Log Management

```bash
# Rotate logs
sudo logrotate -f /etc/logrotate.d/postgrest

# Monitor log size
du -h postgrest.log
```

## Troubleshooting

### Common Issues

1. **PostgreSQL Connection Failed**
   ```bash
   sudo systemctl status postgresql
   sudo journalctl -u postgresql
   ```

2. **PostgREST Not Starting**
   ```bash
   cat postgrest.log
   ./scripts/project.sh status
   ```

3. **Permission Denied**
   ```bash
   sudo chown -R $USER:$USER /opt/saas
   chmod +x scripts/*.sh
   ```

### Recovery Procedures

1. **Service Recovery**
   ```bash
   ./scripts/project.sh restart
   ```

2. **Database Recovery**
   ```bash
   ./scripts/project.sh secure restore
   ```

3. **Full Reset**
   ```bash
   ./scripts/project.sh uninstall
   ./scripts/project.sh setup
   ```

## Performance Tuning

### 1. PostgreSQL Tuning

```bash
# Edit PostgreSQL configuration
sudo nano /etc/postgresql/*/main/postgresql.conf

# Recommended settings for production:
# shared_buffers = 256MB
# effective_cache_size = 1GB
# work_mem = 4MB
# maintenance_work_mem = 64MB
```

### 2. PostgREST Tuning

```bash
# Edit PostgREST configuration
nano config/postgrest.conf

# Performance settings:
# db-pool = 20
# db-pool-timeout = 10
# max-rows = 1000
```

## Scaling Considerations

### 1. Load Balancing

- Use Nginx as reverse proxy
- Implement multiple PostgREST instances
- Use connection pooling

### 2. Database Scaling

- Read replicas for read-heavy workloads
- Connection pooling with PgBouncer
- Consider database sharding for large datasets

### 3. Monitoring

- Prometheus + Grafana for metrics
- ELK stack for log aggregation
- Custom health check endpoints

## Support & Maintenance

### 1. Regular Tasks

- Weekly health checks
- Monthly security updates
- Quarterly backup testing
- Annual performance review

### 2. Update Procedures

```bash
# Backup current deployment
./scripts/project.sh backup

# Update code
git pull origin main

# Test deployment
./scripts/project.sh test

# Verify health
./scripts/project.sh health
```

### 3. Emergency Contacts

- Database Administrator: [Contact Info]
- System Administrator: [Contact Info]
- Development Team: [Contact Info]

---

**Note**: This deployment guide covers the basics. For production deployments, consider additional security measures, monitoring, and disaster recovery planning.
