# SSH Key Management Ideas for Host Deployment

## Current System Analysis
Your `host/` directory already implements a zone-based SSH certificate authority (CA) model, which is excellent. You're using:
- **Zone keys** as certificate authorities
- **User certificates** signed by zone CA
- **Host certificates** for server authentication
- **Ed25519** keys (good choice for security/performance)

## 🔐 Recommended Improvements

### 1. Key Rotation & Lifecycle Management
**Problem**: No automated key rotation visible in current scripts

**Solution**:
```bash
# Add to host/bin/
key-rotate.sh         # Automated key rotation
key-audit.sh          # Check key ages and expiry
key-revoke.sh         # Certificate revocation
```

**Implementation**:
- Set certificate validity periods (e.g., 90 days for users, 1 year for hosts)
- Automated renewal warnings at 30, 15, 7 days before expiry
- Keep rotation history for audit trail
- Implement grace periods for smooth transitions

### 2. Centralized Key Storage & Distribution
**Current Issue**: Keys stored locally in `etc/ssh/`

**Options**:

#### Option A: HashiCorp Vault (Recommended for production)
```bash
# Store zone CA keys in Vault
vault kv put secret/ssh/zones/production \
    private_key=@production.skey \
    public_key=@production.skey.pub

# Dynamic SSH certificate generation
vault write -field=signed_key ssh-client-signer/sign/admin \
    public_key=@admin.pub \
    valid_principals="admin,deploy" \
    ttl="8h"
```

#### Option B: AWS Secrets Manager/Parameter Store
```bash
# Store encrypted in AWS
aws secretsmanager create-secret \
    --name /ssh/zones/production/ca-key \
    --secret-string file://production.skey

# Retrieve with IAM role permissions
aws secretsmanager get-secret-value \
    --secret-id /ssh/zones/production/ca-key
```

#### Option C: Self-Hosted Solution
```bash
# Create encrypted key repository
host/
├── keystore/
│   ├── zones/           # Encrypted zone keys
│   ├── hosts/           # Host certificates
│   ├── users/           # User certificates
│   └── revoked/         # CRL management
```

### 3. Certificate Authority Hierarchy
**Enhancement**: Implement multi-tier CA structure

```
Root CA (offline, hardware-secured)
    └── Zone CAs (online, rotated quarterly)
        ├── Production Zone CA
        ├── Staging Zone CA
        └── Development Zone CA
```

**Benefits**:
- Root CA compromise doesn't affect all zones
- Zone-specific revocation
- Different security policies per environment

### 4. Automated Certificate Management
**New Scripts**:

```bash
# host/bin/cert-manager.sh
#!/bin/bash
cert_manager() {
    case $1 in
        issue)
            # Auto-detect user/host type
            # Apply appropriate validity period
            # Log issuance for audit
            ;;
        renew)
            # Check expiry dates
            # Auto-renew if < 30 days
            # Notify admins
            ;;
        revoke)
            # Add to revocation list
            # Distribute CRL to all hosts
            # Log revocation reason
            ;;
    esac
}
```

### 5. SSH Key Management API
**Create REST API for key operations**:

```python
# host/api/ssh_manager.py
from flask import Flask, jsonify
import subprocess

app = Flask(__name__)

@app.route('/api/ssh/issue/<username>', methods=['POST'])
def issue_certificate(username):
    # Validate request
    # Generate certificate with appropriate principals
    # Return signed certificate
    pass

@app.route('/api/ssh/revoke/<cert_id>', methods=['POST'])
def revoke_certificate(cert_id):
    # Add to CRL
    # Propagate revocation
    pass

@app.route('/api/ssh/status', methods=['GET'])
def certificate_status():
    # Return active certificates
    # Show expiry dates
    # List recent operations
    pass
```

### 6. Enhanced Security Features

#### A. Hardware Security Module (HSM) Integration
```bash
# Store zone CA keys in HSM
pkcs11-tool --module /usr/lib/opensc-pkcs11.so \
    --login --pin $HSM_PIN \
    --write-object production.skey --type privkey
```

#### B. Multi-Factor Authentication for CA Operations
```bash
# Require TOTP for sensitive operations
zone-make-mfa.sh() {
    read -p "Enter TOTP code: " totp_code
    verify_totp $totp_code || exit 1
    # Proceed with key generation
}
```

#### C. Audit Logging
```bash
# Enhanced logging for all operations
log_operation() {
    echo "$(date -Iseconds) | $USER | $1 | $2" >> /var/log/ssh-ca/audit.log
    # Send to SIEM system
    logger -p auth.info -t ssh-ca "$1: $2"
}
```

