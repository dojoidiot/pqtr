# HashiCorp Vault Explained

## What is Vault?

Vault is an open-source tool for **securely storing and accessing secrets**. A "secret" is anything you want to tightly control access to, such as:
- API keys
- Passwords
- SSH keys
- SSL/TLS certificates
- Database credentials
- Encryption keys

Think of Vault as a **highly secure, encrypted database specifically designed for sensitive data** with advanced access control and audit logging.

## 🔑 Core Concepts

### 1. **Secrets Engines**
These are components that store, generate, or encrypt data:

```bash
# Key-Value store (static secrets)
vault kv put secret/myapp/config api_key="abc123" db_pass="xyz789"

# Dynamic database credentials (generated on-demand)
vault read database/creds/my-role
# Returns: username="v-token-my-role-x8x9" password="A1a-78zkpqn"

# SSH Certificate Authority
vault write ssh-ca/sign/admin public_key=@~/.ssh/id_rsa.pub
# Returns: signed SSH certificate
```

### 2. **Authentication Methods**
How users and applications prove their identity:

```bash
# Username/password
vault login -method=userpass username=john

# Token (like an API key)
vault login token=s.XmpNPoi9sRhYtdKHaQhkHP6x

# AppRole (for applications)
vault write auth/approle/login \
    role_id="6b5b4f0a-..." \
    secret_id="e38c4f0e-..."

# Kubernetes (for pods)
vault write auth/kubernetes/login role=myapp jwt=$SERVICE_ACCOUNT_TOKEN
```

### 3. **Policies**
Define who can do what:

```hcl
# policy.hcl
path "secret/data/prod/*" {
  capabilities = ["read"]
}

path "secret/data/dev/*" {
  capabilities = ["create", "read", "update", "delete"]
}

path "ssh-ca/sign/admin" {
  capabilities = ["update"]
}
```

### 4. **Seal/Unseal**
Vault starts in a **sealed** state (encrypted, unusable). Must be unsealed with master keys:

```bash
# Initialize Vault (first time only)
vault operator init
# Returns 5 unseal keys and 1 root token

# Unseal (need 3 of 5 keys by default)
vault operator unseal <key-1>
vault operator unseal <key-2>
vault operator unseal <key-3>
# Vault is now ready to use

# Auto-unseal with AWS KMS (production)
seal "awskms" {
  region = "us-east-1"
  kms_key_id = "alias/vault-unseal"
}
```

## 💾 Storage Backends

Vault itself doesn't store data - it uses a storage backend:

```hcl
# Consul (recommended for HA)
storage "consul" {
  address = "127.0.0.1:8500"
  path    = "vault/"
}

# PostgreSQL
storage "postgresql" {
  connection_url = "postgres://user:pass@localhost:5432/vault"
}

# Local file (dev only)
storage "file" {
  path = "/opt/vault/data"
}

# etcd
storage "etcd" {
  address = "http://localhost:2379"
  path    = "vault/"
}
```

## 🚀 Real-World Use Cases

### 1. **Database Credential Rotation**
```bash
# Configure database connection
vault write database/config/postgresql \
    plugin_name=postgresql-database-plugin \
    connection_url="postgresql://{{username}}:{{password}}@localhost:5432/mydb" \
    username="vault" \
    password="vault-password"

# Create role with 1-hour TTL
vault write database/roles/readonly \
    db_name=postgresql \
    creation_statements="CREATE ROLE \"{{name}}\" WITH LOGIN PASSWORD '{{password}}' VALID UNTIL '{{expiration}}'; \
    GRANT SELECT ON ALL TABLES IN SCHEMA public TO \"{{name}}\";" \
    default_ttl="1h" \
    max_ttl="24h"

# Application requests credentials
vault read database/creds/readonly
# Returns temporary username/password that auto-expire
```

