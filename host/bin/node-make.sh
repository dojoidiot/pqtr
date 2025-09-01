# Harden a node using the zone PUBLIC key.

#!/bin/bash
if [ $# -ne 3 ]; then
    echo "args: <host> <host-name> <zone-pkey> "
    exit 1
fi
set -e

HOST=$1
NAME=$2
ZONE_PKEY=$3
HOST_FILE=.host_area
ZONE_PKEY=$(cat $ZONE_PKEY)

rm -f $HOST_FILE

ssh -o "StrictHostKeyChecking=no"  -o "UserKnownHostsFile=$HOST_FILE" -o "PubkeyAuthentication=no" root@$HOST "bash -s" << END_INIT

if [ -f "/etc/ssh/host.skey" ] ; then
    echo "[warn] already hard."
    exit 0
fi

echo "$NAME" > /etc/hostname
echo "127.0.0.1		$NAME" >> /etc/hosts
echo "$ZONE_PKEY" > /etc/ssh/zone.pkey

apt -y update
apt -y upgrade
apt -y install unattended-upgrades sudo apt-listchanges fail2ban ufw wget git vim software-properties-common gpg htop

ufw allow ssh
ufw --force enable

# Setup SSH using edwards curves, self signed
ssh-keygen -q -t ed25519 -f /etc/ssh/host.skey -C "$NAME" -P ""  
ssh-keygen -h -s /etc/ssh/host.skey -I $NAME /etc/ssh/host.skey.pub
mv /etc/ssh/host.skey-cert.pub  /etc/ssh/host.cert 

# Setup SSH Server. We only specify default overrides.
echo "
HostKey             /etc/ssh/host.skey
HostCertificate     /etc/ssh/host.cert
TrustedUserCAKeys   /etc/ssh/zone.pkey
PermitEmptyPasswords no
PrintMotd no
PasswordAuthentication no
PermitAreaLogin no
UsePAM yes
Subsystem sftp /usr/lib/openssh/sftp-server
AllowUsers env ops
" > /etc/ssh/sshd_config

# Remove message of the day at login
sed -i 's/\(session.*optional.*pam_motd.so.*\)/#\1/'  /etc/pam.d/sshd

# Create the ops account for console management of the host.
useradd --create-home --shell /bin/bash ops
mkdir /home/ops/.ssh
touch /home/ops/.ssh/authorized_keys
touch /home/ops/.ssh/config
chown -R ops:ops /home/ops
chmod 700 /home/ops/.ssh
mkdir -p /mnt/ops
chown -R ops:ops /mnt/ops

# Create the svc account for www facing services.
useradd --create-home --shell /bin/bash svc
mkdir /home/svc/.ssh
touch /home/svc/.ssh/authorized_keys
touch /home/svc/.ssh/config
chown -R svc:svc /home/svc
chmod 700 /home/svc/.ssh
mkdir -p /mnt/svc
chown -R svc:svc /mnt/svc

# Create the sdm account for secure data management services.
useradd --create-home --shell /bin/bash sdm
mkdir /home/sdm/.ssh
touch /home/sdm/.ssh/authorized_keys
touch /home/sdm/.ssh/config
chown -R sdm:sdm /home/sdm
chmod 700 /home/sdm/.ssh
mkdir -p /mnt/sdm
chown -R sdm:sdm /mnt/sdm

# Create the env account that has sudo access for host environment management and group access to all accounts.
useradd --create-home --shell /bin/bash --user-group --groups sudo,ops,svc,sdm env
mkdir /home/env/.ssh
touch /home/env/.ssh/authorized_keys
touch /home/env/.ssh/config
chown -R env:env /home/env
chmod 700 /home/env/.ssh

echo "env ALL=(ALL) NOPASSWD:ALL"  >> /etc/sudoers

sudo reboot -h 0

END_INIT

rm -f $HOST_FILE

echo "[info] host is hard"