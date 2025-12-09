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

## JRPC Endpoints

`POST /rpc` (JSON-RPC 2.0)

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

## Config

`etc/jwta.json`:

```json
{
  "host": "127.0.0.1",
  "port": 8080,
  "db_path": "var/jwta.db",
  "admin_email": "admin@example.com",
  "boot_email": "boot@example.com",
  "mailgun": {
    "domain": "example.com",
    "from": "noreply@example.com",
    "region": "eu"
  }
}
```

## Secrets

Linux kernel keyring (required):

```bash
keyctl add user "jwta:mailgun_api_key" "YOUR_KEY" @u
keyctl add user "jwta:mailgun_domain" "example.com" @u
keyctl add user "jwta:mailgun_from" "noreply@example.com" @u
keyctl add user "jwta:mailgun_region" "eu" @u
```

## CLI

```bash
./tmp/make/jwta --info-file etc/jwta.json --data-area /path/to/data/
```

## Bootstrap

1. Configure `admin_email` and `boot_email` in config
2. Start server - sends bootstrap token to `boot_email`
3. Reply to email (Mailgun routes to `/boot` endpoint)
4. `admin_email` gets PQTR role on next register/login

## Endpoints

- `GET /health` - Health check
- `POST /rpc` - JSON-RPC 2.0
- `POST /boot` - Bootstrap webhook (Mailgun)
