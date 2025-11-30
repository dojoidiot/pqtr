# LABS TLDR

## System Overview

We have **45 dials** that edit image appearance:
- 3 color correction (exposure, temperature, tint)
- 7 tone mapping (contrast, highlights, shadows, toe, shoulder, black, white)
- 3 global color (vibrance, saturation, density)
- 4 split tone (shadow temp/tint, highlight temp/tint)
- 24 selective color (8 hues × 3 HSL)
- 4 detail (sharpen amount/radius, denoise luma/chroma)

Processing is split into:
- **Scene-linear link** (5 dials): exposure, WB, clipping - physics-based
- **Display link** (36 dials + 17³ LUT): tone curves, color grading - perceptual

## Feature Space (19D)

Images are reduced to a **19-dimensional feature vector**:

```
[0-2]   σ₁, σ₂, σ₃       # SVD singular values (energy distribution)
[3]     μ_L               # Mean luminance (brightness)
[4]     μ_C               # Mean chroma (saturation)
[5]     std_L             # Luminance std (CONTRAST - critical)
[6]     std_C             # Chroma std (saturation spread)
[7]     skew_L            # Luminance skewness (high-key vs low-key)
[8-9]   cov_LC, cov_HC    # Correlations (color harmony)
[10-11] μ_a, μ_b          # Lab a*/b* means (COLOR CAST - critical)
[12-15] L_p10, L_p25, L_p75, L_p90  # Luminance percentiles (TONE CURVE)
[16-17] C_p50, C_p90      # Chroma percentiles (saturation level)
[18]    C_shadow          # Shadow chroma (preserve color in darks)
```

## Loss Function

**Weighted L2 loss** (not geodesic):

```
Loss = Σ weights[i] × (feature[i] - target[i])²
```

Weights are trained via batch analysis. Critical features (std_L, percentiles, color cast) have high weights (5.0).

## Optimizers

| Optimizer | Strategy | Use Case |
|-----------|----------|----------|
| **SPSA** | Gradient-free, phased exploration | Default, builds covariance |
| **ACEO** | CMA-ES eigenspace from prior | Fast when covariance known |
| **HYBRID** | ACEO direction → SPSA polish | Best of both |

## Trained Artifacts

| File | Purpose |
|------|---------|
| `etc/cnst.json` | Feature weights (19 values) |
| `etc/prms.json` | SPSA phase params (a0, c0 per block) |
| `etc/cvar.json` | 45×45 covariance matrix for ACEO |

## Current Status

**Discovery:** The "washed out" problem is a pipeline capability gap, not optimizer issue.

```
Target std_L:   0.2244 (camera JPEG contrast)
Max achievable: 0.1303 (our dials at extremes)
Gap:            42% unreachable
```

**Root cause:** Camera JPEGs apply a base tone curve (per picture style) BEFORE adjustments. We start from flat baseline.

**Next step:** Learn per-camera base curves to expand achievable range. See `doc/base_curve.md`.

## Training Tools

```bash
make -f Makefile.tune bounds         # Check achievable feature bounds
make -f Makefile.tune sweep          # Single-dial loss landscape
make -f Makefile.tune train-greedy   # Worst-case analysis
make -f Makefile.tune train-prms     # Phase param grid search
make -f Makefile.tune batch-tune     # Run tune on all images
```

## Key Insight

The base curve represents **photographer intent** - the camera preview is what they SAW when composing and what they EXPECT as editing baseline. Matching it restores their creative intent.
