#!/bin/bash
# ops-security.sh - Security audit report (requires sudo)
# Shows fail2ban status, recent auth failures, firewall state
set -e

BOLD='\033[1m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

section() { echo -e "\n${BOLD}=== $1 ===${NC}"; }

if [ "$EUID" -ne 0 ]; then
    echo "Run with: sudo $0"
    exit 1
fi

section "FAIL2BAN"
if systemctl is-active --quiet fail2ban; then
    echo "Status: active"
    echo ""
    echo "Jails:"
    fail2ban-client status 2>/dev/null | grep "Jail list" | sed 's/.*://;s/,/\n/g' | while read jail; do
        jail=$(echo $jail | xargs)
        [ -z "$jail" ] && continue
        banned=$(fail2ban-client status $jail 2>/dev/null | grep "Currently banned" | awk '{print $NF}')
        total=$(fail2ban-client status $jail 2>/dev/null | grep "Total banned" | awk '{print $NF}')
        if [ "$banned" -gt 0 ] 2>/dev/null; then
            echo -e "  ${RED}$jail${NC}: $banned banned (total: $total)"
        else
            echo -e "  ${GREEN}$jail${NC}: 0 banned (total: $total)"
        fi
    done

    echo ""
    echo "Currently banned IPs:"
    fail2ban-client status 2>/dev/null | grep "Jail list" | sed 's/.*://;s/,/\n/g' | while read jail; do
        jail=$(echo $jail | xargs)
        [ -z "$jail" ] && continue
        ips=$(fail2ban-client status $jail 2>/dev/null | grep "Banned IP" | sed 's/.*://')
        [ -n "$ips" ] && echo "  $jail: $ips"
    done
else
    echo -e "${RED}fail2ban not running${NC}"
fi

section "FIREWALL (UFW)"
if command -v ufw &>/dev/null; then
    ufw status numbered 2>/dev/null | head -20
else
    echo "ufw not installed"
fi

section "AUTH FAILURES (last 24h)"
echo "SSH failures:"
grep "Failed password\|Invalid user" /var/log/auth.log 2>/dev/null | \
    grep "$(date +%b\ %d)\|$(date -d yesterday +%b\ %d)" | \
    awk '{print $1, $2, $3, $9, $11}' | sort | uniq -c | sort -rn | head -10 || echo "  (none)"

echo ""
echo "Blocked by fail2ban:"
grep "Ban " /var/log/fail2ban.log 2>/dev/null | \
    grep "$(date +%Y-%m-%d)\|$(date -d yesterday +%Y-%m-%d)" | \
    awk '{print $NF}' | sort | uniq -c | sort -rn | head -10 || echo "  (none)"

section "AUDIT LOG (recent)"
if command -v ausearch &>/dev/null; then
    echo "Sudo usage (last 10):"
    ausearch -k sudoers -ts today 2>/dev/null | grep "comm=" | tail -10 | \
        sed 's/.*comm="\([^"]*\)".*/  \1/' || echo "  (none today)"

    echo ""
    echo "SSH config changes:"
    ausearch -k sshd_config -ts today 2>/dev/null | grep "name=" | \
        awk -F'name="' '{print "  " $2}' | cut -d'"' -f1 | head -5 || echo "  (none today)"
else
    echo "auditd not installed"
fi

section "CERTIFICATES"
echo "Let's Encrypt:"
if [ -d /etc/letsencrypt/live ]; then
    for cert in /etc/letsencrypt/live/*/fullchain.pem; do
        [ -f "$cert" ] || continue
        domain=$(dirname $cert | xargs basename)
        expiry=$(openssl x509 -enddate -noout -in "$cert" 2>/dev/null | cut -d= -f2)
        days=$(( ($(date -d "$expiry" +%s) - $(date +%s)) / 86400 ))
        if [ $days -lt 14 ]; then
            echo -e "  ${RED}$domain${NC}: expires in $days days"
        elif [ $days -lt 30 ]; then
            echo -e "  ${YELLOW}$domain${NC}: expires in $days days"
        else
            echo -e "  ${GREEN}$domain${NC}: expires in $days days"
        fi
    done
else
    echo "  No certificates found"
fi

echo ""
echo "SSH host certificate:"
if [ -f /etc/ssh/host.cert ]; then
    ssh-keygen -L -f /etc/ssh/host.cert 2>/dev/null | grep -E "(Valid|Principals)" | sed 's/^/  /'
else
    echo "  No host certificate"
fi

section "SYSTEM UPDATES"
if command -v apt &>/dev/null; then
    updates=$(apt list --upgradable 2>/dev/null | grep -c upgradable || echo 0)
    security=$(apt list --upgradable 2>/dev/null | grep -i security | wc -l || echo 0)
    if [ "$security" -gt 0 ]; then
        echo -e "${RED}$security security updates available${NC}"
    fi
    echo "$updates packages can be upgraded"

    echo ""
    echo "Last update: $(stat -c %y /var/lib/apt/lists 2>/dev/null | cut -d' ' -f1 || echo 'unknown')"
fi

echo ""
