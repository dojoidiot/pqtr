# BASE

Base web operations - static site serving + JWT authentication.

## Structure

```
BASE/
├── bin/      # Built executables
├── doc/      # Documentation
├── inc/      # Headers
├── lib/      # Libraries
├── src/      # Source code
├── www/      # Static site files
└── tmp/      # Build artifacts (gitignored)
```

## Purpose

Unified web server for pqtr ecosystem:

- Static file serving (www/)
- JWT authentication service (JRPC endpoints)
- OAuth 2.0 integration (Google, GitHub)
- User management and roles
