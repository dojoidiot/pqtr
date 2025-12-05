# Next Steps

## Context

Read this file after `README.md` to understand current LABS development state.

**BASE** = e90c188 (2025-12-05)

---

## Current Direction: Clean Architecture

**Goal:** Pure separation - RAWS decodes, Pipe processes, vibes configure.

### Architecture Insight

The pipeline drifted. Need to refactor:

**Current (messy):**
- RAWS: decode + some processing baked in
- view.cpp: sigmoid baked into display conversion
- Processing scattered across layers

**Target (clean):**
- RAWS: pure decode only (libraw replacement) → scene-linear RGB out
- Pipe: ALL processing via configurable links
  - Modules handle everything: exposure, WB, sigmoid, tone mapping
  - DARK vibe = dial settings that match darktable defaults
- View: pure sRGB gamma encoding, nothing else

### Why

- RAWS should be a clean-room libraw replacement, nothing more
- All "look" decisions belong in the pipe as configurable dials
- A "darktable look" is just a vibe (preset), not special code
- Easier to reason about: decode → process → display

---

## Findings from Darktable Comparison

Tested LABS vs darktable on same RAW:

| Metric | Without Vibe | With +1.2 EV Vibe |
|--------|--------------|-------------------|
| Mean diff | 15.9% | 17.4% |
| Lum ratio | 0.678 (darker) | 1.103 (brighter) |

The issue isn't just exposure - there are fundamental differences in:
1. What RAWS outputs (may have processing baked in)
2. Sigmoid implementation details
3. Color matrix / white balance handling

---

## Immediate Next Steps

1. **Audit RAWS** - what processing is currently embedded?
2. **Remove sigmoid from view.cpp** - move to pipe module
3. **Design clean separation** - RAWS = decode only
4. **Create DARK vibe** - dial settings matching darktable defaults

---

## Parked

- VIBE neural prediction (per-camera vibes may be sufficient)
- Per-image optimization (DRO is spatially-varying, can't match globally)

---

## Key Files

| File | Purpose |
|------|---------|
| `src/main/part/pipe/view.cpp` | Display conversion (has sigmoid - to remove) |
| `src/main/part/pipe/mods/sigmoid.cpp` | Sigmoid module (keep) |
| `inc/RAWS/raws.hpp` | RAWS interface (audit for embedded processing) |
| `etc/vibes/darktable.json` | Darktable-matching vibe (WIP) |

## Test Commands

```bash
cd LABS

# Generate darktable reference
../dark/lib/dark/bin/darktable-cli \
  image.ARW tmp/darktable_output.png

# LABS render
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/labs \
  image.ARW --output tmp/labs_output.png

# Compare
compare tmp/labs_output.png tmp/darktable_output.png tmp/diff.png
```
