# Next Steps

## Context

Read this file after `README.md` to understand current LABS development state.

**BASE** = a1fb942 (2025-12-05)

---

## Current Direction: Reset & Darktable Alignment

**Goal:** Validate pipeline against darktable, shift to per-camera vibes.

### Strategic Shift (from recommendations.md)

1. **Stop** per-image optimization against embedded JPEGs
2. **Stop** neural dial prediction (parked for now)
3. **Start** per-camera calibration → fixed vibes
4. **Start** manual dial adjustment in DESK UI

### Why

- DRO (Dynamic Range Optimizer) is spatially-varying - no global transform can match it
- Per-image optimization was solving the wrong problem
- Industry approach: calibrate once per camera, user adjusts from baseline

### Darktable Integration

Built darktable in `dark/` (gitignored). Using CLI for reference renders:

```bash
# Process RAW with darktable defaults
/home/z/base/code/pqtr/dark/lib/dark/bin/darktable-cli \
  image.ARW output_dir/ \
  --out-ext png --verbose \
  --core --configdir /home/z/base/code/pqtr/dark/tmp/config
```

### Validation Plan

1. **Compare darktable vs LABS output** - same RAW, both with neutral settings
2. **Identify gaps** - where does our decode differ?
3. **Map darktable modules → LABS dials** - create translation table
4. **Manual validation in DESK** - can you match darktable output with the 45 sliders?

---

## Parked: VIBE Neural Prediction

MLP trained on 537 samples, 0.23% validation loss. Predictions are conservative (dials cluster near 0.5). Parked because:

- Per-image prediction may be wrong target
- Per-camera vibe (simple averaging) may be sufficient
- Can resurrect for scene classification later

---

## Key Architecture Points (validated)

| Aspect | Status |
|--------|--------|
| Pipeline order (RAWS→FLAT→VIEW→POPS) | ✓ Matches darktable |
| 45 dials coverage | ✓ Complete for Lightroom parity |
| DESK UI | ✓ Ready for manual adjustment |
| Scene-referred workflow | ✓ Implemented |

---

## Immediate Next Steps

1. Compare darktable default output vs LABS output
2. Create darktable module → LABS dial mapping
3. Test DESK with manual dial adjustment
4. Create per-camera `.vibe` from darktable style settings

---

## Key Files

| File | Purpose |
|------|---------|
| `doc/recommendations.md` | Pipeline strategy synthesis |
| `note.md` | Reset mode thinking |
| `dark/lib/dark/bin/darktable-cli` | Reference renderer |
| `DESK/bin/desk` | Manual dial adjustment UI |

## Test Commands

```bash
cd LABS

# LABS render (current pipeline)
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/labs \
  image.ARW --output tmp/labs_output.png

# Darktable render (reference)
../dark/lib/dark/bin/darktable-cli \
  image.ARW tmp/ --out-ext png --verbose \
  --core --configdir ../dark/tmp/config

# Compare outputs
compare tmp/labs_output.png tmp/image.png -compose src tmp/diff.png
```