### 2. **SSH Certificate Authority (Perfect for Zone Head)**
```bash
# Configure SSH CA
vault secrets enable -path=ssh-ca ssh
vault write ssh-ca/config/ca \
    private_key=@ca_key \
    public_key=@ca_key.pub

# Create role for admins
vault write ssh-ca/roles/admin \
    key_type=ca \
    ttl=8h \
    max_ttl=24h \
    allow_user_certificates=true \
    allowed_users="admin,deploy" \
    default_extensions='{"permit-pty": "", "permit-port-forwarding": ""}'

# Sign user's SSH key
vault write -field=signed_key ssh-ca/sign/admin \
    public_key=@~/.ssh/id_rsa.pub \
    valid_principals="admin,deploy" \
    ttl="8h" > ~/.ssh/id_rsa-cert.pub

# SSH using certificate (no password needed)
ssh -i ~/.ssh/id_rsa admin@server
```

### 3. **Encryption as a Service**
```bash
# Enable transit engine
vault secrets enable transit

# Create encryption key
vault write -f transit/keys/myapp

# Encrypt data
vault write transit/encrypt/myapp \
    plaintext=$(echo "sensitive data" | base64)
# Returns: ciphertext="vault:v1:8SDd3WHDOjf7mq69CyCqYjBXAiQQAVZRkFM="

# Decrypt data
vault write transit/decrypt/myapp \
    ciphertext="vault:v1:8SDd3WHDOjf7mq69CyCqYjBXAiQQAVZRkFM="
# Returns: plaintext (base64 encoded)
```

## 🏗️ Architecture for Zone Head

```bash
# Zone Head Vault Setup
zone-head/
├── vault/
│   ├── config/
│   │   └── server.hcl          # Main configuration
│   ├── policies/
│   │   ├── zone-admin.hcl      # Zone administrator policy
│   │   ├── node-member.hcl     # Zone member policy
│   │   └── ci-deploy.hcl       # CI/CD deployment policy
│   ├── data/                   # Encrypted storage (if using file backend)
│   └── audit/                  # Audit logs
```

**server.hcl for Zone Head:**
```hcl
# Listener configuration
listener "tcp" {
  address     = "0.0.0.0:8200"
  tls_cert_file = "/opt/vault/tls/cert.pem"
  tls_key_file  = "/opt/vault/tls/key.pem"
}

# Storage backend (using Consul)
storage "consul" {
  address = "127.0.0.1:8500"
  path    = "vault/"
  scheme  = "https"
  tls_ca_file = "/opt/consul/tls/ca.pem"
  tls_cert_file = "/opt/consul/tls/cert.pem"
  tls_key_file = "/opt/consul/tls/key.pem"
}

# High Availability
api_addr = "https://zone-head.local:8200"
cluster_addr = "https://zone-head.local:8201"

# Audit logging
audit {
  file {
    path = "/var/log/vault/audit.log"
  }
}

# Auto-unseal using local TPM chip
seal "pkcs11" {
  lib = "/usr/lib/libtpm2_pkcs11.so"
  slot = "0"
  pin = "file:///opt/vault/tpm-pin"
  key_label = "vault-unseal"
}

# Telemetry
telemetry {
  prometheus_retention_time = "30s"
  disable_hostname = true
}
```

## 📝 Practical Zone Head Implementation

### Step 1: Initialize Zone Head Vault
```bash
# Start Vault
vault server -config=/opt/vault/config/server.hcl

# Initialize
vault operator init -key-shares=5 -key-threshold=3 \
    -format=json > /opt/vault/init.json

# Unseal
vault operator unseal $(jq -r '.unseal_keys_b64[0]' init.json)
vault operator unseal $(jq -r '.unseal_keys_b64[1]' init.json)
vault operator unseal $(jq -r '.unseal_keys_b64[2]' init.json)

# Login as root
vault login $(jq -r '.root_token' init.json)
```

