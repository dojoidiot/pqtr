# Nginx Configuration

Production nginx config for PQTR with security hardening.

Deployed automatically by `bin/http-init.sh`. This document is the reference.

## Security Checklist

| Feature | Status | Implementation |
|---------|--------|----------------|
| TLS 1.2/1.3 only | Yes | `ssl_protocols TLSv1.2 TLSv1.3` |
| ECDSA certificates | Yes | `--key-type ecdsa` |
| Strong ciphers | Yes | ECDHE-ECDSA/RSA-AES-GCM only |
| HSTS | Yes | 1 year, includeSubDomains, preload |
| CSP | Yes | Strict self + WASM policy |
| OCSP stapling | Yes | Cloudflare + Google resolvers |
| Rate limiting | Yes | 10r/s API, 50r/s static |
| Connection limits | Yes | 10 concurrent per IP |
| fail2ban | Yes | Rate limit + bot detection jails |
| Security headers | Yes | X-Frame, X-Content-Type, Referrer, Permissions |
| Bot blocking | Yes | Return 444 for scanners |
| Method restrictions | Yes | POST-only /jrpc, GET-only /pull |

## Main Config (/etc/nginx/nginx.conf)

```nginx
user www-data;
worker_processes auto;
pid /run/nginx.pid;

events {
    worker_connections 1024;
    multi_accept on;
}

http {
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    keepalive_timeout 30;
    types_hash_max_size 2048;

    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    access_log /var/log/nginx/access.log;
    error_log /var/log/nginx/error.log;

    # Rate limiting zones
    limit_req_zone $binary_remote_addr zone=api:10m rate=10r/s;
    limit_req_zone $binary_remote_addr zone=static:10m rate=50r/s;
    limit_conn_zone $binary_remote_addr zone=conn:10m;

    # Security headers (global)
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;
    add_header Permissions-Policy "geolocation=(), microphone=(), camera=()" always;

    server_tokens off;

    gzip on;
    gzip_vary on;
    gzip_proxied any;
    gzip_types text/plain text/css application/json application/javascript text/xml application/xml application/wasm;

    # TLS hardening
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off;
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 1d;
    ssl_session_tickets off;

    # OCSP stapling
    ssl_stapling on;
    ssl_stapling_verify on;
    resolver 1.1.1.1 8.8.8.8 valid=300s;
    resolver_timeout 5s;

    include /etc/nginx/sites-enabled/*;
}
```

## Site Config (/etc/nginx/sites-enabled/pqtr.ai)

```nginx
upstream base { server 127.0.0.1:4040; }

# Redirect HTTP to HTTPS
server {
    listen 80;
    listen [::]:80;
    server_name pqtr.ai;
    return 301 https://$server_name$request_uri;
}

# Block requests with no/wrong Host header (scanner bots)
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    listen 443 ssl default_server;
    listen [::]:443 ssl default_server;
    ssl_certificate /etc/letsencrypt/live/pqtr.ai/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/pqtr.ai/privkey.pem;
    return 444;
}

# Main site
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name pqtr.ai;

    ssl_certificate /etc/letsencrypt/live/pqtr.ai/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/pqtr.ai/privkey.pem;

    # HSTS (1 year, include subdomains, preload-ready)
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains; preload" always;

    # Content Security Policy
    add_header Content-Security-Policy "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; font-src 'self'; frame-ancestors 'none'; base-uri 'self'; form-action 'self'" always;

    # Connection limits
    limit_conn conn 10;
    client_max_body_size 1m;
    client_body_timeout 10s;
    client_header_timeout 10s;

    # API - strict rate limiting
    location /jrpc {
        limit_req zone=api burst=5 nodelay;
        limit_except POST { deny all; }

        proxy_pass http://base;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        proxy_connect_timeout 5s;
        proxy_send_timeout 10s;
        proxy_read_timeout 30s;
    }

    location /push {
        limit_req zone=api burst=3 nodelay;
        limit_except POST { deny all; }
        client_max_body_size 50m;

        proxy_pass http://base;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    location /pull {
        limit_req zone=api burst=10 nodelay;
        limit_except GET { deny all; }

        proxy_pass http://base;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    # Static files (WASM app)
    location / {
        limit_req zone=static burst=20 nodelay;

        proxy_pass http://base;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;

        location ~* \.(js|wasm|css|png|jpg|ico)$ {
            proxy_pass http://base;
            expires 1d;
            add_header Cache-Control "public, immutable";
        }
    }

    # Security: block attack paths
    location ~ /\. { deny all; }
    location ~ ~$ { deny all; }
    location ~* (wp-admin|wp-login|xmlrpc|\.php|\.asp|\.aspx|\.jsp) { return 444; }
    location ~* (eval|base64|exec|system|passthru) { return 444; }
}
```

## Fail2ban Configuration

### Rate Limit Filter (/etc/fail2ban/filter.d/nginx-limit-req.conf)

```ini
[Definition]
failregex = limiting requests, excess:.* by zone.*client: <HOST>
ignoreregex =
```

### Bot Detection Filter (/etc/fail2ban/filter.d/nginx-botsearch.conf)

```ini
[Definition]
failregex = ^<HOST> -.*"(GET|POST|HEAD).*HTTP.*" 444
            ^<HOST> -.*"(GET|POST|HEAD).*(wp-|xmlrpc|\.php|\.asp).*HTTP.*"
ignoreregex =
```

### Jails (/etc/fail2ban/jail.d/nginx.conf)

```ini
[nginx-limit-req]
enabled = true
filter = nginx-limit-req
logpath = /var/log/nginx/error.log
maxretry = 10
findtime = 60
bantime = 600
bantime.increment = true
bantime.factor = 2
bantime.maxtime = 86400

[nginx-botsearch]
enabled = true
filter = nginx-botsearch
logpath = /var/log/nginx/access.log
maxretry = 2
findtime = 60
bantime = 86400
```

## Verification

```bash
# Check nginx config syntax
sudo nginx -t

# Check headers (should see HSTS, CSP, etc.)
curl -I https://pqtr.ai

# Check SSL grade - should be A+
# https://www.ssllabs.com/ssltest/analyze.html?d=pqtr.ai

# Check security headers
# https://securityheaders.com/?q=pqtr.ai

# Check fail2ban
sudo fail2ban-client status
sudo fail2ban-client status nginx-limit-req
sudo fail2ban-client status nginx-botsearch

# Test rate limiting (should see 429s after burst)
for i in {1..20}; do curl -s -o /dev/null -w "%{http_code} " -X POST https://pqtr.ai/jrpc; done; echo

# Check certbot renewal
sudo certbot renew --dry-run
```

## Maintenance

```bash
# Reload nginx after config changes
sudo nginx -t && sudo systemctl reload nginx

# Unban an IP
sudo fail2ban-client set nginx-botsearch unbanip 1.2.3.4

# View logs
sudo tail -f /var/log/nginx/access.log
sudo tail -f /var/log/nginx/error.log

# Check certificate expiry
sudo certbot certificates

# Manual renewal
sudo certbot renew
```
