#!/bin/bash
# zone-init.sh - Promote a hardened node to zone head
# Run this from your local desktop after node-make.sh

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "Usage: $0 <zone-head-ip> <zone-name>"
    echo "Example: $0 192.168.1.10 production"
    exit 1
fi

ZONE_HEAD_IP=$1
ZONE_NAME=$2
HERE=$(cd "$(dirname "$0")"; pwd -P)
ZONE_KEYS="$HERE/../etc/ssh"

# Verify we have the zone keys locally
if [[ ! -f "$ZONE_KEYS/$ZONE_NAME.skey" ]]; then
    echo "Error: Zone keys not found. Run zone-make.sh first."
    exit 1
fi

# Verify we have env user credentials
if [[ ! -f "$ZONE_KEYS/env-$ZONE_NAME.skey" ]]; then
    echo "Error: env user keys not found. Run user-make.sh and user-sign.sh first."
    exit 1
fi

echo "=== Promoting $ZONE_HEAD_IP to zone head for $ZONE_NAME ==="

# 1. Transfer zone keys securely to zone head
echo "→ Transferring zone authority..."
ssh -i "$ZONE_KEYS/env-$ZONE_NAME.skey" env@$ZONE_HEAD_IP "
    # Create zone head directories
    sudo mkdir -p /opt/zone/{safe,bin,config,nodes}
    sudo chown env:env /opt/zone -R
    chmod 700 /opt/zone/safe
"

# 2. Copy zone keys using secure channel
echo "→ Installing zone keys..."
scp -i "$ZONE_KEYS/env-$ZONE_NAME.skey" \
    "$ZONE_KEYS/$ZONE_NAME.skey" \
    "$ZONE_KEYS/$ZONE_NAME.skey.pub" \
    env@$ZONE_HEAD_IP:/opt/zone/safe/


# 3. Install safe script
echo "→ Installing safe..."
cat << 'SAFE_SCRIPT' | ssh -i "$ZONE_KEYS/env-$ZONE_NAME.skey" env@$ZONE_HEAD_IP "cat > /opt/zone/bin/safe.sh && chmod +x /opt/zone/bin/safe.sh"
#!/bin/bash
# Simple safe for zone head
SAFE_DIR="/opt/zone/safe"
MASTER_PASS_FILE="/opt/zone/.master"

init() {
    openssl rand -base64 32 > "$MASTER_PASS_FILE"
    chmod 600 "$MASTER_PASS_FILE"
    echo "Safe initialized"
}

save() {
    local name=$1
    local file=${2:--}
    openssl enc -aes-256-cbc -salt -pbkdf2 \
        -in "$file" -out "$SAFE_DIR/$name.enc" \
        -pass file:"$MASTER_PASS_FILE"
    echo "Stored: $name"
}

load() {
    local name=$1
    openssl enc -aes-256-cbc -d -pbkdf2 \
        -in "$SAFE_DIR/$name.enc" \
        -pass file:"$MASTER_PASS_FILE"
}

case ${1:-} in
    init)  init ;;
    save)  save "$2" "${3:--}" ;;
    load)  load "$2" ;;
    *)     echo "Usage: $0 {init|save|load}" ;;
esac
SAFE_SCRIPT

# 4. Initialize safe and store zone keys
echo "→ Securing zone keys..."
ssh -i "$ZONE_KEYS/env-$ZONE_NAME.skey" env@$ZONE_HEAD_IP "
    # Initialize safe
    /opt/zone/bin/safe.sh init
    
    # Store zone CA key encrypted
    /opt/zone/bin/safe.sh save zone-ca-private /opt/zone/safe/$ZONE_NAME.skey
    /opt/zone/bin/safe.sh save zone-ca-public /opt/zone/safe/$ZONE_NAME.skey.pub
    
    # Remove unencrypted keys
    shred -u /opt/zone/safe/$ZONE_NAME.skey
    rm /opt/zone/safe/$ZONE_NAME.skey.pub
    
    # Create zone config
    cat > /opt/zone/config/zone.conf << EOF
ZONE_NAME=$ZONE_NAME
ZONE_HEAD=$ZONE_HEAD_IP
ZONE_CREATED=$(date -Iseconds)
EOF
"

# 5. Install zone management scripts
echo "→ Installing zone management tools..."
cat << 'ZONE_BOSS' | ssh -i "$ZONE_KEYS/env-$ZONE_NAME.skey" env@$ZONE_HEAD_IP "cat > /opt/zone/bin/zone-boss.sh && chmod +x /opt/zone/bin/zone-boss.sh"
#!/bin/bash
# zone-boss.sh - Zone head management operations

ZONE_DIR="/opt/zone"
source "$ZONE_DIR/config/zone.conf"

