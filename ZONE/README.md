# ZONE - PQTR Server Management

Scripts for hardening and managing PQTR infrastructure nodes.

## Structure

```
ZONE/
├── bin/
│   ├── zone-make.sh     # Create pqtr zone keys (once)
│   ├── user-make.sh     # Create user SSH keys
│   ├── user-sign.sh     # Sign user for zone access
│   ├── node-make.sh     # Harden a new server
│   ├── node-sign.sh     # Sign node for zone membership
│   ├── http-init.sh     # Deploy nginx + SSL
│   ├── ops-init.sh      # Deploy ops monitoring tools
│   ├── ops-status.sh    # System status (no sudo)
│   ├── ops-security.sh  # Security audit (sudo)
│   └── ops-logs.sh      # Log viewer
├── doc/
│   ├── nginx.md         # Production nginx configuration
│   └── yubi.md          # YubiKey setup guide
└── etc/ssh/             # SSH keys (gitignored)
```

## Setup New Node

### 1. Create Zone (once, on desktop)

```bash
cd ZONE
./bin/zone-make.sh pqtr
./bin/user-make.sh env pqtr
./bin/user-sign.sh ~/.ssh/pqtr.skey env ~/.ssh/env-pqtr.skey.pub
```

### 2. Harden Server

```bash
# Provision VM in cloud panel, get IP and root password
./bin/node-make.sh <IP> <hostname> ~/.ssh/pqtr.skey.pub
```

This installs: SSH certificates, fail2ban, ufw, auditd, kernel hardening.

### 3. Sign Node

```bash
./bin/node-sign.sh <hostname> ~/.ssh/pqtr.skey
```

### 4. Deploy Web Services

```bash
./bin/http-init.sh <hostname> pqtr.ai admin@pqtr.ai
```

### 5. Deploy Ops Tools

```bash
./bin/ops-init.sh <hostname>
```

## Users

| User | SSH | Sudo | Purpose |
|------|-----|------|---------|
| `env` | Yes | Full | Environment management |
| `ops` | Yes | Limited | Console monitoring |
| `svc` | No | No | Web services |
| `sdm` | No | No | Secure data |

## Ops Monitoring

After `ops-init.sh`, the ops user has these commands:

```bash
ssh ops@<hostname>

# Quick commands (aliases)
status      # System overview (no sudo)
security    # Security audit report
logs nginx  # View nginx logs
logs auth   # View auth logs
logs f2b    # View fail2ban logs
bans        # Fail2ban status
conns       # Current connections
ports       # Listening ports
load        # Load and memory
```

### ops-status (no sudo required)
- System info (hostname, uptime, kernel)
- Memory and disk usage
- Service status (sshd, nginx, fail2ban, auditd)
- Network connections and ports
- Logged in users

### ops-security (sudo required)
- fail2ban jails and banned IPs
- Firewall rules
- Auth failures (last 24h)
- Audit log events
- Certificate expiry
- Pending security updates

### ops-logs
```bash
ops-logs auth       # Auth log
ops-logs nginx      # Nginx access + errors
ops-logs fail2ban   # Ban events
ops-logs audit      # Audit events
ops-logs svc        # BASE service log
ops-logs all        # Summary of all
ops-logs nginx -f   # Follow mode
```

## Deploying BASE

```bash
# From project root (builds and deploys)
./send.sh <hostname>

# On server
sudo -u svc /home/svc/base/bin/base.sh exec
```

## SSH Access

Certificate-based SSH:

```bash
# Admin access
ssh -i ~/.ssh/env-pqtr.skey env@<hostname>

# Ops access (monitoring only)
ssh -i ~/.ssh/ops-pqtr.skey ops@<hostname>
```

## Key Files

**Desktop** (`~/.ssh/`):
- `pqtr.skey` - Zone CA private key (protect!)
- `pqtr.skey.pub` - Zone CA public key
- `env-pqtr.skey*` - Admin user keys
- `ops-pqtr.skey*` - Ops user keys

**Server** (`/etc/ssh/`):
- `host.skey` - Node private key
- `host.cert` - Node certificate
- `zone.pkey` - Zone public key

## Security Hardening Applied

