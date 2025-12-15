#!/bin/bash
# Harden a new server for PQTR zone
# Security: SSH certificates, fail2ban, ufw, kernel hardening, audit logging
set -e
if [ $# -ne 3 ]; then
    echo "args: <host-ip> <host-name> <zone-pkey>"
    exit 1
fi

HOST=$1
NAME=$2
ZONE_PKEY=$(cat $3)
HOST_FILE=$(mktemp)

trap "rm -f $HOST_FILE" EXIT

echo "[info] connecting to root@$HOST..."
ssh -o "StrictHostKeyChecking=no" -o "UserKnownHostsFile=$HOST_FILE" -o "PubkeyAuthentication=no" root@$HOST "bash -s" << END_INIT

if [ -f "/etc/ssh/host.skey" ]; then
    echo "[warn] already hardened"
    exit 0
fi

echo "[info] setting hostname..."
echo "$NAME" > /etc/hostname
echo "127.0.0.1    $NAME" >> /etc/hosts

echo "[info] saving zone public key..."
echo "$ZONE_PKEY" > /etc/ssh/zone.pkey

echo "[info] updating packages..."
apt update
apt upgrade -y
apt install -y unattended-upgrades apt-listchanges fail2ban ufw wget git vim htop auditd

echo "[info] configuring kernel security..."
cat > /etc/sysctl.d/99-security.conf << 'SYSCTL'
# Network security
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0
net.ipv4.conf.all.log_martians = 1
net.ipv4.icmp_echo_ignore_broadcasts = 1
net.ipv4.icmp_ignore_bogus_error_responses = 1
net.ipv4.tcp_syncookies = 1

# IPv6 hardening
net.ipv6.conf.all.accept_redirects = 0
net.ipv6.conf.default.accept_redirects = 0
net.ipv6.conf.all.accept_source_route = 0
net.ipv6.conf.default.accept_source_route = 0

# Kernel hardening
kernel.randomize_va_space = 2
kernel.kptr_restrict = 2
kernel.dmesg_restrict = 1
kernel.perf_event_paranoid = 3
kernel.yama.ptrace_scope = 1
fs.protected_hardlinks = 1
fs.protected_symlinks = 1
fs.suid_dumpable = 0
SYSCTL
sysctl --system

echo "[info] configuring firewall..."
ufw default deny incoming
ufw default allow outgoing
ufw limit ssh  # Rate limit SSH connections
ufw --force enable

echo "[info] generating host SSH keys..."
ssh-keygen -q -t ed25519 -f /etc/ssh/host.skey -C "$NAME" -P ""
ssh-keygen -h -s /etc/ssh/host.skey -I $NAME /etc/ssh/host.skey.pub
mv /etc/ssh/host.skey-cert.pub /etc/ssh/host.cert

echo "[info] configuring SSH server..."
cat > /etc/ssh/sshd_config << 'SSHD_CONFIG'
# Keys and certificates
HostKey /etc/ssh/host.skey
HostCertificate /etc/ssh/host.cert
TrustedUserCAKeys /etc/ssh/zone.pkey

# Authentication
PermitRootLogin no
PasswordAuthentication no
PermitEmptyPasswords no
PubkeyAuthentication yes
AuthenticationMethods publickey
MaxAuthTries 3
LoginGraceTime 20

# Session security
ClientAliveInterval 300
ClientAliveCountMax 2
MaxSessions 3

# Disable forwarding (enable per-user if needed)
AllowAgentForwarding no
AllowTcpForwarding no
X11Forwarding no
PermitTunnel no

# Other
PrintMotd no
UsePAM yes
Subsystem sftp /usr/lib/openssh/sftp-server

# Allowed users
AllowUsers env ops
SSHD_CONFIG

sed -i 's/\(session.*optional.*pam_motd.so.*\)/#\1/' /etc/pam.d/sshd

echo "[info] configuring fail2ban for SSH..."
cat > /etc/fail2ban/jail.d/ssh.conf << 'F2B_SSH'
[sshd]
enabled = true
port = ssh
filter = sshd
logpath = /var/log/auth.log
maxretry = 3
findtime = 300
bantime = 3600
bantime.increment = true
bantime.factor = 2
bantime.maxtime = 86400
F2B_SSH
systemctl enable fail2ban
systemctl restart fail2ban

echo "[info] configuring audit logging..."
cat > /etc/audit/rules.d/pqtr.rules << 'AUDIT_RULES'
# Delete all existing rules
-D

# Buffer size
-b 8192

# Failure mode (1=printk, 2=panic)
-f 1

# Monitor sudo usage
-w /etc/sudoers -p wa -k sudoers
-w /etc/sudoers.d/ -p wa -k sudoers

# Monitor SSH config
-w /etc/ssh/sshd_config -p wa -k sshd_config

# Monitor user/group changes
-w /etc/passwd -p wa -k identity
-w /etc/group -p wa -k identity
-w /etc/shadow -p wa -k identity

# Monitor login files
-w /var/log/lastlog -p wa -k logins
-w /var/log/faillog -p wa -k logins

# Monitor cron
-w /etc/crontab -p wa -k cron
-w /etc/cron.d/ -p wa -k cron

# Make config immutable (reboot to change)
-e 2
AUDIT_RULES
systemctl enable auditd
systemctl restart auditd

echo "[info] creating service accounts..."

# ops - console operations
useradd --create-home --shell /bin/bash ops
mkdir -p /home/ops/.ssh /mnt/ops
chmod 700 /home/ops/.ssh
chown -R ops:ops /home/ops /mnt/ops

# svc - web services (no SSH access)
useradd --create-home --shell /bin/bash svc
mkdir -p /home/svc/.ssh /mnt/svc
chmod 700 /home/svc/.ssh
chown -R svc:svc /home/svc /mnt/svc

# sdm - secure data (no SSH access)
useradd --create-home --shell /bin/bash sdm
mkdir -p /home/sdm/.ssh /mnt/sdm
chmod 700 /home/sdm/.ssh
chown -R sdm:sdm /home/sdm /mnt/sdm

# env - sudo access
useradd --create-home --shell /bin/bash --user-group --groups sudo,ops,svc,sdm env
mkdir -p /home/env/.ssh
chmod 700 /home/env/.ssh
chown -R env:env /home/env

echo "env ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

echo "[info] setting secure umask..."
echo "umask 027" >> /etc/profile.d/umask.sh

echo "[info] disabling unused services..."
systemctl disable --now cups 2>/dev/null || true
systemctl disable --now avahi-daemon 2>/dev/null || true
systemctl disable --now bluetooth 2>/dev/null || true

echo "[info] rebooting..."
reboot -h 0

END_INIT

echo "[done] $NAME hardened, rebooting"
