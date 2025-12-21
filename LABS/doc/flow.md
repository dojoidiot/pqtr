# FLOW - Camera-Matching Pipeline

Flow is the new processing pipeline replacing pipe. It processes RAW files through
a 5-stage pipeline to match the camera's embedded JPEG.

## Pipeline Order (Canonical)

```
HEAD ──► TUNE ──► TINT ──► DRUM ──► VIBE ──► output
  │        │        │        │        │
  │        │        │        │        └── User dials
  │        │        │        └── Local tone map (CLAHE/DRO)
  │        │        └── 3D color LUT
  │        └── Global tone curve (1D)
  └── GPU RAW decode (BLC, WB, demosaic, CST, warp)
```

**Why this order?** Cameras apply global processing (tone curve, color) first,
then DRO lifts shadows locally on the already-processed image.

## Stages

### 1. HEAD - GPU RAW Decode

Scene-linear RGB from:
- Black level correction (BLC)
- White balance (WB)
- Demosaic (Bayer → RGB)
- Color space transform (CST)
- Lens warp correction

Output: float32 RGB [0,1+]

### 2. TUNE - Global Tone Curve

256-bin 1D luminance mapping learned from LUTE.

Sony stores 4 control points in EXIF:
```
Sony Tone Curve: 8000 10400 12900 14100
```
(Output values at 25%, 50%, 75%, 100% of 14-bit range)

### 3. TINT - 3D Color LUT

17^3 RGB lookup table learned from LUTE.
Handles Creative Style (Standard, Vivid, Portrait...).
Requires >70% cell coverage to avoid artifacts.

### 4. DRUM - Local Tone Mapping

CLAHE (Contrast Limited Adaptive Histogram Equalization).
Applied AFTER global processing to match camera DRO order.

**Matching mode:** When LUTE is trained from embedded JPEG, DRUM is skipped.
The learned curve already includes what DRO did - applying DRUM would double it.

**Standalone mode:** When no LUTE profile, apply DRUM based on EXIF DRO level:

| Level | clip_limit |
|-------|------------|
| Off   | skip       |
| Lv1   | 2.0        |
| Lv2   | 3.5        |
| Lv3   | 5.0        |
| Lv4   | 7.0        |
| Lv5   | 10.0       |
| Auto  | 5.0        |

### 5. VIBE - User Dials

Lightroom-style sliders for fine-tuning:
- Exposure, Contrast, Whites, Blacks → modifies TUNE
- Shadows, Highlights, Clarity → modifies DRUM
- Temperature, Tint, Vibrance, Saturation → modifies TINT

## Learning (LUTE)

LUTE learns camera profiles from RAW + embedded JPEG pairs:

```
HEAD output (scene-linear) ──► LUTE ──► TUNE curve + TINT lut
                      ▲
                      │
           embedded JPEG (target)
```

One profile per camera model + creative style.

## Files

```
inc/flow.hpp              - Flow API
inc/drum.hpp              - DRUM API
inc/tune.hpp              - TUNE API
inc/tint.hpp              - TINT API
inc/lute.hpp              - LUTE API

src/main/flow/flow.cpp    - Flow implementation
src/main/flow/part/       - Stage implementations
  head.cpp                - GPU RAW decode
  tune.cpp                - Tone curve
  tint.cpp                - 3D LUT
  drum.cpp                - CLAHE
  vibe.cpp                - User dials
  lute.cpp                - Learning bridge

src/main/lute/lute.cpp    - Camera profile learning

src/test/flow/flow.cpp    - Pipeline test with stage diffs
```

## Test Output

```
tmp/var/flow/
  DSC00144.0.ref.jpg       - Camera JPEG (target)
  DSC00144.1.head.png      - After HEAD
  DSC00144.2.tune.png      - After TUNE
  DSC00144.3.tint.png      - After TINT
  DSC00144.4.drum.png      - After DRUM
  DSC00144.5.vibe.png      - Final output
  DSC00144.*.diff.png      - Error vs reference (gray=match)
```

## References

- Apical Iridix: Sony's DRO is licensed Iridix technology
- CLAHE: Zuiderveld, Graphics Gems IV (1994)
- OpenCV: cv2.createCLAHE(clipLimit, tileGridSize)
- darktable: filmic, local contrast modules