### 7. Key Distribution Improvements

#### A. Pull-Based Distribution
```bash
# Hosts pull their certificates
host-pull-cert.sh() {
    # Run via cron on hosts
    curl -H "Authorization: Bearer $HOST_TOKEN" \
        https://ca.internal/api/host-cert/$HOSTNAME \
        -o /etc/ssh/ssh_host_ed25519_key-cert.pub
}
```

#### B. Push-Based with Ansible
```yaml
# ansible/ssh-cert-deploy.yml
- hosts: all
  tasks:
    - name: Deploy host certificate
      copy:
        src: "certs/{{ inventory_hostname }}-cert.pub"
        dest: /etc/ssh/ssh_host_ed25519_key-cert.pub
        mode: '0644'
      notify: restart sshd
```

### 8. Emergency Access Procedures

```bash
# host/bin/emergency-access.sh
#!/bin/bash
# Break-glass procedure for emergency access
emergency_cert() {
    # Generate time-limited certificate (1 hour)
    # Log extensively
    # Alert security team
    # Require dual authorization
    ssh-keygen -s emergency-ca.key \
        -V +1h \
        -n root,admin \
        -I "emergency-$(date +%s)" \
        $USER_KEY
}
```

### 9. Monitoring & Alerting

```bash
# Monitor certificate operations
monitor-ssh-ca.sh() {
    # Check certificate expiry
    find /etc/ssh -name "*-cert.pub" -exec ssh-keygen -L -f {} \; | \
        grep "Valid:" | check_expiry
    
    # Alert on unusual activity
    tail -f /var/log/ssh-ca/audit.log | \
        grep -E "(revoke|emergency|root)" | \
        send_alert
    
    # Track failed authentication attempts
    journalctl -u sshd | grep "certificate" | analyze_patterns
}
```

### 10. Integration with CI/CD

```groovy
// Jenkinsfile
pipeline {
    stages {
        stage('Deploy') {
            steps {
                script {
                    // Get temporary deployment certificate
                    sh """
                        vault write -field=signed_key \
                            ssh-client-signer/sign/jenkins \
                            public_key=@jenkins.pub \
                            valid_principals="deploy" \
                            ttl="30m" > jenkins-cert.pub
                    """
                    // Use certificate for deployment
                    sh "ssh -i jenkins -o CertificateFile=jenkins-cert.pub deploy@host 'deploy.sh'"
                }
            }
        }
    }
}
```

## 🚀 Implementation Priority

### Phase 1: Foundation (Week 1)
1. Implement key rotation scripts
2. Set up audit logging
3. Create certificate expiry monitoring

### Phase 2: Automation (Week 2)
1. Build certificate management API
2. Implement automated renewal
3. Set up CRL distribution

### Phase 3: Security Hardening (Week 3)
1. Integrate with secret management system
2. Implement MFA for CA operations
3. Set up emergency access procedures

### Phase 4: Integration (Week 4)
1. CI/CD integration
2. Monitoring and alerting
3. Documentation and training

## 📊 Quick Wins

1. **Add certificate expiry checking**:
```bash
echo "0 9 * * 1 /opt/host/bin/check-cert-expiry.sh" | crontab -
```

2. **Implement basic audit logging**:
```bash
echo 'Match User *
    ForceCommand /usr/local/bin/ssh-audit-wrapper $SSH_ORIGINAL_COMMAND' >> /etc/ssh/sshd_config
```

3. **Create key inventory**:
```bash
find /home -name "*.pub" -o -name "*-cert.pub" | \
    xargs -I {} ssh-keygen -l -f {} > /var/log/key-inventory.txt
```

## 🔒 Security Best Practices

1. **Never store CA private keys on managed hosts**
2. **Use short-lived certificates** (hours/days, not years)
3. **Implement certificate transparency** logging
4. **Regular security audits** of CA operations
5. **Principle of least privilege** for certificate principals
6. **Separate CAs** for different environments
7. **Automated certificate renewal** before expiry
8. **Immutable audit logs** shipped to central SIEM
9. **Regular disaster recovery drills**
10. **Certificate pinning** for critical services

## 📈 Metrics to Track

- Certificate issuance rate
- Average certificate lifetime
- Renewal success rate
- Time to revocation
- Failed authentication attempts
- Unauthorized access attempts
- Certificate expiry incidents
- CA availability (uptime)

This approach builds on your existing zone-based system while adding enterprise-grade key management capabilities suitable for production deployments.