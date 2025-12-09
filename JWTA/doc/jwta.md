# JWTA - JWT Web Auth

Email OTP authentication with role-based access control.

## Architecture

```
inc/
  jwta.hpp    # Claims, crypto, jwt, rpc, Service
  data.hpp    # Store interface (User, Otp)
  mail.hpp    # Mailer interface

src/main/
  jwta.cpp    # crypto, jwt, Service implementation
  host.cpp    # HTTP server (main)
  plug/
    sqlite.cpp   # Store implementation
    mailgun.cpp  # Mailer implementation
```

## CLI

```bash
./tmp/make/jwta --data-area <path> <config.json>
```

## Config

`etc/jwta.json`:

```json
{
    "host": "127.0.0.1",
    "port": 8080,
    "jrpc_path": "/jrpc",
    "boot_path": "/boot",
    "admin_email": "admin@example.com",
    "boot_email": "boot@example.com",
    "otp_from": "Auth <auth@example.com>",
    "otp_text": "Your verification code is: %s\n\nThis code expires in 10 minutes.",
    "sqlite": {
        "file": "jwta.db"
    },
    "mailgun": {
        "api_key": "mailgun.api_key",
        "domain": "example.com",
        "region": "eu"
    }
}
```

## Secrets

Linux kernel keyring (key name from `mailgun.api_key` in config):

```bash
keyctl add user "mailgun.api_key" "YOUR_KEY" @u
```

## JRPC Endpoints

`POST /jrpc` (JSON-RPC 2.0)

| Method | Params | Returns |
|--------|--------|---------|
| `register` | `email` | `{ok, expires}` |
| `verify` | `email, otp` | `{jwt, refresh_token, user_id, role}` |
| `login` | `email` | `{ok, expires}` |
| `refresh` | `refresh_token` | `{jwt}` |

### Admin Methods (PQTR role)

| Method | Params | Returns |
|--------|--------|---------|
| `find` | `jwt, email` | `{user_id, email, tier, role, locked, created_at}` |
| `give` | `jwt, user_id, role` | `{ok}` |
| `take` | `jwt, user_id` | `{ok}` |
| `lock` | `jwt, user_id` | `{ok}` |
| `free` | `jwt, user_id` | `{ok}` |
| `drop` | `jwt, user_id` | `{ok}` |
| `info` | `jwt` | `{total_users, users_none, users_play, users_hero, users_pqtr}` |

## Roles

| Role | Description |
|------|-------------|
| `NONE` | Default, no special access |
| `PLAY` | Player access |
| `HERO` | Enhanced access |
| `PQTR` | Admin, can manage users |

## JWT Format

```json
{
  "alg": "EdDSA",
  "typ": "JWT"
}
```

```json
{
  "iss": "jwta.pqtr.io",
  "sub": "user-uuid",
  "email": "user@example.com",
  "tier": "registered",
  "role": "NONE",
  "iat": 1702000000,
  "exp": 1702003600
}
```

## Bootstrap

1. Configure `admin_email` and `boot_email` in config
2. Start server - sends bootstrap token to `boot_email`
3. Reply to email (Mailgun routes to `boot_path` endpoint)
4. `admin_email` gets PQTR role on next register/login

## Nginx

```nginx
upstream jwta { server 127.0.0.1:8080; }

server {
    listen 443 ssl;
    server_name auth.example.com;

    client_max_body_size 8k;
    limit_req zone=auth burst=5 nodelay;

    location /jrpc {
        proxy_pass http://jwta;
        proxy_set_header X-Real-IP $remote_addr;
    }
    location /boot {
        proxy_pass http://jwta;
    }
}

limit_req_zone $binary_remote_addr zone=auth:10m rate=10r/m;
```
