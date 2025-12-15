#!/bin/bash
# Create user SSH keypair for zone access
set -e
if [ $# -ne 2 ]; then
    echo "args: <user> <zone-name>"
    exit 1
fi
USER=$1
ZONE=$2
ssh-keygen -q -t ed25519 -f ~/.ssh/$USER-$ZONE.skey -C "$USER@$ZONE"
chmod 600 ~/.ssh/$USER-$ZONE.skey*
echo "[done] user keys: ~/.ssh/$USER-$ZONE.skey{,.pub}"
