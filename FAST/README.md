# PQTR:FAST

[back](../README.md)

Minimal Android app for field shooters on custom Android hardware. Captures RAW files and transfers them over WiFi for later processing.

## Role in PQTR

FAST is for rapid field capture. It runs on a custom Android build optimized for speed, not processing. Images are transferred to a desktop running DESK for actual editing.

```
Camera ──► [FAST] ──► FTP/WiFi ──► Desktop
             │
             └── Capture only, no processing
```

- **Platform**: Custom Android build
- **Purpose**: Rapid RAW capture and transfer
- **Processing**: None (handled by DESK later)

## Concept

| Feature | Description |
|---------|-------------|
| Capture | Trigger camera shutter, receive RAW file |
| Transfer | FTP over WiFi to desktop/server |
| Preview | Embedded JPEG only (no processing) |
| Storage | Local buffer until transfer confirmed |

## Why Custom Android?

Standard Android apps have latency and permission issues for direct camera control. FAST runs on a custom build that:

- Direct hardware access to camera
- Minimal OS overhead
- Reliable WiFi/FTP stack
- Single-purpose interface

## Workflow

1. Shoot with tethered camera
2. RAW files appear on FAST device
3. Auto-transfer over WiFi to desktop
4. Process later with DESK

## Status

**Planned** - Not yet implemented.

## Dependencies

- Custom Android build
- LABS pipe library (native, for future local preview)
- FTP client library
