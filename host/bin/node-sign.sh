# Sign a hard node as a member of the zone using the zone PRIVATE key so that users can trust it.  
# Has to be run with a key with the env role.
#!/bin/bash
if [ $# -ne 2 ]; then
    echo "args: <host> <zone-skey> "
    exit 1
fi
set -e
HOST=$1 
ZONE_SKEY=$2
HOST_FILE=.host_area
HOST_PKEY=.host-pkey.pub
HOST_CERT=.host-pkey-cert.pub

rm -f .host*

ssh-keyscan -t ssh-ed25519 $HOST 2>/dev/null | awk '{print $2 " " $3}' > $HOST_PKEY
ssh-keygen -h -s $ZONE_SKEY -I $HOST $HOST_PKEY
scp -o "StrictHostKeyChecking=no" -o "UserKnownHostsFile=$HOST_FILE" $HOST_CERT env@$HOST:/home/env/host.cert
ssh -o "UserKnownHostsFile=$HOST_FILE" env@$HOST "bash -s" << END_SSH
    sudo mv /home/env/host.cert /etc/ssh/host.cert
    sudo systemctl restart sshd
END_SSH

rm -f .host*

echo "sign host done."