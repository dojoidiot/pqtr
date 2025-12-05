# Next Steps

## Context

Read this file after `README.md` to understand current LABS development state.

**BASE** = bc8304e (2025-12-05)

---

## Current Issue: Pink Color Cast

LABS output shows pink/magenta tint on neutral subjects (metal appears pink instead of gray).

### Known Facts
- Camera preview (embedded JPEG): gray metal ✓
- Darktable output: gray metal ✓
- LABS output: pink metal ✗

### Root Cause
RGB/BGR channel mismatch somewhere in pipeline. Not yet located.

### Next Action
Methodically trace channel order through pipeline:
1. Verify RAWS demosaic output channel order
2. Verify color matrix preserves order
3. Verify sigmoid preserves order
4. Verify imwrite receives correct order

Do NOT attempt fixes until exact swap location is found.

---

## Architecture (Working)

```
RAWS (decode) → Pipe (links) → Sigmoid → Gamma → Display
```

- **RAWS:** Pure decode → scene-linear RGB
- **Pipe:** Links process scene-linear
- **Sigmoid:** Scene→display tone mapping (in pipe.cpp)
- **View:** Pure sRGB gamma encoding only

---

## Completed

1. ✓ Remove sigmoid from view.cpp - moved to pipe module
2. ✓ Design clean separation - RAWS = decode only
3. ✓ Create dark.json neutral vibe

---

## Parked

- Darktable baseline validation (blocked by color cast)
- VIBE neural prediction
- Per-image optimization
