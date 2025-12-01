# LABS TLDR

## Core Insight: Photographers Select Based on Style

Photographers don't see flat RAW. They see the camera's styled preview:

1. **Compose** while looking at LCD (with picture style applied)
2. **Expose** based on what they see (styled histogram, styled highlights)
3. **Select** keepers based on that appearance
4. **Edit** as adjustments TO what they chose, not FROM scratch

**The camera JPEG isn't "a reference" - it's the photographer's intent.**

This drives the entire architecture: **Style first, then tweaks.**

---

## Two-Layer Architecture: Style + Tweaks

The pipeline separates **style** (automatic, from reference) from **tweaks** (per-image adjustments):

```
Reference Image (camera JPEG or edited photo)
         ↓
    [RAWS extracts style transforms]
         ↓
    Color Matrix (3x3) + Base Curve (1D×3)
         ↓
    [Style applied - already close to reference]
         ↓
    45 Dials (fine-tune per-image)
         ↓
    Output
```

**Key insight**: When matching camera JPEGs, dials converge to neutral (0.5) because the style transforms already capture the look. When matching edited photos, dials will deviate to capture the photographer's creative choices.

## Style Transforms (Automatic)

Extracted by RAWS from RAW→reference comparison:

| Transform | Purpose | Captures |
|-----------|---------|----------|
| **Color Matrix (3x3)** | Cross-channel color | Hue rotation, color grading |
| **Base Curve (1D×3)** | Per-channel tone | Contrast, brightness, channel balance |

These are **deterministic** - same camera + style = same transforms.

## Tweaks (45 Dials)

For per-image adjustments on top of the style:

- 3 color correction (exposure, temperature, tint)
- 7 tone mapping (contrast, highlights, shadows, toe, shoulder, black, white)
- 3 global color (vibrance, saturation, density)
- 4 split tone (shadow temp/tint, highlight temp/tint)
- 24 selective color (8 hues × 3 HSL)
- 4 detail (sharpen amount/radius, denoise luma/chroma)

## Feature Space (23D)

Images are compared via a **23-dimensional feature vector**:

```
[0-2]   σ₁, σ₂, σ₃           # SVD singular values
[3-4]   μ_L, μ_C             # Mean luminance, chroma
[5-6]   std_L, std_C         # Contrast, saturation spread
[7]     skew_L               # Tone asymmetry
[8-9]   cov_LC, cov_HC       # Correlations
[10-11] μ_a, μ_b             # Global color cast
[12-15] L_p10..L_p90         # Tone curve percentiles
[16-17] C_p50, C_p90         # Saturation percentiles
[18]    C_shadow             # Shadow chroma
[19-20] a_shadow, b_shadow   # Shadow color (split tone signal)
[21-22] a_highlight, b_highlight  # Highlight color
```

## Current Results (2024-12-01)

With base curve + baseline guard:

| Baseline Range | Count | Final |
|----------------|-------|-------|
| 3-5% | 3 | Same (guard preserves) |
| 5-13% | 6 | Improved |
| 35% | 1 | 16% (DSC01531 outlier) |

**Baseline guard:** Optimizer never degrades quality. If combined loss improves but spectral worsens, neutral dials are restored.

**DSC01531 outlier:** Complex cross-channel colors (saturated greens/reds) that per-channel curves can't capture. Needs color matrix estimation.

## Architecture

```
RAWS:
  1. Decode RAW → scene-linear data
  2. Extract embedded JPEG → preview
  3. Estimate colorMatrix[9] from data→preview (cross-channel)
  4. Estimate baseCurve[768] from data→preview (BGR × 256)
  5. Return {data, preview, colorMatrix, baseCurve}

pipe::Link:
  colorCorrection → colorMatrix → baseCurve → toneMapping → ...
```

## Use Cases

**Camera JPEG matching** (current):
- Style = camera's picture style (Standard, Vivid, etc.)
- Dials ≈ 0.5 (neutral) - style does all the work

**Photographer style matching** (future):
- Style = base transforms learned from edited examples
- Dials = creative adjustments the photographer made
- Can apply same style to new images

## Key Files

| File | Purpose |
|------|---------|
| `RAWS/src/main/raws.cpp` | Style extraction |
| `LABS/src/main/part/pipe/mods/color_matrix.cpp` | Matrix application |
| `LABS/src/main/part/pipe/mods/base_curve.cpp` | Curve application |
| `etc/cnst.json` | Feature weights (23 values) |