### Step 2: Configure for SSH CA
```bash
# Enable SSH engine
vault secrets enable -path=ssh-zone-ca ssh

# Write CA keys
vault write ssh-zone-ca/config/ca \
    private_key=@/opt/zone/ca/zone-ca-key \
    public_key=@/opt/zone/ca/zone-ca-key.pub

# Create signing roles
vault write ssh-zone-ca/roles/zone-admin \
    key_type=ca \
    ttl=24h \
    allow_user_certificates=true \
    allowed_users="admin,operator" \
    default_extensions='{"permit-pty": "", "permit-port-forwarding": ""}'

vault write ssh-zone-ca/roles/zone-node \
    key_type=ca \
    ttl=720h \
    allow_host_certificates=true \
    allowed_domains="*.zone.local" \
    allow_subdomains=true
```

### Step 3: Store Zone Secrets
```bash
# Enable KV store
vault secrets enable -path=zone-secrets kv-v2

# Store zone configuration
vault kv put zone-secrets/config \
    consul_encrypt_key="$(consul keygen)" \
    zone_id="prod-001" \
    zone_domain="prod.zone.local"

# Store node enrollment tokens
vault kv put zone-secrets/enrollment/node01 \
    token="$(uuidgen)" \
    expires="2024-12-31"
```

### Step 4: Create Policies
```bash
# Zone admin policy
cat > zone-admin.hcl <<EOF
# Manage SSH certificates
path "ssh-zone-ca/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# Read zone secrets
path "zone-secrets/*" {
  capabilities = ["read", "list"]
}

# Manage auth methods
path "auth/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}
EOF

vault policy write zone-admin zone-admin.hcl

# Zone member policy
cat > zone-member.hcl <<EOF
# Get host certificate
path "ssh-zone-ca/sign/zone-node" {
  capabilities = ["update"]
}

# Read own configuration
path "zone-secrets/nodes/{{identity.entity.aliases.auth_cert_*.name}}" {
  capabilities = ["read"]
}
EOF

vault policy write zone-member zone-member.hcl
```

## 🔒 Security Features

1. **Encryption at Rest**: All data encrypted with AES-256-GCM
2. **Encryption in Transit**: TLS for all communications
3. **Audit Logging**: Every operation logged with who/what/when
4. **Dynamic Secrets**: Credentials that auto-expire
5. **Leasing & Renewal**: Time-bound access to secrets
6. **Revocation**: Instantly revoke access to secrets
7. **Response Wrapping**: Single-use tokens for secret retrieval

## 📊 Monitoring Vault

```bash
# Check status
vault status

# Monitor audit log
tail -f /var/log/vault/audit.log | jq

# Metrics endpoint (Prometheus format)
curl https://zone-head:8200/v1/sys/metrics

# List active leases
vault list sys/leases/lookup/database/creds/readonly

# Check health
curl https://zone-head:8200/v1/sys/health
```

## 🚨 Common Operations

```bash
# Backup (using Consul backend)
consul snapshot save vault-backup.snap

# Restore
consul snapshot restore vault-backup.snap

# Rotate encryption key
vault operator rotate

# Rekey master keys
vault operator rekey -init -key-shares=5 -key-threshold=3

# Step down (trigger leader election)
vault operator step-down

# Generate root token (emergency)
vault operator generate-root -init
```

## Why Vault for Zone Head?

1. **Centralized Secret Management**: Single source of truth for all zone secrets
2. **Dynamic SSH Certificates**: Auto-expiring certificates for users and hosts
3. **Audit Trail**: Complete history of who accessed what
4. **High Availability**: Can run in HA mode with Consul
5. **API-First**: Everything accessible via REST API
6. **Fine-Grained Access Control**: Policies down to specific paths
7. **Encryption as a Service**: Offload encryption to Vault
8. **Auto-Unseal**: Can use TPM/HSM/Cloud KMS for automatic unsealing

Vault essentially becomes the **security brain** of your zone head, managing all cryptographic operations and access control in a centralized, auditable way.