# Lightweight Script-Based Keystore for Zone Head

## Yes! Absolutely feasible with Linux built-in tools

You can build a robust key management system using just:
- **GPG** (GnuPG) - Encryption and key management
- **pass** - Unix password store (GPG-based)
- **systemd-creds** - Systemd credential storage
- **kernel keyring** - Linux kernel keystore
- **OpenSSL** - Encryption primitives
- **Age** - Modern simple encryption tool

## 🔧 Option 1: GPG-Based Keystore (Recommended)

### Setup
```bash
#!/bin/bash
# zone-keystore-init.sh

ZONE_NAME=${1:-production}
KEYSTORE_DIR="/opt/zone/keystore"

# Create GPG key for zone head
gpg --batch --generate-key <<EOF
Key-Type: RSA
Key-Length: 4096
Subkey-Type: RSA
Subkey-Length: 4096
Name-Real: Zone Head $ZONE_NAME
Name-Email: zone-$ZONE_NAME@localhost
Expire-Date: 2y
%no-protection
%commit
EOF

# Export zone head GPG key
gpg --export-secret-keys "Zone Head $ZONE_NAME" > $KEYSTORE_DIR/zone-master.gpg
gpg --export "Zone Head $ZONE_NAME" > $KEYSTORE_DIR/zone-master.pub

# Initialize keystore directory
mkdir -p $KEYSTORE_DIR/{keys,certs,tokens,audit}
chmod 700 $KEYSTORE_DIR
```

### Key Storage Functions
```bash
#!/bin/bash
# keystore.sh - Lightweight key management

KEYSTORE_DIR="/opt/zone/keystore"
ZONE_GPG_ID="Zone Head production"
AUDIT_LOG="$KEYSTORE_DIR/audit/audit.log"

# Store a key
store_key() {
    local key_name=$1
    local key_data=$2
    local metadata=$3
    
    # Encrypt with GPG
    echo "$key_data" | gpg --encrypt --armor \
        --recipient "$ZONE_GPG_ID" \
        > "$KEYSTORE_DIR/keys/${key_name}.gpg"
    
    # Store metadata
    echo "$metadata" > "$KEYSTORE_DIR/keys/${key_name}.meta"
    
    # Audit log
    echo "$(date -Iseconds)|STORE|$key_name|$USER|$metadata" >> $AUDIT_LOG
    
    echo "Key stored: $key_name"
}

# Retrieve a key
get_key() {
    local key_name=$1
    
    if [[ ! -f "$KEYSTORE_DIR/keys/${key_name}.gpg" ]]; then
        echo "Key not found: $key_name" >&2
        return 1
    fi
    
    # Decrypt with GPG
    gpg --decrypt --quiet "$KEYSTORE_DIR/keys/${key_name}.gpg"
    
    # Audit log
    echo "$(date -Iseconds)|RETRIEVE|$key_name|$USER" >> $AUDIT_LOG
}

# List keys
list_keys() {
    for key in $KEYSTORE_DIR/keys/*.gpg; do
        basename "$key" .gpg
    done
}

# Rotate a key
rotate_key() {
    local key_name=$1
    local new_key_data=$2
    
    # Backup old key
    mv "$KEYSTORE_DIR/keys/${key_name}.gpg" \
       "$KEYSTORE_DIR/keys/${key_name}.gpg.$(date +%Y%m%d-%H%M%S)"
    
    # Store new key
    store_key "$key_name" "$new_key_data" "rotated:$(date -Iseconds)"
}

# Delete a key
delete_key() {
    local key_name=$1
    
    # Move to revoked directory
    mkdir -p "$KEYSTORE_DIR/revoked"
    mv "$KEYSTORE_DIR/keys/${key_name}.gpg" \
       "$KEYSTORE_DIR/revoked/${key_name}.gpg.$(date +%Y%m%d-%H%M%S)"
    
    # Audit log
    echo "$(date -Iseconds)|DELETE|$key_name|$USER" >> $AUDIT_LOG
}
```

## 🗝️ Option 2: Linux Kernel Keyring

