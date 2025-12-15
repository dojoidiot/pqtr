#!/bin/bash
# ops-logs.sh - View system logs (requires sudo for some)
# Usage: ops-logs [auth|nginx|fail2ban|audit|svc|all] [-f]
set -e

LOG=${1:-all}
FOLLOW=""
[ "$2" = "-f" ] && FOLLOW="-f"

case $LOG in
    auth)
        echo "=== Auth Log ==="
        sudo tail -50 $FOLLOW /var/log/auth.log
        ;;
    nginx)
        echo "=== Nginx Access ==="
        sudo tail -30 $FOLLOW /var/log/nginx/access.log
        echo ""
        echo "=== Nginx Errors ==="
        sudo tail -20 $FOLLOW /var/log/nginx/error.log
        ;;
    fail2ban|f2b)
        echo "=== Fail2ban ==="
        sudo tail -50 $FOLLOW /var/log/fail2ban.log
        ;;
    audit)
        echo "=== Audit Log ==="
        sudo ausearch -ts recent 2>/dev/null | tail -50 || sudo tail -50 $FOLLOW /var/log/audit/audit.log
        ;;
    svc|base)
        echo "=== BASE Service ==="
        if [ -f /home/svc/base/run/base.log ]; then
            tail -100 $FOLLOW /home/svc/base/run/base.log
        else
            echo "(no log file)"
        fi
        ;;
    all)
        echo "Usage: ops-logs [auth|nginx|fail2ban|audit|svc] [-f]"
        echo ""
        echo "Quick summary:"
        echo ""
        echo "--- Recent Auth (last 10 lines) ---"
        sudo tail -10 /var/log/auth.log 2>/dev/null || echo "(unavailable)"
        echo ""
        echo "--- Recent Nginx Errors (last 5) ---"
        sudo tail -5 /var/log/nginx/error.log 2>/dev/null || echo "(unavailable)"
        echo ""
        echo "--- Recent Fail2ban (last 5) ---"
        sudo tail -5 /var/log/fail2ban.log 2>/dev/null || echo "(unavailable)"
        ;;
    *)
        echo "Unknown log: $LOG"
        echo "Options: auth, nginx, fail2ban, audit, svc, all"
        exit 1
        ;;
esac
