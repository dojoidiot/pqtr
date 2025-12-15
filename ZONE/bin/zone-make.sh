#!/bin/bash
# Create zone SSH CA keypair
set -e
if [ $# -ne 1 ]; then
    echo "args: <zone-name>"
    exit 1
fi
ZONE=$1
ssh-keygen -q -t ed25519 -f ~/.ssh/$ZONE.skey -C "$ZONE zone"
chmod 600 ~/.ssh/$ZONE.skey*
echo "[done] zone keys: ~/.ssh/$ZONE.skey{,.pub}"
