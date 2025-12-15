#!/bin/bash
# Sign hardened node with zone CA for trusted membership
set -e
if [ $# -ne 2 ]; then
    echo "args: <host> <zone-skey>"
    exit 1
fi

HOST=$1
ZONE_SKEY=$2
HOST_FILE=$(mktemp)
HOST_PKEY=$(mktemp)
HOST_CERT=$(mktemp)

trap "rm -f $HOST_FILE $HOST_PKEY $HOST_CERT ${HOST_PKEY}-cert.pub" EXIT

echo "[info] fetching host public key..."
ssh-keyscan -t ssh-ed25519 $HOST 2>/dev/null | awk '{print $2 " " $3}' > $HOST_PKEY

echo "[info] signing with zone CA..."
ssh-keygen -h -s $ZONE_SKEY -I $HOST $HOST_PKEY
mv ${HOST_PKEY}-cert.pub $HOST_CERT

echo "[info] installing certificate..."
scp -o "StrictHostKeyChecking=no" -o "UserKnownHostsFile=$HOST_FILE" $HOST_CERT env@$HOST:/home/env/host.cert
ssh -o "UserKnownHostsFile=$HOST_FILE" env@$HOST "bash -s" << END_SSH
sudo mv /home/env/host.cert /etc/ssh/host.cert
sudo systemctl restart sshd
END_SSH

echo "[done] $HOST signed and certificate installed"
