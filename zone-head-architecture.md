# Zone Head Architecture - Self-Hosted Storage & Process Control

## Zone Head Concept
A **Zone Head** serves as the authoritative node for a security zone, combining:
- Root CA for the zone
- Process controller for zone members
- Centralized secure storage
- Service orchestration

## 🗄️ Self-Hosted Storage Solutions

### 1. **Consul + Vault (Recommended)**
Best for: Production environments requiring high security

```bash
# Zone Head Stack
zone-head/
├── consul/              # Distributed KV store & service mesh
├── vault/               # Secret management
├── nomad/               # Process orchestration (optional)
└── fabio/               # Load balancer (optional)
```

**Implementation**:
```bash
# Start Consul (storage backend)
consul agent -server -bootstrap-expect=1 \
    -data-dir=/opt/consul/data \
    -config-dir=/opt/consul/config \
    -encrypt=$CONSUL_ENCRYPT_KEY \
    -bind=0.0.0.0

# Start Vault (uses Consul as backend)
vault server -config=/opt/vault/config.hcl

# config.hcl
storage "consul" {
  address = "127.0.0.1:8500"
  path    = "vault/"
}

listener "tcp" {
  address     = "0.0.0.0:8200"
  tls_disable = 0
  tls_cert_file = "/opt/vault/tls/cert.pem"
  tls_key_file  = "/opt/vault/tls/key.pem"
}
```

**Key Storage**:
```bash
# Store zone CA key
vault kv put secret/zones/production/ca \
    private_key=@zone-ca.key \
    certificate=@zone-ca.crt \
    serial_number=1000

# Enable SSH secrets engine
vault secrets enable -path=ssh-ca ssh
vault write ssh-ca/config/ca \
    private_key=@zone-ca.key \
    public_key=@zone-ca.pub
```

### 2. **etcd + Custom Key Manager**
Best for: Kubernetes-style environments

```bash
# Zone Head with etcd
zone-head/
├── etcd/                # Distributed reliable KV store
├── key-manager/         # Custom key management service
└── process-controller/  # Zone process management
```

**Setup**:
```bash
# Start etcd cluster (single node for zone head)
etcd --name zone-head \
    --data-dir=/var/lib/etcd \
    --listen-client-urls=https://0.0.0.0:2379 \
    --advertise-client-urls=https://zone-head:2379 \
    --client-cert-auth \
    --trusted-ca-file=/etc/etcd/ca.pem \
    --cert-file=/etc/etcd/server.pem \
    --key-file=/etc/etcd/server-key.pem
```

**Custom Key Manager Service**:
```python
# key-manager/app.py
import etcd3
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
import base64

class ZoneKeyManager:
    def __init__(self):
        self.etcd = etcd3.client(
            host='localhost',
            port=2379,
            ca_cert='/etc/etcd/ca.pem',
            cert_cert='/etc/etcd/client.pem',
            cert_key='/etc/etcd/client-key.pem'
        )
        # Master key from HSM or secure enclave
        self.master_key = self.get_master_key()
        self.cipher = ChaCha20Poly1305(self.master_key)
    
    def store_key(self, key_id, key_data):
        # Encrypt key before storing
        nonce = os.urandom(12)
        encrypted = self.cipher.encrypt(nonce, key_data, key_id.encode())
        
        # Store in etcd with metadata
        self.etcd.put(
            f'/keys/{key_id}',
            base64.b64encode(nonce + encrypted).decode(),
            lease=self.create_lease()
        )
    
    def retrieve_key(self, key_id):
        encrypted_data = self.etcd.get(f'/keys/{key_id}')
        if not encrypted_data:
            raise KeyError(f"Key {key_id} not found")
        
        # Decrypt
        data = base64.b64decode(encrypted_data)
        nonce, ciphertext = data[:12], data[12:]
        return self.cipher.decrypt(nonce, ciphertext, key_id.encode())
```

### 3. **PostgreSQL + pgcrypto**
Best for: Leveraging existing database infrastructure

