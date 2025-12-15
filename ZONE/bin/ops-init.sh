#!/bin/bash
# ops-init.sh - Deploy ops tools to a hardened node
# Run from desktop: ./bin/ops-init.sh <hostname>
set -e
if [ $# -ne 1 ]; then
    echo "args: <hostname>"
    exit 1
fi

HOST=$1
HERE=$(cd "$(dirname "$0")" && pwd -P)

echo "[info] deploying ops tools to $HOST..."

# Copy scripts
scp "$HERE/ops-status.sh" "$HERE/ops-security.sh" "$HERE/ops-logs.sh" env@$HOST:/home/env/

# Install and configure
ssh env@$HOST "bash -s" << 'END_SSH'
set -e

echo "[info] installing to /usr/local/bin..."
sudo mv /home/env/ops-status.sh /usr/local/bin/ops-status
sudo mv /home/env/ops-security.sh /usr/local/bin/ops-security
sudo mv /home/env/ops-logs.sh /usr/local/bin/ops-logs
sudo chmod 755 /usr/local/bin/ops-*

echo "[info] configuring sudoers for ops..."
sudo tee /etc/sudoers.d/ops > /dev/null << 'SUDOERS'
# Ops account - limited sudo for monitoring
# Security report
ops ALL=(ALL) NOPASSWD: /usr/local/bin/ops-security

# Log viewing (read-only)
ops ALL=(ALL) NOPASSWD: /usr/bin/tail -* /var/log/auth.log
ops ALL=(ALL) NOPASSWD: /usr/bin/tail -* /var/log/nginx/*
ops ALL=(ALL) NOPASSWD: /usr/bin/tail -* /var/log/fail2ban.log
ops ALL=(ALL) NOPASSWD: /usr/bin/tail -* /var/log/audit/audit.log
ops ALL=(ALL) NOPASSWD: /usr/sbin/ausearch *

# Service status (read-only)
ops ALL=(ALL) NOPASSWD: /usr/bin/systemctl status *
ops ALL=(ALL) NOPASSWD: /usr/bin/fail2ban-client status *
ops ALL=(ALL) NOPASSWD: /usr/sbin/ufw status *

# No other sudo
SUDOERS
sudo chmod 440 /etc/sudoers.d/ops

echo "[info] creating ops aliases..."
sudo tee /home/ops/.bash_aliases > /dev/null << 'ALIASES'
# Ops shortcuts
alias status='ops-status'
alias security='sudo ops-security'
alias logs='ops-logs'
alias svc-status='sudo systemctl status'
alias svc-logs='journalctl -u'

# Quick checks
alias bans='sudo fail2ban-client status'
alias conns='ss -tn state established'
alias ports='ss -tlnp'
alias load='uptime; free -h'
ALIASES
sudo chown ops:ops /home/ops/.bash_aliases

echo "[done] ops tools installed"
echo ""
echo "SSH as ops and run:"
echo "  status      - system overview"
echo "  security    - security audit"
echo "  logs nginx  - view nginx logs"
echo "  bans        - fail2ban status"
END_SSH

echo "[done] ops tools deployed to $HOST"
