# Next Steps

## Context

Read this file after `README.md` to understand current LABS development state.

**BASE** = 2a0b3b8 (2025-12-06)

---

## Current: High ISO Images (Flat & Grainy)

DSC01559 (ISO 2000) looks flat and grainy compared to DSC00202 (ISO 100).

| Image | ISO | Result |
|-------|-----|--------|
| DSC00202 | 100 | Good - proper tone and color |
| DSC01559 | 2000 | Flat and grainy |

### Root Cause

Sony's embedded JPEG has noise reduction applied. Our pipeline matches *color/tone* but can't replicate *noise reduction*:

```
Camera JPEG (ISO 2000):
  RAW → NR → tone/color → clean preview

Our pipeline:
  RAW → tone/color → noisy output
           ↑
     matched to clean target
```

The optimizer reduces contrast to hide noise → "flat" appearance.

### Options

1. **Apply denoise before optimization** - fixed NR before matching
2. **Include denoise dials in optimization** - learn NR per image
3. **ISO-aware processing** - baseline denoise scaled by ISO
4. **Different loss function** - tolerate noise in features

### Recommended

**ISO-aware + denoise dials:**
1. Read ISO from EXIF
2. Apply baseline denoise: `strength = log2(ISO / 100) * 0.1`
3. Include denoise dials in optimization for fine-tuning

---

## Completed This Session

- [x] Output file structure: base.png, view.png, 0.pipe.png, tail.png, diff.png
- [x] Fixed RGB/BGR issue in tail.save() (pipeline is BGR native)
- [x] Work area stays unchanged during tune
- [x] Tail saves at 2048px (social size) for fast iteration
- [x] Full res available via Export

---

## Backlog

| Priority | Issue | Notes |
|----------|-------|-------|
| **P1** | High ISO flat/grainy | See above |
| P2 | DRO spatial effects | 12-16% error on foliage |
| P3 | Canon CR2/CR3 support | RAWS expansion |
| P3 | Nikon NEF support | RAWS expansion |

---

## Architecture (Working)

```
RAWS (decode) → Pipe HEAD → BODY (links) → TAIL (sigmoid/gamma)
```

- **RAWS:** Pure decode → scene-linear RGB + embedded preview
- **Pipe HEAD:** Holds decoded data (base + view)
- **Pipe BODY:** Links process scene-linear (45 dials)
- **Pipe TAIL:** Sigmoid → Gamma → Save at 2048px