```sql
-- Schema for zone head storage
CREATE SCHEMA zone_head;

-- Encrypted key storage
CREATE TABLE zone_head.keys (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    key_type VARCHAR(50) NOT NULL,
    key_id VARCHAR(255) UNIQUE NOT NULL,
    encrypted_data BYTEA NOT NULL,
    metadata JSONB,
    created_at TIMESTAMP DEFAULT NOW(),
    expires_at TIMESTAMP,
    revoked BOOLEAN DEFAULT FALSE
);

-- Audit log
CREATE TABLE zone_head.audit_log (
    id BIGSERIAL PRIMARY KEY,
    operation VARCHAR(50) NOT NULL,
    key_id VARCHAR(255),
    user_id VARCHAR(100),
    ip_address INET,
    details JSONB,
    timestamp TIMESTAMP DEFAULT NOW()
);

-- Store encrypted key
INSERT INTO zone_head.keys (key_type, key_id, encrypted_data, metadata)
VALUES (
    'zone-ca',
    'production-zone-ca',
    pgp_sym_encrypt('-----BEGIN PRIVATE KEY-----...', 'master-password'),
    '{"zone": "production", "algorithm": "ed25519"}'::jsonb
);
```

**Access Layer**:
```python
# zone-storage/db_storage.py
import psycopg2
from psycopg2.extras import RealDictCursor

class ZoneStorage:
    def __init__(self, conn_string, master_password):
        self.conn = psycopg2.connect(conn_string)
        self.master_password = master_password
    
    def store_key(self, key_type, key_id, key_data, metadata=None):
        with self.conn.cursor() as cur:
            cur.execute("""
                INSERT INTO zone_head.keys 
                (key_type, key_id, encrypted_data, metadata)
                VALUES (%s, %s, pgp_sym_encrypt(%s, %s), %s)
            """, (key_type, key_id, key_data, self.master_password, metadata))
            self.conn.commit()
            self.audit_log('store_key', key_id)
    
    def get_key(self, key_id):
        with self.conn.cursor(cursor_factory=RealDictCursor) as cur:
            cur.execute("""
                SELECT key_type, 
                       pgp_sym_decrypt(encrypted_data, %s) as key_data,
                       metadata
                FROM zone_head.keys
                WHERE key_id = %s AND NOT revoked
            """, (self.master_password, key_id))
            result = cur.fetchone()
            self.audit_log('retrieve_key', key_id)
            return result
```

### 4. **SoftHSM + PKCS#11**
Best for: Hardware security without physical HSM

```bash
# Install and configure SoftHSM
apt-get install softhsm2

# Initialize token for zone head
softhsm2-util --init-token --slot 0 \
    --label "zone-head-production" \
    --pin 1234 --so-pin 4321

# Store zone CA key in HSM
pkcs11-tool --module /usr/lib/softhsm/libsofthsm2.so \
    --login --pin 1234 \
    --write-object zone-ca.key \
    --type privkey --id 01 --label "zone-ca"
```

**Integration Layer**:
```python
# hsm_storage.py
from PyKCS11 import *

class HSMStorage:
    def __init__(self, module_path, pin):
        self.pkcs11 = PyKCS11Lib()
        self.pkcs11.load(module_path)
        self.session = None
        self.pin = pin
        self.connect()
    
    def connect(self):
        slot = self.pkcs11.getSlotList(tokenPresent=True)[0]
        self.session = self.pkcs11.openSession(slot)
        self.session.login(self.pin)
    
    def store_key(self, label, key_data):
        # Store key in HSM
        template = [
            (CKA_CLASS, CKO_SECRET_KEY),
            (CKA_KEY_TYPE, CKK_AES),
            (CKA_LABEL, label),
            (CKA_VALUE, key_data),
            (CKA_ENCRYPT, True),
            (CKA_DECRYPT, True)
        ]
        self.session.createObject(template)
```

### 5. **Tang + Clevis (Network-Bound Disk Encryption)**
Best for: Automated key escrow and recovery

```bash
# Install Tang server on zone head
apt-get install tang
systemctl enable tangd.socket
systemctl start tangd.socket

# Zone members use Clevis to bind to Tang
apt-get install clevis clevis-luks

# Bind disk encryption to zone head
clevis luks bind -d /dev/vda2 tang '{"url":"http://zone-head"}'
```

## 🎛️ Process Control Tools

### 1. **systemd + D-Bus**
For zone member service control:

```python
# process_controller.py
import dbus
import paramiko

class ZoneProcessController:
    def __init__(self, zone_members):
        self.members = zone_members
        self.bus = dbus.SystemBus()
    
    def control_service(self, host, service, action):
        ssh = paramiko.SSHClient()
        ssh.load_host_keys('/etc/ssh/ssh_known_hosts')
        ssh.connect(host, username='zone-controller', 
                   key_filename='/opt/zone/keys/controller.key')
        
        command = f"sudo systemctl {action} {service}"
        stdin, stdout, stderr = ssh.exec_command(command)
        return stdout.read().decode()
    
    def restart_all_services(self, service):
        results = {}
        for member in self.members:
            results[member] = self.control_service(member, service, 'restart')
        return results
```