### node-make.sh
- SSH: Ed25519 certs, MaxAuthTries=3, no forwarding, idle timeout
- Firewall: ufw default deny, SSH rate limited
- fail2ban: SSH jail with incremental bans
- Kernel: sysctl hardening (ASLR, network protections)
- Audit: auditd monitoring critical files
- Services: Disable cups, avahi, bluetooth
- Filesystem: Secure umask, protected symlinks

### http-init.sh
- TLS: 1.2/1.3 only, ECDSA, OCSP stapling
- Headers: HSTS, CSP, X-Frame-Options, etc.
- Rate limiting: 10r/s API, 50r/s static
- fail2ban: Rate limit + bot detection jails
- Auto-renewal: certbot timer enabled

---

## Appendix: Risks & Mitigations

A simple guide to why each security measure exists.

### A. Authentication Attacks

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Password guessing** | Attacker tries common passwords | No passwords allowed - certificates only |
| **Brute force** | Automated login attempts | fail2ban bans after 3 failures |
| **Stolen SSH key** | Attacker gets your private key | Keys on YubiKey (can't be copied) |
| **Man-in-the-middle** | Attacker impersonates server | SSH certificates verify server identity |
| **Credential stuffing** | Reused passwords from breaches | No passwords, unique keys per user |

**How certificates help:** Traditional SSH uses "trust on first use" - you hope the server is real the first time. With certificates, both sides prove identity through the zone CA. A stolen key only works if the attacker also has physical access to your YubiKey.

### B. Network Attacks

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Port scanning** | Attacker finds open services | ufw blocks all except 22/80/443 |
| **SSH flooding** | Overwhelm SSH with connections | `ufw limit ssh` rate limits |
| **DDoS on web** | Flood HTTP requests | nginx rate limiting + fail2ban |
| **IP spoofing** | Fake source addresses | Kernel `rp_filter` validates sources |
| **ICMP attacks** | Ping floods, smurf attacks | Kernel ignores broadcast pings |
| **SYN floods** | Exhaust connection table | Kernel `tcp_syncookies` enabled |

**Defense in depth:** Multiple layers catch what others miss. Rate limiting slows attacks, fail2ban blocks repeat offenders, and kernel settings handle low-level attacks.

### C. Web Application Attacks

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Eavesdropping** | Read traffic in transit | TLS 1.2/1.3 encryption |
| **Downgrade attack** | Force weak encryption | Only strong ciphers allowed |
| **SSL stripping** | Redirect HTTPS to HTTP | HSTS forces HTTPS for 1 year |
| **Clickjacking** | Embed site in malicious iframe | X-Frame-Options: SAMEORIGIN |
| **XSS (cross-site scripting)** | Inject malicious JavaScript | CSP restricts script sources |
| **MIME confusion** | Browser misinterprets file type | X-Content-Type-Options: nosniff |
| **Scanner bots** | Probe for vulnerabilities | Return 444 (drop connection) |
| **WordPress probes** | /wp-admin, xmlrpc attacks | Block + ban after 2 attempts |

**Content Security Policy explained:** CSP tells browsers "only run scripts from our domain." Even if an attacker injects `<script>evil()</script>`, the browser refuses to run it because it didn't come from 'self'.

### D. Privilege Escalation

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Root compromise** | Attacker gains full control | No root login, sudo required |
| **Service escape** | Web app escapes to system | svc user has no sudo, limited permissions |
| **Kernel exploit** | Attack via kernel vulnerability | ASLR randomizes memory layout |
| **Process debugging** | Attach to other processes | ptrace restricted to children |
| **SUID abuse** | Exploit privileged binaries | Protected symlinks/hardlinks |

**Separation of duties:** Each user can only do their job:
- `env` manages the system (sudo)
- `ops` monitors only (limited sudo for read-only)
- `svc` runs services (no sudo, can't escape)
- `sdm` handles data (no sudo, isolated)

### E. Lateral Movement

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Agent forwarding** | Hijack SSH agent to jump hosts | AllowAgentForwarding no |
| **Port forwarding** | Tunnel through server | AllowTcpForwarding no |
| **X11 forwarding** | GUI-based attacks | X11Forwarding no |
| **Session hijacking** | Take over idle session | ClientAliveInterval kicks idle users |
| **Multiple sessions** | Many shells from one compromise | MaxSessions 3 |

**Why disable forwarding:** If an attacker compromises a session, forwarding lets them use your server as a launchpad. With forwarding disabled, compromise is contained to that one server.

### F. Information Disclosure

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Version fingerprinting** | Learn software versions | server_tokens off, no Server header |
| **Error messages** | Reveal internal paths | Generic error pages |
| **Directory listing** | See all files | Disabled by default |
| **Kernel info leak** | /proc exposes details | kptr_restrict, dmesg_restrict |
| **Core dumps** | Passwords in crash dumps | suid_dumpable = 0 |

**Why hide versions:** Attackers look for "nginx/1.18.0" to find known vulnerabilities. Without version info, they must try attacks blindly, triggering detection.

### G. Persistence & Detection Evasion

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Hidden changes** | Attacker modifies configs | auditd monitors /etc/ssh, /etc/sudoers |
| **User creation** | Add backdoor account | auditd monitors /etc/passwd, /etc/shadow |
| **Cron backdoor** | Schedule malicious jobs | auditd monitors /etc/crontab |
| **Log tampering** | Delete evidence | Audit log separate, harder to modify |
| **Unnoticed access** | Attacker returns undetected | ops-security shows recent auth failures |

**Audit logging:** Every change to critical files is recorded. Even if an attacker gains access, their actions leave traces. The `-e 2` makes audit rules immutable - requires reboot to change.

### H. Denial of Service

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Connection exhaustion** | Use all available connections | limit_conn 10 per IP |
| **Slowloris** | Hold connections open | client_body_timeout 10s |
| **Request flooding** | Overwhelm application | Rate limiting + fail2ban |
| **Disk filling** | Fill logs to crash system | Log rotation (logrotate) |
| **Memory exhaustion** | Use all RAM | Connection limits, request limits |

**Incremental bans:** fail2ban starts with 10-minute bans, doubling each time up to 24 hours. Persistent attackers get longer bans automatically.

### I. Certificate & Crypto Risks

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Expired certificate** | Site shows security warning | certbot auto-renewal |
| **Weak algorithms** | Crypto can be broken | Ed25519 + AES-256-GCM only |
| **Certificate theft** | Attacker impersonates site | Private keys have strict permissions |
| **Revocation blindness** | Can't check if cert is revoked | OCSP stapling enabled |
| **Zone CA compromise** | All nodes compromised | Protect zone private key (YubiKey) |

**The zone CA is the crown jewel:** If the zone CA private key (`pqtr.skey`) is stolen, an attacker can create valid certificates for any user or host. Keep it on a YubiKey or air-gapped machine.

### J. Supply Chain & Updates

| Risk | What happens | Mitigation |
|------|--------------|------------|
| **Unpatched vulnerabilities** | Known exploits work | unattended-upgrades auto-patches |
| **Delayed security fixes** | Window of exposure | Security updates applied automatically |
| **Compromised packages** | Malware in updates | Use official Ubuntu/Debian repos only |

**Why auto-updates:** Most breaches exploit known vulnerabilities with available patches. Automatic security updates close the window between disclosure and patching.

---

### Quick Reference: What Protects What

```
Attack Surface          Protection Stack
─────────────────────   ─────────────────────────────────────
Internet → Firewall     ufw (default deny, rate limit SSH)
         → Web Server   nginx (TLS, rate limit, headers, bot blocking)
         → fail2ban     (ban repeat offenders)
         → Application  BASE (behind proxy, no direct exposure)

SSH → Firewall          ufw limit ssh
    → SSH Server        Certificates, no passwords, no root
    → fail2ban          Ban after 3 failures
    → Kernel            ptrace/ASLR protections

Local → Users           Separated accounts (env/ops/svc/sdm)
      → Permissions     Minimal sudo, secure umask
      → Audit           auditd logs all changes
      → Kernel          Hardened sysctl settings
```

### If Something Goes Wrong

| Symptom | Check | Fix |
|---------|-------|-----|
| Can't SSH | `ops-security` from another node | Check fail2ban, verify key/cert |
| Site down | `status`, `logs nginx` | Check nginx, BASE service |
| High load | `status`, `conns` | Check for attack, review bans |
| Suspicious activity | `security`, `logs auth` | Review audit log, rotate keys |
| Certificate expiring | `security` shows days left | `sudo certbot renew` |

---

*Remember: Security is layers. No single measure is perfect, but together they make attacks expensive and detection likely.*
