# DESK Documentation Standards

[back](./desk.md)

## Purpose

This document defines DESK-specific documentation standards. All DESK documentation follows [LABS/doc/docs.md](../../LABS/doc/docs.md) as the base standard.

---

## DESK-Specific Terminology

In addition to LABS terminology, DESK uses:

| Term | Definition |
|------|------------|
| **root folder** | User-selected directory containing RAW files and sidecars |
| **project** | A RAW file with its associated sidecar files |
| **sidecar** | JSON file storing project or pipe state |
| **Projects panel** | UI panel listing all projects |
| **Link editor** | UI panel for editing modules and dials |

---

## File Naming

| Pattern | Purpose |
|---------|---------|
| `<name>.desk.json` | DESK project settings |
| `<name>.pipe.json` | Pipe configuration |
| `<name>.png` | Output image |

Where `<name>` is the RAW filename without extension.

---

## Cross-References

DESK docs reference LABS docs for:

- Pipe architecture → [LABS/doc/pipe.md](../../LABS/doc/pipe.md)
- Data formats → [LABS/doc/data.md](../../LABS/doc/data.md)
- Module details → [LABS/doc/mods/](../../LABS/doc/mods/)

Use relative paths for all cross-references.
