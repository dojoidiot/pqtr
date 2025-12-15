#!/bin/bash
# ops-status.sh - System status report for ops account
# Runs without sudo, shows key system health indicators
set -e

BOLD='\033[1m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

section() { echo -e "\n${BOLD}=== $1 ===${NC}"; }

section "SYSTEM"
echo "Hostname: $(hostname)"
echo "Uptime:   $(uptime -p)"
echo "Load:     $(cat /proc/loadavg | cut -d' ' -f1-3)"
echo "Kernel:   $(uname -r)"

section "MEMORY"
free -h | grep -E "^(Mem|Swap):"

section "DISK"
df -h / /home /mnt 2>/dev/null | grep -v "^Filesystem" | awk '{print $6 ": " $3 "/" $2 " (" $5 " used)"}'

section "SERVICES"
for svc in sshd nginx fail2ban auditd; do
    if systemctl is-active --quiet $svc 2>/dev/null; then
        echo -e "  ${GREEN}●${NC} $svc"
    elif systemctl is-enabled --quiet $svc 2>/dev/null; then
        echo -e "  ${RED}○${NC} $svc (stopped)"
    else
        echo -e "  ${YELLOW}-${NC} $svc (not installed)"
    fi
done

section "NETWORK"
echo "Listening ports:"
ss -tlnp 2>/dev/null | grep LISTEN | awk '{print "  " $4}' | sort -u || echo "  (need ss)"

echo ""
echo "Connections:"
ss -tn state established 2>/dev/null | grep -v "Recv-Q" | wc -l | xargs -I{} echo "  {} established connections"

section "USERS"
echo "Logged in:"
who | awk '{print "  " $1 " from " $5 " since " $3 " " $4}' || echo "  (none)"

echo ""
echo "Recent logins:"
last -n 5 -w 2>/dev/null | head -5 | awk '{print "  " $1 " " $3 " " $4 " " $5 " " $6}' || echo "  (unavailable)"

section "SECURITY (sudo required for details)"
echo "Run: sudo ops-security"
echo ""