### 2. **Ansible Tower/AWX**
For complex orchestration:

```yaml
# zone-head-playbook.yml
---
- name: Zone Head Operations
  hosts: zone_members
  vars:
    zone_name: production
  tasks:
    - name: Distribute new certificates
      copy:
        src: "/opt/zone/certs/{{ inventory_hostname }}.crt"
        dest: /etc/ssl/certs/host.crt
        owner: root
        mode: '0644'
      notify: restart services
    
    - name: Update zone configuration
      template:
        src: zone.conf.j2
        dest: /etc/zone/zone.conf
      notify: reload configuration
```

### 3. **Consul + Consul-Template**
For dynamic configuration:

```hcl
# consul-template configuration
template {
  source = "/opt/zone/templates/nginx.conf.ctmpl"
  destination = "/etc/nginx/nginx.conf"
  command = "systemctl reload nginx"
  perms = 0644
}

# nginx.conf.ctmpl
upstream backend {
  {{range service "web"}}
  server {{.Address}}:{{.Port}};
  {{end}}
}
```

## 📦 Complete Zone Head Stack

```bash
zone-head/
├── storage/
│   ├── consul/          # Primary KV store
│   ├── vault/           # Secret management
│   └── postgres/        # Backup storage
├── control/
│   ├── ansible/         # Configuration management
│   ├── systemd/         # Service control
│   └── monitoring/      # Prometheus + Grafana
├── security/
│   ├── softhsm/         # Software HSM
│   ├── tang/            # Key escrow
│   └── audit/           # Audit logging
└── api/
    ├── rest/            # REST API for zone operations
    └── grpc/            # gRPC for inter-zone communication
```

## 🚀 Zone Head Deployment Script

```bash
#!/bin/bash
# deploy-zone-head.sh

set -e

ZONE_NAME=${1:-production}
ZONE_HEAD_IP=${2:-10.0.0.10}

echo "Deploying Zone Head for $ZONE_NAME..."

# 1. Install base components
apt-get update
apt-get install -y consul vault postgresql softhsm2 tang ansible

# 2. Configure Consul
cat > /etc/consul/server.json <<EOF
{
  "server": true,
  "datacenter": "$ZONE_NAME",
  "data_dir": "/opt/consul/data",
  "log_level": "INFO",
  "encrypt": "$(consul keygen)",
  "ca_file": "/etc/consul/ca.pem",
  "cert_file": "/etc/consul/server.pem",
  "key_file": "/etc/consul/server-key.pem",
  "verify_incoming": true,
  "verify_outgoing": true
}
EOF

# 3. Configure Vault
cat > /etc/vault/config.hcl <<EOF
storage "consul" {
  address = "127.0.0.1:8500"
  path    = "vault/"
}

listener "tcp" {
  address     = "$ZONE_HEAD_IP:8200"
  tls_disable = 0
  tls_cert_file = "/etc/vault/tls/cert.pem"
  tls_key_file  = "/etc/vault/tls/key.pem"
}

api_addr = "https://$ZONE_HEAD_IP:8200"
cluster_addr = "https://$ZONE_HEAD_IP:8201"
EOF

# 4. Initialize SoftHSM
softhsm2-util --init-token --slot 0 \
    --label "$ZONE_NAME-hsm" \
    --pin $HSM_PIN --so-pin $HSM_SO_PIN

# 5. Start services
systemctl enable --now consul vault tangd postgresql

# 6. Initialize Vault
vault operator init -key-shares=5 -key-threshold=3

echo "Zone Head deployed successfully!"
```

## 🔐 Security Considerations

1. **Network Isolation**: Zone head on dedicated VLAN
2. **Firewall Rules**: Strict ingress/egress controls
3. **Encrypted Communication**: TLS for all connections
4. **Access Control**: MFA for zone head operations
5. **Backup Strategy**: Regular encrypted backups of storage
6. **Monitoring**: Real-time alerts for anomalies
7. **Disaster Recovery**: Secondary zone head for failover

This architecture provides a robust, self-hosted solution for zone head operations with multiple storage options depending on your security and operational requirements.