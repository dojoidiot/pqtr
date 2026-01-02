# Pipeline Tuning Guide

## Purpose
Document objective functions and tunable parameters for camera-adaptive pipeline optimization.

## Verification Status
- **filmicrgb.c**: VERIFIED - No structural bugs. Spline pre-computed for black_source=-5.0, toe lifts blacks to 7.6%. Code matches DT.
- **colorbalancergb.c**: VERIFIED - Contrast and shadow controls work as expected.
- **Shadow crush**: TUNING issue, not code bug. Confirmed by successful lift with contrast adjustment.

## Objective Functions

### 1. Overall Brightness Match
```
metric: |mean(gold) - mean(reference)|
target: < 1.0 levels (8-bit)
current: ~0.5 levels (tuned via exposure_bias)
```

### 2. Shadow Preservation
```
metric: count(pixels < threshold) ratio
formula: gold_dark_pixels / ref_dark_pixels
target: 1.0x (equal shadow density)
current: ~2x (gold has 2x more crushed pixels)
threshold: mean < 10 (8-bit)
```

### 3. Shadow Level Match
```
metric: mean(gold[dark_mask]) - mean(ref[dark_mask])
where: dark_mask = ref.mean(axis=2) < 40
target: 0.0
current: -4.0 to -10.0 (gold shadows darker)
```

### 4. Per-Channel Balance
```
metric: per-channel mean difference (R, G, B)
target: balanced difference across channels
notes: Red channel shows more crush in shadows
```

## Tunable Parameters

### Camera-Specific (in sony.c or per-camera file)
| Parameter | Current | Range | Effect |
|-----------|---------|-------|--------|
| exposure_bias | 1.05 EV | 0.5-2.0 | Overall brightness |
| d65_coeffs[3] | from matrix | computed | White balance |

### colorbalancergb.c
| Parameter | Current | Range | Effect |
|-----------|---------|-------|--------|
| contrast | 0.80 | 0.5-1.5 | Shadow lift vs highlight compression |
| shadows_weight | 4.0 | 1.0-8.0 | Shadow mask falloff |
| shadows[0-3] | 1.0 | 0.5-1.5 | Per-channel shadow lift |
| midtones_Y | 1.0 | 0.5-1.5 | Midtone gamma |
| grey_fulcrum | 0.1845 | 0.1-0.3 | Contrast pivot point |

### filmicrgb.c (COUPLED - do not change independently)
| Parameter | Current | Notes |
|-----------|---------|-------|
| black_source | -5.0 | Spline computed for this value |
| spline.* | from DT | Pre-computed, interdependent |

## Optimization Strategy: CMA-ES

Covariance Matrix Adaptation Evolution Strategy - appropriate because:
- Parameters are covariant (contrast ↔ exposure_bias coupling observed)
- Non-convex objective landscape (trade-off surfaces)
- Derivative-free (no backprop through pixel pipeline)
- ~5-10 continuous parameters (CMA-ES sweet spot)

### Parameter Vector x (Full Space)

**Camera/Exposure (2 params)**
```
exposure_bias         # EV, range [-1.0, 3.0]
white_fulcrum         # range [0.5, 2.0]
```

**colorbalancergb - Global (6 params)**
```
contrast              # range [0.5, 1.5] - Y contrast around grey_fulcrum
midtones_Y            # range [0.5, 1.5] - Y gamma
grey_fulcrum          # range [0.05, 0.5] - contrast pivot point
shadows_weight        # range [1.0, 12.0] - shadow mask falloff
highlights_weight     # range [1.0, 12.0] - highlight mask falloff
midtones_weight       # range [2.0, 16.0] - midtone mask width
```

**colorbalancergb - Per-Channel Lift/Gain/Gamma (12 params)**
```
global[4]             # range [-0.1, 0.1] - per-channel offset (R,G,B,unused)
shadows[4]            # range [0.5, 1.5] - per-channel shadow multiplier
highlights[4]         # range [0.5, 1.5] - per-channel highlight multiplier
midtones[4]           # range [0.5, 1.5] - per-channel midtone power
```

**colorbalancergb - Chroma/Saturation/Brilliance (15 params)**
```
chroma_global         # range [-0.5, 0.5]
chroma[4]             # range [-0.5, 0.5] - per-region chroma
vibrance              # range [-0.5, 0.5]
saturation_global     # range [-0.5, 0.5]
saturation[4]         # range [-0.5, 0.5] - per-region saturation
brilliance_global     # range [-0.5, 0.5]
brilliance[4]         # range [-0.5, 0.5] - per-region brilliance
```

**colorbalancergb - Hue (1 param)**
```
hue_angle             # range [-π, π] - global hue rotation
```

**Total: 36 tunable parameters**

Bounds enforce photographic validity - values outside these produce artifacts.

### Parameter Groups

For staged optimization or dimensionality reduction:

| Group | Params | Controls |
|-------|--------|----------|
| brightness | exposure_bias, contrast, midtones_Y | overall tone |
| shadows | shadows[4], shadows_weight, brilliance[0] | shadow response |
| highlights | highlights[4], highlights_weight, brilliance[2] | highlight response |
| color | chroma_*, saturation_*, hue_angle | color character |
| channel | global[4], midtones[4] | per-channel fine tune |

