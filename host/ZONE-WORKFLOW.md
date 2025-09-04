# Zone Creation and Management Workflow

## Complete Workflow: Desktop → Zone Head → Distributed Zone

### Phase 1: Local Bootstrap (On Your Desktop)
```bash
cd host/bin

# 1. Create zone root keys
./zone-make.sh production

# 2. Create env user for administration  
./user-make.sh env production

# 3. Sign env user with zone authority
./user-sign.sh etc/ssh/production.skey env etc/ssh/env-production.skey.pub
```

### Phase 2: Server Hardening (Remote Server)
```bash
# 4. Provision cloud server (get root access)
# Then from desktop:
./node-make.sh 192.168.1.10 prod-node-01 etc/ssh/production.skey.pub
```

### Phase 3: Zone Head Promotion (From Desktop)
```bash
# 5. Promote hardened node to zone head
./zone-init.sh 192.168.1.10 production

# 6. Verify zone head is running
ssh env@192.168.1.10 '/opt/zone/bin/zone-manage.sh list-nodes'
```

### Phase 4: Zone Operations (From Zone Head)

Once the zone head is operational, all zone management happens there:

#### Adding New Nodes
```bash
# From zone head (or remotely via SSH)
/opt/zone/bin/zone-manage.sh add-node 192.168.1.11 prod-node-02
```

#### Managing Users
```bash
# Sign a developer's SSH key
/opt/zone/bin/zone-manage.sh sign-user alice developer ~/.ssh/alice.pub

# Sign an admin's SSH key  
/opt/zone/bin/zone-manage.sh sign-user bob admin ~/.ssh/bob.pub
```

#### Managing Hosts
```bash
# Sign a node's host key
/opt/zone/bin/zone-manage.sh sign-host prod-node-02 /etc/ssh/ssh_host_ed25519_key.pub
```

## What Happens at Each Step

### 1. **zone-make.sh**: Creates zone certificate authority
- Generates ed25519 keypair
- This becomes the trust root for the entire zone

### 2. **user-make.sh**: Creates user keypair
- Standard SSH key generation
- Will be signed by zone CA

### 3. **user-sign.sh**: Grants zone authority
- Signs user key with zone CA
- Creates SSH certificate with "env" principal
- Enables passwordless access to hardened nodes

### 4. **node-make.sh**: Hardens server
- Configures SSH with certificate authentication
- Sets up env user with sudo access
- Installs zone CA public key for trust
- Disables password authentication
- Configures firewall rules

### 5. **zone-init.sh**: Promotes to zone head
- Transfers zone CA keys (encrypted)
- Installs keystore for secure storage
- Sets up management scripts
- Removes local unencrypted keys
- Configures zone operations

### 6. **zone-manage.sh**: Ongoing operations
- Signs new user/host certificates
- Manages zone membership
- Handles key rotation
- Tracks node inventory

## Security Model

```
[Desktop: Zone Creation]
    ↓ (zone CA keys)
[Zone Head: Secure Storage]
    ↓ (certificates)
[Zone Nodes: Certificate Auth Only]
```

1. **Zone keys created offline** on trusted desktop
2. **One-time transfer** to zone head over SSH
3. **Encrypted storage** on zone head
4. **Certificate-based auth** for all zone members
5. **No passwords** anywhere in production

## Key Storage on Zone Head

The zone head uses a simple encrypted keystore:
- Master password in `/opt/zone/.master` (generated randomly)
- Zone CA keys encrypted with OpenSSL AES-256
- Retrieved only when signing operations needed
- Keys immediately shredded after use

## Adding More Zone Heads (HA)

For high availability, you can promote additional zone heads:

```bash
# From first zone head, replicate to second
/opt/zone/bin/zone-replicate.sh 192.168.1.20

# Both heads can now sign certificates
# Use DNS round-robin or load balancer
```

## Disaster Recovery

If zone head fails:

1. **Have backups**: Regular encrypted backups of `/opt/zone/keystore/`
2. **Promote new head**: Run zone-init.sh on a new hardened node
3. **Restore keystore**: Restore from encrypted backup
4. **Resume operations**: New head has full zone authority

## Best Practices

1. **Delete local keys** after zone head is operational
2. **Regular backups** of zone head keystore
3. **Monitor certificate expiry** (cron job included)
4. **Audit log reviews** weekly
5. **Key rotation** annually or on compromise
6. **Test disaster recovery** quarterly

## Common Operations

### Revoke a user's access
```bash
# On zone head
echo "$(date): Revoked" >> /opt/zone/revoked/alice
# Then on all nodes: Update authorized_keys
```

### Rotate zone CA keys
```bash
# Generate new zone keys
./zone-make.sh production-v2
# Sign all active users/hosts with new CA
# Deploy new CA public key to all nodes
# Phase out old CA
```

### Emergency access
```bash
# If zone head is down, use local backup
gpg -d zone-backup.gpg | tar -xzf -
# Use recovered keys to sign emergency cert
ssh-keygen -s recovered-ca.key -n root -I emergency emergency.pub
```

## This Achieves

✅ **No yak-shaving** - Simple scripts, standard tools  
✅ **Secure by default** - Certificate auth, no passwords  
✅ **Distributed control** - Zone head manages operations  
✅ **Disaster recovery** - Clear backup/restore process  
✅ **Audit trail** - All operations logged  
✅ **Practical** - Works with existing infrastructure