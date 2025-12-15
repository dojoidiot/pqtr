#!/bin/bash
# Deploy nginx + SSL for PQTR
# Security: TLS 1.2+, HSTS, CSP, OCSP stapling, rate limiting, fail2ban
set -e
if [ $# -ne 3 ]; then
    echo "args: <host-name> <site-name> <site-mail>"
    exit 1
fi

HOST_NAME=$1
SITE_NAME=$2
SITE_MAIL=$3

ssh env@$HOST_NAME "bash -s" << END_INIT

if [ -f "/etc/nginx/sites-enabled/$SITE_NAME" ]; then
    echo "[warn] already configured"
    exit 0
fi

echo "[info] installing packages..."
sudo apt update
sudo apt upgrade -y
sudo apt install -y nginx certbot python3-certbot-nginx

echo "[info] configuring firewall..."
sudo ufw allow http
sudo ufw allow https

echo "[info] getting SSL certificate..."
sudo systemctl stop nginx
sudo certbot certonly --key-type ecdsa --elliptic-curve secp256r1 \\
    --non-interactive --agree-tos --standalone --preferred-challenges \\
    http -d $SITE_NAME -m $SITE_MAIL

echo "[info] writing nginx main config..."
sudo tee /etc/nginx/nginx.conf > /dev/null << 'NGINX_MAIN'
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

    # Rate limiting
    limit_req_zone \$binary_remote_addr zone=api:10m rate=10r/s;
    limit_req_zone \$binary_remote_addr zone=static:10m rate=50r/s;
    limit_conn_zone \$binary_remote_addr zone=conn:10m;

    # Security headers (global)
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;
    add_header Permissions-Policy "geolocation=(), microphone=(), camera=()" always;

    server_tokens off;
    more_clear_headers Server;

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
NGINX_MAIN

echo "[info] writing site config..."
sudo tee /etc/nginx/sites-enabled/$SITE_NAME > /dev/null << NGINX_SITE
upstream base { server 127.0.0.1:4040; }

# Redirect HTTP to HTTPS
server {
    listen 80;
    listen [::]:80;
    server_name $SITE_NAME;
    return 301 https://\\\$server_name\\\$request_uri;
}

# Block requests with no/wrong Host header (scanner bots)
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    listen 443 ssl default_server;
    listen [::]:443 ssl default_server;
    ssl_certificate /etc/letsencrypt/live/$SITE_NAME/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/$SITE_NAME/privkey.pem;
    return 444;
}

# Main site
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name $SITE_NAME;

    ssl_certificate /etc/letsencrypt/live/$SITE_NAME/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/$SITE_NAME/privkey.pem;

    # HSTS (1 year, include subdomains, allow preload)
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
        proxy_set_header Host \\\$host;
        proxy_set_header X-Real-IP \\\$remote_addr;
        proxy_set_header X-Forwarded-For \\\$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \\\$scheme;

        proxy_connect_timeout 5s;
        proxy_send_timeout 10s;
        proxy_read_timeout 30s;
    }

    location /push {
        limit_req zone=api burst=3 nodelay;
        limit_except POST { deny all; }
        client_max_body_size 50m;

        proxy_pass http://base;
        proxy_set_header Host \\\$host;
        proxy_set_header X-Real-IP \\\$remote_addr;
        proxy_set_header X-Forwarded-For \\\$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \\\$scheme;
    }

    location /pull {
        limit_req zone=api burst=10 nodelay;
        limit_except GET { deny all; }

        proxy_pass http://base;
        proxy_set_header Host \\\$host;
        proxy_set_header X-Real-IP \\\$remote_addr;
        proxy_set_header X-Forwarded-For \\\$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \\\$scheme;
    }

    # Static files (WASM app)
    location / {
        limit_req zone=static burst=20 nodelay;

        proxy_pass http://base;
        proxy_set_header Host \\\$host;
        proxy_set_header X-Real-IP \\\$remote_addr;

        location ~* \\.(js|wasm|css|png|jpg|ico)\\\$ {
            proxy_pass http://base;
            expires 1d;
            add_header Cache-Control "public, immutable";
        }
    }

    # Security: block common attack paths
    location ~ /\\. { deny all; }
    location ~ ~\\\$ { deny all; }
    location ~* (wp-admin|wp-login|xmlrpc|\\.php|\\.asp|\\.aspx|\\.jsp) { return 444; }
    location ~* (eval|base64|exec|system|passthru) { return 444; }
}
NGINX_SITE

echo "[info] configuring fail2ban for nginx..."
sudo tee /etc/fail2ban/filter.d/nginx-limit-req.conf > /dev/null << 'F2B_FILTER'
[Definition]
failregex = limiting requests, excess:.* by zone.*client: <HOST>
ignoreregex =
F2B_FILTER

sudo tee /etc/fail2ban/filter.d/nginx-botsearch.conf > /dev/null << 'F2B_BOTS'
[Definition]
failregex = ^<HOST> -.*"(GET|POST|HEAD).*HTTP.*" 444
            ^<HOST> -.*"(GET|POST|HEAD).*(wp-|xmlrpc|\.php|\.asp).*HTTP.*"
ignoreregex =
F2B_BOTS

sudo tee /etc/fail2ban/jail.d/nginx.conf > /dev/null << 'F2B_JAIL'
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
F2B_JAIL

echo "[info] removing default site..."
sudo rm -f /etc/nginx/sites-enabled/default

echo "[info] testing nginx config..."
sudo nginx -t

echo "[info] starting services..."
sudo systemctl restart fail2ban
sudo systemctl restart nginx
sudo systemctl enable nginx

echo "[info] setting up certbot auto-renewal..."
sudo systemctl enable certbot.timer
sudo systemctl start certbot.timer

echo "[done] nginx configured for $SITE_NAME"
echo ""
echo "Verify with:"
echo "  curl -I https://$SITE_NAME"
echo "  sudo nginx -t"
echo "  sudo fail2ban-client status"

END_INIT