### Using keyctl for Session Keys
```bash
#!/bin/bash
# kernel-keystore.sh

# Add key to kernel keyring
add_key() {
    local key_name=$1
    local key_data=$2
    
    # Add to session keyring
    key_id=$(echo -n "$key_data" | keyctl padd user "$key_name" @s)
    echo "Key added with ID: $key_id"
    
    # Set timeout (optional - 1 hour)
    keyctl timeout $key_id 3600
}

# Get key from keyring
get_key() {
    local key_name=$1
    
    # Find key ID
    key_id=$(keyctl search @s user "$key_name")
    
    # Read key data
    keyctl pipe $key_id
}

# List keys
list_keys() {
    keyctl list @s
}

# Persist keyring to disk (encrypted)
save_keyring() {
    local backup_file=$1
    local passphrase=$2
    
    # Export all keys
    keyctl list @s | while read key; do
        key_id=$(echo $key | cut -d: -f1)
        keyctl pipe $key_id
    done | gpg --symmetric --passphrase "$passphrase" > "$backup_file"
}
```

## 🔐 Option 3: Pass (Unix Password Store)

### Setup Pass for Zone Keys
```bash
#!/bin/bash
# pass-keystore.sh

# Initialize pass with GPG key
pass init "Zone Head production"

# Store SSH CA key
pass insert -m zone/ssh-ca/private < /opt/zone/ca/zone-ca-key
pass insert -m zone/ssh-ca/public < /opt/zone/ca/zone-ca-key.pub

# Store node enrollment token
pass generate zone/nodes/node01/token 32

# Store with metadata
echo -e "$(cat key.pem)\n---\ncreated: $(date)\ntype: rsa-2048" | \
    pass insert -m zone/keys/api-key

# Retrieve key
pass show zone/ssh-ca/private

# List all keys
pass ls

# Git integration for history
pass git init
pass git remote add origin zone-head:/opt/zone/pass-repo
pass git push
```

## 🛡️ Option 4: Age (Modern Encryption Tool)

### Simple Age-Based Keystore
```bash
#!/bin/bash
# age-keystore.sh

KEYSTORE_DIR="/opt/zone/keystore"
AGE_KEY="$KEYSTORE_DIR/zone-head.key"

# Generate zone head key
age-keygen -o "$AGE_KEY"
RECIPIENT=$(age-keygen -y "$AGE_KEY")

# Store a secret
store_secret() {
    local name=$1
    local data=$2
    
    echo "$data" | age -r "$RECIPIENT" -o "$KEYSTORE_DIR/$name.age"
}

# Retrieve a secret
get_secret() {
    local name=$1
    
    age -d -i "$AGE_KEY" "$KEYSTORE_DIR/$name.age"
}

# Encrypt file
age -r "$RECIPIENT" -o secrets.tar.gz.age secrets.tar.gz

# Decrypt file
age -d -i "$AGE_KEY" secrets.tar.gz.age > secrets.tar.gz
```

## 📦 Option 5: systemd-creds (systemd 250+)

### Systemd Credential Storage
```bash
#!/bin/bash
# systemd-creds-keystore.sh

# Store credential
echo -n "secret-password" | systemd-creds encrypt - zone.password

# Store with PCR binding (TPM)
systemd-creds encrypt \
    --with-key=tpm2 \
    --tpm2-pcrs=7+14 \
    plaintext.txt \
    zone.credential

# Retrieve credential
systemd-creds decrypt zone.credential -

# List credentials
systemd-creds list

# Use in service
cat > /etc/systemd/system/zone-service.service <<EOF
[Service]
LoadCredentialEncrypted=password:zone.password
ExecStart=/opt/zone/bin/service.sh
Environment="PASSWORD=%d/password"
EOF
```

## 🚀 Complete Lightweight Zone Head System

### Directory Structure
```bash
zone-head/
├── keystore.sh          # Main keystore script
├── keystore/
│   ├── master.key       # GPG or Age master key
│   ├── keys/            # Encrypted keys
│   ├── certs/           # Certificates
│   ├── audit/           # Audit logs
│   └── backup/          # Encrypted backups
└── bin/
    ├── zone-ca.sh       # SSH CA operations
    ├── node-enroll.sh   # Node enrollment
    └── key-rotate.sh    # Key rotation
```

