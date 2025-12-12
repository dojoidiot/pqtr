# BASE

Web server for PQTR - static site serving + JWT authentication + file storage.

## Structure

```
BASE/
├── bin/base              # Server executable
├── inc/                  # Headers
│   ├── base.hpp          # Service API
│   ├── data.hpp          # Store interface
│   ├── mail.hpp          # Mailer interface
│   └── itag.hpp          # User ID generation
├── src/main/
│   ├── base.cpp          # Entry point (main, config, httplib)
│   └── part/
│       ├── http.cpp      # Service handlers
│       ├── crypto.cpp    # libsodium wrapper
│       └── jwt.cpp       # JWT encode/decode
├── src/main/plug/
│   ├── sqlite.cpp        # Store implementation
│   ├── mailgun.cpp       # Mailer (production)
│   └── console.cpp       # Mailer (test mode)
├── www/                  # Static site + WASM app
└── var/                  # Runtime data (gitignored)
```

## Features

- Static file serving
- OTP email authentication (Mailgun)
- JWT tokens (EdDSA signed)
- User roles: NONE, PLAY, HERO, PQTR
- Binary file upload (`/push` endpoint)
- JRPC API for auth and file management

## Security

- Path traversal prevention (validated path components)
- Rate limiting: 3 OTP requests / 10 min, 5 verify attempts / OTP
- Constant-time OTP comparison (sodium_memcmp)
- No shell commands with user input (httplib for mail)
- JWT claims escaped, itag validated before file ops
- Bootstrap token one-time use only
