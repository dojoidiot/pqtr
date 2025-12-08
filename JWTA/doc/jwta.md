# JWTA Protocol

## Rule

**JWTA = JWT Web Auth**

Dedicated authentication service for pqtr ecosystem.

## Responsibilities

| JWTA does | JWTA doesn't |
|-----------|--------------|
| OAuth flow | Business logic |
| JWT issuance | File storage |
| Token refresh | RPC methods |
| User database | Image processing |

## Architecture

```
┌─────────┐     ┌─────────┐     ┌─────────┐
│ Browser │     │  JWTA   │     │  SAAS   │
└────┬────┘     └────┬────┘     └────┬────┘
     │               │               │
     │──── OAuth ───>│               │
     │<─── JWT ──────│               │
     │               │               │
     │──── JWT ──────────────────────>│
     │               │    (validate)  │
     │<──── JRPC ────────────────────│
```

## Endpoints

### OAuth

```
GET  /auth/google    → redirect to Google OAuth
GET  /auth/github    → redirect to GitHub OAuth
GET  /auth/callback  → OAuth callback, returns JWT
```

### Token

```
POST /auth/refresh   → exchange refresh token for new JWT
POST /auth/revoke    → invalidate refresh token
GET  /auth/keys      → public keys for JWT verification (JWKS)
```

## JWT Format

### Header

```json
{
  "alg": "RS256",
  "typ": "JWT",
  "kid": "key-id"
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
| `registered` | OAuth login, personal storage |
| `pro` | Paid, higher limits, API access |

## Token Lifetimes

| Token | Lifetime |
|-------|----------|
| Access JWT | 1 hour |
| Refresh token | 30 days |

## Validation

SAAS validates JWT without calling JWTA:

1. Fetch public keys from `/auth/keys` (cache)
2. Verify JWT signature with public key
3. Check `exp` not passed
4. Check `iss` matches expected
5. Extract `sub`, `tier` for authorization

## Storage

JWTA needs persistent storage for:

- User records (sub, email, tier, created)
- Refresh tokens (hashed)
- OAuth state tokens (temporary)

SQLite for MVP, PostgreSQL for scale.
