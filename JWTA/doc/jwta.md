# JWTA Protocol

## Rule

**JWTA = JWT Web Auth**

Dedicated authentication service with custodial ed25519 keys.

## Features

- Email OTP registration/login (no passwords)
- Custodial ed25519 keypair per user
- JWT with EdDSA signatures
- Secrets via Linux kernel keyring

## Architecture

```
┌─────────┐     ┌─────────┐     ┌─────────┐
│ Browser │     │  JWTA   │     │  SAAS   │
└────┬────┘     └────┬────┘     └────┬────┘
     │               │               │
     │── register ──>│               │
     │<── OTP email ─│               │
     │── verify ────>│               │
     │<── JWT ───────│               │
     │               │               │
     │──── JWT ──────────────────────>│
     │<──── JRPC ────────────────────│
```

## JRPC Endpoints

Single endpoint: `POST /rpc` (JSON-RPC 2.0)

### Methods

| Method | Params | Returns |
|--------|--------|---------|
| `register` | `email` | `{ok, expires}` |
| `verify` | `email, otp` | `{jwt, refresh_token, user_id, pubkey_hex}` |
| `login` | `email` | `{ok, expires}` |
| `refresh` | `refresh_token` | `{jwt}` |
| `pubkey` | `user_id` | `{pubkey_hex}` |

### Example

```json
{"jsonrpc":"2.0","method":"register","params":{"email":"user@example.com"},"id":1}
```

```json
{"jsonrpc":"2.0","result":{"ok":true,"expires":600},"id":1}
```

## JWT Format

### Header

```json
{
  "alg": "EdDSA",
  "typ": "JWT"
}
```

### Payload

```json
{
  "iss": "jwta.pqtr.io",
  "sub": "user_123",
  "email": "user@example.com",
  "tier": "registered",
  "iat": 1702000000,
  "exp": 1702086400
}
```

### Tiers

| Tier | Description |
|------|-------------|
| `anonymous` | No account, rate limited |
| `registered` | Email verified, personal storage |
| `pro` | Paid, higher limits |

## Token Lifetimes

| Token | Lifetime |
|-------|----------|
| Access JWT | 1 hour |
| Refresh token | 30 days |
| OTP | 10 minutes |

## Secrets

Secrets loaded from Linux kernel keyring (preferred) or environment:

| Key Name | Env Fallback | Description |
|----------|--------------|-------------|
| `jwta_master` | `JWTA_MASTER_KEY` | 32-byte master key (64 hex chars) |
| `jwta_smtp_pass` | `JWTA_SMTP_PASS` | SMTP password |

```bash
# Load at boot
keyctl add user jwta_master "00010203...1e1f" @s
```

## Config

`etc/jwta.json`:

```json
{
    "host": "127.0.0.1",
    "port": 8080,
    "db_path": "var/jwta.db",
    "smtp": {
        "host": "smtp.mailgun.org",
        "port": 587,
        "user": "postmaster@example.com",
        "from": "noreply@example.com"
    }
}
```

## Storage

SQLite database with tables:

- `users` - id, email, tier, pubkey, privkey_encrypted, privkey_salt
- `otps` - email, code, expires_at, purpose
- `refresh_tokens` - user_id, token_hash

## Encryption

User private keys encrypted at rest:

1. Per-user random salt (16 bytes)
2. BLAKE2b key derivation (master + salt)
3. XChaCha20-Poly1305 authenticated encryption
4. Stored as 104 bytes (ciphertext + nonce + tag)
