# Next Steps

## Context

Read this file after `README.md` to understand current LABS development state.

**BASE** = bc8304e (2025-12-05)

---

## Current: Pink Color Cast

LABS output shows pink/magenta tint on neutral subjects.

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
RAWS (decode) → Pipe HEAD → BODY (links) → TAIL (sigmoid/gamma)
```

- **RAWS:** Pure decode → scene-linear RGB + embedded preview
- **Pipe HEAD:** Holds decoded data
- **Pipe BODY:** Links process scene-linear (45 dials)
- **Pipe TAIL:** Sigmoid → Gamma → Display

---

## Completed

1. ✓ Remove sigmoid from view.cpp - moved to pipe module
2. ✓ Design clean separation - RAWS = decode only
3. ✓ Create dark.json neutral vibe
4. ✓ Remove curve/poly estimation from RAWS (belongs in LABS/TUNE)

---

## Parked

- Darktable baseline validation (blocked by color cast)
- VIBE neural prediction
- Per-image optimization
