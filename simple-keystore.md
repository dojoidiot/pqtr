# Simple Script-Based SSH Keystore

## The Practical Solution

You already have a good zone-based SSH system in `host/bin/`. Here's a simple keystore to complement it:

```bash
#!/bin/bash
# keystore.sh - Dead simple encrypted key storage

KEYSTORE_DIR="/opt/zone/keystore"
MASTER_PASS_FILE="/opt/zone/.master"  # Store this securely!

# Initialize (run once)
init() {
    mkdir -p "$KEYSTORE_DIR"
    chmod 700 "$KEYSTORE_DIR"
    
    # Generate a strong master password
    openssl rand -base64 32 > "$MASTER_PASS_FILE"
    chmod 600 "$MASTER_PASS_FILE"
    
    echo "Keystore initialized at $KEYSTORE_DIR"
}

# Store a key (encrypted with OpenSSL)
store() {
    local name=$1
    local file=${2:--}  # stdin if not specified
    
    openssl enc -aes-256-cbc -salt -pbkdf2 \
        -in "$file" \
        -out "$KEYSTORE_DIR/$name.enc" \
        -pass file:"$MASTER_PASS_FILE"
    
    echo "Stored: $name"
}

# Get a key (decrypt)
get() {
    local name=$1
    
    openssl enc -aes-256-cbc -d -pbkdf2 \
        -in "$KEYSTORE_DIR/$name.enc" \
        -pass file:"$MASTER_PASS_FILE"
}

# List stored keys
list() {
    ls -1 "$KEYSTORE_DIR"/*.enc 2>/dev/null | \
        xargs -n1 basename | \
        sed 's/.enc$//'
}

# Main
case ${1:-} in
    init)  init ;;
    store) store "$2" "$3" ;;
    get)   get "$2" ;;
    list)  list ;;
    *)     echo "Usage: $0 {init|store|get|list}" ;;
esac
```

## That's It!

**Usage:**
```bash
# One-time setup
./keystore.sh init

# Store your zone CA key
./keystore.sh store zone-ca-key /path/to/zone.skey

# Retrieve it when needed
./keystore.sh get zone-ca-key > /tmp/zone.skey
ssh-keygen -s /tmp/zone.skey ...
rm /tmp/zone.skey

# List what's stored
./keystore.sh list
```

## If You Want Slightly More Features

Add these only if actually needed:

### 1. Audit Logging (if required)
```bash
# Add to each function:
echo "$(date -Iseconds)|$1|$name|$USER" >> "$KEYSTORE_DIR/audit.log"
```

### 2. Key Rotation (if required)
```bash
rotate() {
    local name=$1
    mv "$KEYSTORE_DIR/$name.enc" "$KEYSTORE_DIR/$name.enc.old"
    echo "Rotated. Now run: $0 store $name newfile"
}
```

### 3. GPG Instead of OpenSSL (if you prefer)
```bash
# Replace openssl commands with:
store: gpg --symmetric --cipher-algo AES256 --output "$KEYSTORE_DIR/$name.gpg"
get:   gpg --decrypt "$KEYSTORE_DIR/$name.gpg"
```

## What About the Master Password?

Keep it simple:
1. **Option A**: Store in `/opt/zone/.master` with 600 permissions (good enough for most)
2. **Option B**: Use environment variable `export KEYSTORE_PASS="..."`
3. **Option C**: Read from USB key that you insert when needed

## Integration With Your Existing Scripts

Modify your existing `host/bin/user-sign.sh`:
```bash
# Before:
ssh-keygen -s $ZONE_SKEY -n $ROLE_NAME -I $ROLE_NAME-role $USER_PKEY

# After:
ZONE_SKEY_TEMP=$(mktemp)
/opt/zone/keystore.sh get zone-ca-key > $ZONE_SKEY_TEMP
ssh-keygen -s $ZONE_SKEY_TEMP -n $ROLE_NAME -I $ROLE_NAME-role $USER_PKEY
rm $ZONE_SKEY_TEMP
```

## Why This Is Better

1. **50 lines of code** instead of 500
2. **Uses only OpenSSL** (installed everywhere)
3. **No external dependencies**
4. **Easy to understand and audit**
5. **Works today** without any setup

## When You Actually Need More

Only add complexity when you hit real problems:
- **Multiple admins?** Then add user-specific keys
- **100+ nodes?** Then consider Vault
- **Compliance requirements?** Then add HSM
- **High availability needed?** Then replicate the keystore
- **Dynamic secrets?** Then you need Vault

But for most zone head deployments? The simple script above is enough.