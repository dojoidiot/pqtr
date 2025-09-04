# Host Project - SSH Zone Management

## What It Does

Creates a secure SSH infrastructure using certificate-based authentication. No passwords, just certificates signed by a zone authority.

## Key Concepts

- **Zone**: A group of servers with shared SSH certificate authority
- **Zone Head**: First server that manages the zone (signs certificates, tracks nodes)  
- **Zone Authority**: Ed25519 SSH CA keypair that signs all certificates

## Quick Setup

### 1. Create Zone (On Desktop)

```bash
cd host/bin

# Create zone authority
./zone-make.sh production

# Create admin user
./user-make.sh env production

# Sign admin's key
./user-sign.sh etc/ssh/production.skey env etc/ssh/env-production.skey.pub
```

### 2. Harden First Server

```bash
# 1. Provision server in cloud panel, note IP and root password
# 2. Harden from desktop with root access:
./node-make.sh 192.168.1.10 prod-node-01 etc/ssh/production.skey.pub
```

### 3. Make It Zone Head

```bash
# Promote to zone head
./zone-init.sh 192.168.1.10 production

# Delete local keys (now on zone head)
rm etc/ssh/production.skey*
```

### 4. Use Zone Head

SSH to zone head as env user, then:

```bash
# Register a node (tells you how to harden it)
zone-boss.sh make 192.168.1.11 node02
# Output: "To harden this node, run from your desktop:
#   ./node-make.sh 192.168.1.11 node02 etc/ssh/production.skey.pub"

# Sign users  
zone-boss.sh sign user alice dev alice.pub

# Sign hosts
zone-boss.sh sign host server01 host.pub

# List nodes
zone-boss.sh list
```

## How It Works

The system separates **provisioning**, **hardening**, and **management**:

1. **Cloud provisioning** - Use cloud panel to create servers (get root password)
2. **Desktop hardening** - Run `node-make.sh` with root access to secure server
3. **Zone head management** - Zone head tracks nodes and signs certificates
4. **Certificate-based SSH** - All access via signed certificates (no passwords)

This separation makes sense because:
- **Cloud panel** provisions bare servers with root passwords
- **Desktop** has the hardening scripts and zone authority
- **Zone head** manages the operational zone (no root access needed)

## Files Created

**Desktop:**
```
host/etc/ssh/
├── production.skey         # Zone CA private key
├── production.skey.pub     # Zone CA public key
└── env-production.skey*    # Admin user keys
```

**Zone Head:**
```
/opt/zone/
├── safe/                   # Encrypted key storage
├── bin/
│   ├── safe.sh            # Encrypted storage (init/save/load)
│   └── zone-boss.sh       # Zone operations (make/sign/list)
├── config/zone.conf       # Zone metadata
└── nodes/                 # Node inventory
```

## Common Tasks

### Grant User Access
```bash
# User sends public key to admin
# Admin on zone head:
zone-boss.sh sign user bob admin bob.pub

# User can now SSH:
ssh -i ~/.ssh/mykey bob@any-zone-node
```

### Add New Server
```bash
# 1. Provision server in cloud panel (get root password/key)

# 2. Register with zone head
ssh env@zone-head
zone-boss.sh make 192.168.1.20 node03
# Shows: "To harden this node, run from your desktop: ..."
exit

# 3. Harden from desktop (using root access from step 1)
./node-make.sh 192.168.1.20 node03 etc/ssh/production.skey.pub

# Now node03 is hardened and part of the zone
```

### Emergency Recovery
```bash
# If zone head dies, restore from backup:
tar -xzf zone-backup.tar.gz
./zone-init.sh 192.168.1.30 production
```

## Security

- **No passwords** - Only SSH certificates
- **Encrypted storage** - Zone keys in "safe" (AES-256)
- **One-way transfer** - Keys move desktop → zone head once
- **Audit logging** - All operations logged
- **Certificate expiry** - Time-limited access

## Scripts Reference

| Script | Purpose | Run From |
|--------|---------|----------|
| `zone-make.sh` | Create zone CA | Desktop |
| `user-make.sh` | Create user keys | Desktop |
| `user-sign.sh` | Sign user cert | Desktop |
| `node-make.sh` | Harden server | Desktop |
| `zone-init.sh` | Create zone head | Desktop |
| `zone-boss.sh` | Zone operations | Zone Head |
| `safe.sh` | Key storage | Zone Head |

## That's It

Simple, secure SSH management without the enterprise complexity.