### Objective Function f(x)

```
f(x) = w1 * brightness_error(x)
     + w2 * shadow_ratio(x)
     + w3 * channel_imbalance(x)

where:
    brightness_error = |mean(gold) - mean(ref)| / 255
    shadow_ratio = |log(dark_pixels_gold / dark_pixels_ref)|
    channel_imbalance = std([R_diff, G_diff, B_diff]) / 255
```

Weights w1, w2, w3 set relative importance. Start with equal weights, adjust based on visual inspection.

### Convergence Measure

Cosine similarity between x and x*:

```
cos(θ) = (x · x*) / (|x| |x*|)
```

Measures directional alignment independent of parameter scale. Converged when cos(θ) > 0.99 across iterations.

### Covariance Learning

CMA-ES learns Σ (covariance matrix) capturing parameter relationships:

```
Observed coupling:
    contrast ↓  →  overall brightness ↑
    exposure_bias ↓  →  overall brightness ↓

Σ will learn: Cov(contrast, exposure_bias) < 0
    → search along the trade-off manifold
```

### Implementation Notes

```python
import cma

def objective(x):
    # x = [exposure_bias, contrast, ...]
    run_pipeline(x)  # writes tmp/var/gold.png
    return compute_metrics(gold, reference)

x0 = [1.05, 1.0, 4.0, 1.0, 0.1845]  # current values
sigma0 = 0.1  # initial step size
bounds = [[0.5, 0.5, 1.0, 0.8, 0.1],
          [2.0, 1.5, 8.0, 1.2, 0.3]]

es = cma.CMAEvolutionStrategy(x0, sigma0, {'bounds': bounds})
es.optimize(objective)
```

### Two-Stage Learning

#### Stage 1: Camera Baseline (Manufacturer Intent)

Learn what the camera manufacturer considers "correct" for this sensor.

**Reference**: In-camera JPEG embedded in RAW or shot as RAW+JPEG
**Target**: Match manufacturer's default processing

```
Input:  RAW file
Target: Embedded JPEG (or paired JPEG from RAW+JPEG)
Learn:  x_camera = argmin f(x | RAW, JPEG_manufacturer)
```

This captures:
- Camera's default tone curve
- Manufacturer's color science (Sony look, Canon look, Fuji look)
- Sensor-specific shadow/highlight handling
- Default saturation and vibrance

**Per-camera profiles**: Run once per camera model, store x_camera in database.

#### Stage 2: User Vibes (Personal Style)

Learn user's editing preferences from their Lightroom exports.

**Reference**: User's edited Lightroom exports
**Target**: Match user's aesthetic choices

```
Input:  RAW files (multiple)
Target: Corresponding Lightroom exports
Learn:  x_user = argmin Σ f(x | RAW_i, LR_export_i)
```

This captures:
- User's preferred contrast curve
- Color grading tendencies (warm/cool, saturated/muted)
- Shadow/highlight preferences
- Consistent "look" across their work

**Training set**: 20-50 RAW+export pairs spanning different scenes (portraits, landscapes, low-light, etc.)

#### Combined Profile

```
x_final = x_camera + Δx_user

where:
    x_camera = camera baseline (Stage 1)
    Δx_user  = user deviation from baseline (Stage 2 - Stage 1)
```

Or learn Stage 2 starting from x_camera as x0:

```python
# Stage 1: Camera baseline
x_camera = cma_optimize(f, x0=defaults, reference=jpeg_manufacturer)

# Stage 2: User style (warm start from camera baseline)
x_user = cma_optimize(f, x0=x_camera, reference=lightroom_exports)
```

### Reference Image Sources

| Source | Use | Notes |
|--------|-----|-------|
| Embedded JPEG | Camera baseline | Extract with exiftool -b -PreviewImage |
| RAW+JPEG pair | Camera baseline | Higher quality than embedded preview |
| Lightroom export | User vibes | Full-res TIFF or JPEG export |
| DT reference | Pipeline validation | darktable-cli output |

### Profile Storage

```
profiles/
├── cameras/
│   ├── sony_ilce-7m3.json      # x_camera for Sony A7 III
│   ├── canon_eos-r5.json       # x_camera for Canon R5
│   └── fuji_x-t5.json          # x_camera for Fuji X-T5
└── users/
    ├── user_alice.json         # x_user for Alice's style
    └── user_bob.json           # x_user for Bob's style
```

Load at runtime: camera profile from EXIF, user profile from config.

## Current Tuning State

### Sony ILCE-7M3
```
exposure_bias: 1.05 EV (tuned)
contrast: 0.80 (testing - lifts shadows but increases overall brightness)
d65_coeffs: 2.6715, 1.0, 1.3470 (from cameras.xml)
```

### Trade-offs Discovered
- Lowering contrast lifts shadows but increases overall brightness
- May need exposure_bias adjustment to compensate
- Shadow lift and brightness are coupled through the tone curve
