# VIEW: Calibration Todo

[back](./view.md)

Task list for VIEW calibration images and implementation.

---

## Current State

We have 10 ARW files in `var/pics/`, all shot with:
- Creative Style: Standard
- DRO: Auto
- Contrast/Saturation/Sharpness: Normal (0)

**This is enough to start.** We can calibrate "Standard + DRO Auto" immediately.

---

## Phase 1: Lab Rat (Now)

Using existing images only.

- [ ] Extract embedded JPEG from one test ARW
- [ ] Build view_extract tool (exiftool wrapper or native)
- [ ] Process ARW through raws → pipe with neutral dials
- [ ] Compare pipe output to embedded JPEG visually
- [ ] Run diff to get baseline spectral loss
- [ ] Run tune/SPSA to find dials that minimize loss
- [ ] Store result as `a7iii_standard_dro_auto.json`
- [ ] Validate: apply stored dials, confirm low spectral loss

---

## Phase 2: Expand Styles (Future)

Calibration images needed. Shoot each with:
- Same scene (controlled lighting, color checker optional)
- RAW+JPEG mode
- Tripod for consistency

### Required Shots

| Creative Style | DRO | Priority | Notes |
|---------------|-----|----------|-------|
| Standard | Off | High | Deterministic baseline |
| Vivid | Auto | Medium | Common "pop" look |
| Vivid | Off | Medium | |
| Portrait | Auto | Low | Skin tone optimized |
| Landscape | Auto | Low | Saturated greens/blues |
| Neutral | Auto | Low | Flat for grading |

### Nice to Have

| Creative Style | DRO | Notes |
|---------------|-----|-------|
| Clear | Auto | A7III specific |
| Deep | Auto | A7III specific |
| Light | Auto | A7III specific |
| Sunset | Auto | Warm tones |
| Night Scene | Auto | Low-light optimized |

### Adjustment Matrix (per style)

For complete coverage, each style needs shots at adjustment extremes:

| Contrast | Saturation | Sharpness |
|----------|------------|-----------|
| -3 | 0 | 0 |
| +3 | 0 | 0 |
| 0 | -3 | 0 |
| 0 | +3 | 0 |
| 0 | 0 | -3 |
| 0 | 0 | +3 |

This validates the offset mapping. Lower priority - start with defaults (0,0,0).

---

## Phase 3: Multi-Camera (Future)

- A7III (current)
- A7IV
- A7C
- A6xxx series (APS-C)

Different cameras may have different Creative Style implementations. Each needs separate calibration.

---

## Reference Image Suggestions

For calibration shots:

1. **Color checker** - X-Rite ColorChecker or similar, neutral lighting
2. **Skin tones** - Portrait with caucasian/asian/african subjects
3. **Landscape** - Foliage, sky, water (saturated natural colors)
4. **High contrast** - Backlit scene, tests DRO and highlight handling
5. **Low key** - Dark scene, tests shadow handling
6. **Neutral gray** - 18% gray card for exposure reference

Not all needed immediately - color checker alone gets you far.

---

## Notes

- Embedded JPEG is 1616x1080 - sufficient for spectral loss
- Sidecar JPG is full resolution - use if higher precision needed
- Multiple images per setting improve robustness (average the calibrated dials)