make() {
    local node_ip=$1
    local node_name=$2
    
    echo "Making node $node_name ($node_ip) in zone $ZONE_NAME"
    
    # Store node info
    echo "$node_ip" > "$ZONE_DIR/nodes/$node_name"
    
    echo "Node registered: $node_name"
    echo ""
    echo "To harden this node, run from your desktop:"
    echo "  cd host/bin"
    echo "  ./node-make.sh $node_ip $node_name etc/ssh/$ZONE_NAME.skey.pub"
}

sign() {
    local type=$1
    shift
    
    # Get zone CA key from safe
    TMPKEY=$(mktemp)
    /opt/zone/bin/safe.sh load zone-ca-private > $TMPKEY
    
    case $type in
        user)
            local user=$1
            local role=$2
            local pubkey=$3
            ssh-keygen -s $TMPKEY -n "$role" -I "$user-$role" "$pubkey"
            echo "Signed user certificate: ${pubkey}-cert.pub"
            ;;
        host)
            local hostname=$1
            local host_pubkey=$2
            ssh-keygen -s $TMPKEY -h -n "$hostname" -I "$hostname" "$host_pubkey"
            echo "Signed host certificate: ${host_pubkey}-cert.pub"
            ;;
        *)
            echo "Usage: sign {user|host} <args>"
            ;;
    esac
    
    shred -u $TMPKEY
}

list() {
    echo "Nodes in zone $ZONE_NAME:"
    for node in $ZONE_DIR/nodes/*; do
        [[ -f "$node" ]] || continue
        echo "  - $(basename $node): $(cat $node)"
    done
}

drop() {
    local node_name=$1
    
    if [[ ! -f "$ZONE_DIR/nodes/$node_name" ]]; then
        echo "Error: Node $node_name not found"
        return 1
    fi
    
    # Move to revoked directory with timestamp
    mkdir -p "$ZONE_DIR/revoked"
    timestamp=$(date +%Y%m%d-%H%M%S)
    mv "$ZONE_DIR/nodes/$node_name" "$ZONE_DIR/revoked/$node_name.$timestamp"
    
    echo "Node $node_name dropped at $timestamp"
    echo ""
    echo "To complete lockout, update all remaining nodes:"
    echo "  1. Remove $node_name from known_hosts"
    echo "  2. Update firewall rules to block $(cat "$ZONE_DIR/revoked/$node_name.$timestamp")"
    echo "  3. Revoke any certificates issued to $node_name"
}

case ${1:-} in
    make)  make "$2" "$3" ;;
    sign)  sign "$@" ;;
    list)  list ;;
    drop)  drop "$2" ;;
    *)
        echo "Usage: $0 {make|sign|list|drop}"
        echo ""
        echo "Commands:"
        echo "  make <ip> <name>                     Add node to zone"
        echo "  sign user <user> <role> <key>        Sign user SSH key"  
        echo "  sign host <hostname> <key>           Sign host SSH key"
        echo "  list                                 List zone nodes"
        echo "  drop <name>                          Revoke node from zone"
        ;;
esac
ZONE_BOSS

# 6. Setup cron for maintenance tasks
echo "→ Setting up maintenance tasks..."
ssh -i "$ZONE_KEYS/env-$ZONE_NAME.skey" env@$ZONE_HEAD_IP "
    # Add cron job for certificate expiry checks
    (crontab -l 2>/dev/null || true; echo '0 0 * * * /opt/zone/bin/check-certs.sh') | crontab -
"

# 7. Verify zone head is operational
echo "→ Verifying zone head..."
ssh -i "$ZONE_KEYS/env-$ZONE_NAME.skey" env@$ZONE_HEAD_IP "
    /opt/zone/bin/zone-boss.sh list
    echo ''
    echo 'Zone head status: ACTIVE'
    echo 'Zone name: $ZONE_NAME'
    echo 'Zone head: $ZONE_HEAD_IP'
"

echo ""
echo "=== Zone head initialized successfully! ==="
echo ""
echo "Zone head is now operational at $ZONE_HEAD_IP"
echo "You can now:"
echo "  1. Add nodes:    ssh env@$ZONE_HEAD_IP '/opt/zone/bin/zone-boss.sh make <ip> <name>'"
echo "  2. Sign users:   ssh env@$ZONE_HEAD_IP '/opt/zone/bin/zone-boss.sh sign user <user> <role> <keyfile>'"
echo "  3. Sign hosts:   ssh env@$ZONE_HEAD_IP '/opt/zone/bin/zone-boss.sh sign host <hostname> <keyfile>'"
echo ""
echo "IMPORTANT: Securely DELETE the local zone keys from $ZONE_KEYS/ now that they're on the zone head!"