### Main Keystore Script
```bash
#!/bin/bash
# keystore.sh - Complete lightweight keystore

set -euo pipefail

KEYSTORE_DIR="${KEYSTORE_DIR:-/opt/zone/keystore}"
MASTER_KEY="${MASTER_KEY:-$KEYSTORE_DIR/master.key}"
LOG_FILE="$KEYSTORE_DIR/audit/audit.log"

# Initialize keystore
init() {
    mkdir -p "$KEYSTORE_DIR"/{keys,certs,audit,backup}
    chmod 700 "$KEYSTORE_DIR"
    
    # Generate master key (using Age for simplicity)
    if [[ ! -f "$MASTER_KEY" ]]; then
        age-keygen -o "$MASTER_KEY"
        chmod 600 "$MASTER_KEY"
        echo "Generated master key: $MASTER_KEY"
    fi
    
    # Initialize audit log
    touch "$LOG_FILE"
    chmod 600 "$LOG_FILE"
}

# Audit logging
audit() {
    echo "$(date -Iseconds)|$1|$2|${USER:-system}|${SSH_CLIENT:-local}" >> "$LOG_FILE"
}

# Store encrypted data
store() {
    local key_name=$1
    local key_type=${2:-secret}
    local input_file=${3:--}
    
    local key_path="$KEYSTORE_DIR/keys/${key_name}"
    local recipient=$(age-keygen -y "$MASTER_KEY")
    
    # Encrypt and store
    age -r "$recipient" -a -o "${key_path}.age" "$input_file"
    
    # Store metadata
    cat > "${key_path}.meta" <<-EOF
		type: $key_type
		created: $(date -Iseconds)
		creator: $USER
		checksum: $(sha256sum "$input_file" | cut -d' ' -f1)
	EOF
    
    audit "STORE" "$key_name"
    echo "Stored: $key_name"
}

# Retrieve decrypted data
get() {
    local key_name=$1
    local output_file=${2:--}
    
    local key_path="$KEYSTORE_DIR/keys/${key_name}.age"
    
    if [[ ! -f "$key_path" ]]; then
        echo "Error: Key not found: $key_name" >&2
        return 1
    fi
    
    # Decrypt
    if [[ "$output_file" == "-" ]]; then
        age -d -i "$MASTER_KEY" "$key_path"
    else
        age -d -i "$MASTER_KEY" "$key_path" > "$output_file"
    fi
    
    audit "RETRIEVE" "$key_name"
}

# List keys with metadata
list() {
    for key in "$KEYSTORE_DIR/keys"/*.age; do
        [[ -f "$key" ]] || continue
        
        basename "$key" .age
        if [[ -f "${key%.age}.meta" ]]; then
            cat "${key%.age}.meta" | sed 's/^/  /'
        fi
        echo
    done
}

# Rotate a key
rotate() {
    local key_name=$1
    local new_data_file=${2:--}
    
    # Backup old key
    local timestamp=$(date +%Y%m%d-%H%M%S)
    local key_path="$KEYSTORE_DIR/keys/${key_name}"
    
    if [[ -f "${key_path}.age" ]]; then
        mv "${key_path}.age" "$KEYSTORE_DIR/backup/${key_name}.${timestamp}.age"
        mv "${key_path}.meta" "$KEYSTORE_DIR/backup/${key_name}.${timestamp}.meta"
    fi
    
    # Store new key
    store "$key_name" "secret" "$new_data_file"
    
    audit "ROTATE" "$key_name"
    echo "Rotated: $key_name"
}

# Delete (revoke) a key
delete() {
    local key_name=$1
    
    local key_path="$KEYSTORE_DIR/keys/${key_name}"
    local timestamp=$(date +%Y%m%d-%H%M%S)
    
    # Move to backup with revoked marker
    if [[ -f "${key_path}.age" ]]; then
        mv "${key_path}.age" "$KEYSTORE_DIR/backup/${key_name}.revoked.${timestamp}.age"
        mv "${key_path}.meta" "$KEYSTORE_DIR/backup/${key_name}.revoked.${timestamp}.meta"
        
        audit "DELETE" "$key_name"
        echo "Deleted: $key_name"
    else
        echo "Error: Key not found: $key_name" >&2
        return 1
    fi
}

# Backup entire keystore
backup() {
    local backup_file=${1:-"keystore-backup-$(date +%Y%m%d-%H%M%S).tar.gz.age"}
    
    # Create tarball and encrypt
    tar -czf - -C "$KEYSTORE_DIR" keys certs | \
        age -r $(age-keygen -y "$MASTER_KEY") -a > "$backup_file"
    
    audit "BACKUP" "$backup_file"
    echo "Backup created: $backup_file"
}

# SSH CA operations
ssh_ca() {
    case ${1:-} in
        sign-user)
            local user_key=$2
            local principal=$3
            local validity=${4:-8h}
            
            # Sign with zone CA key
            get "zone-ca-key" | ssh-keygen -s /dev/stdin \
                -I "$principal-$(date +%s)" \
                -n "$principal" \
                -V "+$validity" \
                "$user_key"
            
            audit "SSH_SIGN_USER" "$principal"
            ;;
            
        sign-host)
            local host_key=$2
            local hostname=$3
            
            # Sign host key
            get "zone-ca-key" | ssh-keygen -s /dev/stdin \
                -I "$hostname" \
                -h \
                -n "$hostname" \
                -V "+365d" \
                "$host_key"
            
            audit "SSH_SIGN_HOST" "$hostname"
            ;;
    esac
}

# Main command dispatcher
case ${1:-} in
    init)    init ;;
    store)   shift; store "$@" ;;
    get)     shift; get "$@" ;;
    list)    list ;;
    rotate)  shift; rotate "$@" ;;
    delete)  shift; delete "$@" ;;
    backup)  shift; backup "$@" ;;
    ssh-ca)  shift; ssh_ca "$@" ;;
    *)
        cat <<-EOF
		Usage: $0 {init|store|get|list|rotate|delete|backup|ssh-ca} [args...]
		
		Commands:
		  init                    Initialize keystore
		  store <name> [type]     Store encrypted key/secret
		  get <name> [file]       Retrieve decrypted key/secret
		  list                    List all keys with metadata
		  rotate <name> [file]    Rotate a key
		  delete <name>           Delete (revoke) a key
		  backup [file]           Backup entire keystore
		  ssh-ca <cmd> [args]     SSH CA operations
		
		SSH CA Commands:
		  ssh-ca sign-user <key> <principal> [validity]
		  ssh-ca sign-host <key> <hostname>
		
		Environment Variables:
		  KEYSTORE_DIR    Keystore directory (default: /opt/zone/keystore)
		  MASTER_KEY      Master key file (default: \$KEYSTORE_DIR/master.key)
		EOF
        exit 1
        ;;
esac
```

