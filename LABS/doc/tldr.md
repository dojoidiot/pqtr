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

## Jacobian: Dial→Feature Sensitivity (45×23)

The Jacobian matrix J[d][f] measures how much feature f changes when dial d moves by 1 unit. Computed via central difference (±5% perturbation from neutral).

**Uses:**
1. **Gradient-informed optimization** - take steps in high-impact directions
2. **Feature weight adjustment** - low-sensitivity features are unreachable
3. **Understanding dial→feature relationships**

**Key file:** `etc/jacob.json` (45×23 matrix with dial/feature names)

## Current Results (2024-12-01)

With base curve + baseline guard + 3D LUT:

| Image | Baseline | Final |
|-------|----------|-------|
| DSC00144 | 13.16% | 12.96% |
| DSC00159 | 4.04% | 1.82% |
| DSC00202 | 6.22% | 2.59% |
| DSC00234 | 6.73% | ~5% |
| DSC01531 | ~36% | ~16% |

**Summary:**
- Most images: baseline < 7%, final < 5%
- Base curve handles per-channel tone mapping
- 3D LUT captures nonlinear color relationships
- Baseline guard prevents optimizer from degrading quality

**DSC01531 outlier:** Complex cross-channel colors (saturated greens/reds) that per-channel curves can't capture. Root cause: per-channel tone curves shift hue on saturated colors.

## Why We Can't Match Perfectly

The 5% residual loss comes from:

| Cause | Contribution | Fix |
|-------|-------------|-----|
| Per-channel curves shift hue | 50% | Luminance-based tone mapping |
| 3D LUT limited resolution | 30% | Higher grid or tetrahedral interp |
| DRO spatial variation | 15% | Local tone mapping (out of scope) |
| Color matrix, alignment | 5% | Validation against darktable |

**Quick wins**: Neutral-pixel curve estimation and luminance-preserving tone mapping. See [todo.md](./todo.md#strategic-analysis-path-to-camera-parity-2024-12-01) for full analysis.

## Direct LUT Experiment (2024-12-01)

**Hypothesis**: Camera matching is measurement, not optimization. A single 33³ LUT measured directly from flat→JPEG should achieve near-zero loss.

**Result**: Direct LUT performs **worse** than the current pipeline:

| Image | Direct LUT | Current Pipeline |
|-------|------------|------------------|
| DSC00144 | 7.0% | ~5% |
| DSC01531 | 18.8% | 16% |

**Why it fails**: 96% of LUT cells are empty. Scene-linear data clusters in low value range (83% below 0.33 after gamma). Uniform 33³ grid wastes cells on unused RGB regions.

**Conclusion**: The measurement hypothesis is correct, but the current two-phase architecture (base curve → dials → small LUT) is more efficient than a single large LUT. Base curves capture the dominant 1D tone transforms, leaving only 3D color shifts for the LUT. See [todo.md](./todo.md#direct-lut-experiment-2024-12-01) for details.

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
| `etc/jacob.json` | Jacobian matrix (45×23 dial→feature sensitivity) |
| `src/test/geos/jacob.cpp` | Jacobian estimation tool |
