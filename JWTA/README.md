# JWTA

JWT Web Auth service.

## Structure

```
JWTA/
├── bin/      # Built executables
├── doc/      # Documentation
├── inc/      # Headers
├── lib/      # Libraries
├── src/      # Source code
└── tmp/      # Build artifacts (gitignored)
```

## Purpose

Standalone authentication service for pqtr ecosystem.

- OAuth 2.0 integration (Google, GitHub)
- JWT issuance and refresh
- User management
- Stateless token validation (consumers verify signature only)