## 🔒 Security Hardening

### Protect the Master Key
```bash
# Use hardware token (YubiKey)
gpg --card-edit

# Or use TPM 2.0
tpm2_nvdefine -C o -s 32 -a "ownerread|policywrite" 0x1500016
tpm2_nvwrite -C o 0x1500016 -i master.key

# Or use kernel keyring with timeout
keyctl add user zone_master "$(cat master.key)" @s
keyctl timeout $(keyctl search @s user zone_master) 3600
```

### Audit and Monitoring
```bash
# Watch audit log
tail -f $KEYSTORE_DIR/audit/audit.log

# Alert on suspicious activity
grep -E "DELETE|ROTATE" $KEYSTORE_DIR/audit/audit.log | \
    mail -s "Keystore Alert" admin@example.com

# Log rotation
cat > /etc/logrotate.d/keystore <<EOF
/opt/zone/keystore/audit/*.log {
    daily
    rotate 90
    compress
    notifempty
    create 600 root root
}
EOF
```

## ✅ Advantages of Lightweight Approach

1. **No dependencies** - Uses only standard Linux tools
2. **Simple** - Easy to understand and audit
3. **Portable** - Works on any Linux distribution
4. **Fast** - No network calls or complex protocols
5. **Offline capable** - No external services required
6. **Version control friendly** - Can use Git for encrypted keys
7. **Scriptable** - Easy to integrate with existing scripts

## ⚠️ Limitations

1. **No HA** - Single point of failure (unless replicated)
2. **Manual scaling** - Need to handle distribution yourself
3. **Basic access control** - Relies on filesystem permissions
4. **No dynamic secrets** - All secrets are static
5. **Limited audit** - Basic logging vs. enterprise audit trail

## 🎯 When to Use This Approach

**Good for:**
- Small to medium deployments
- Air-gapped environments
- Embedded systems
- Development/testing
- Quick prototypes
- When Vault is overkill

**Not ideal for:**
- Large scale (100+ nodes)
- Compliance requirements (PCI, HIPAA)
- Multi-team environments
- Need for dynamic secrets
- Complex access policies

This lightweight approach gives you 80% of the functionality with 20% of the complexity - perfect for many real-world scenarios!