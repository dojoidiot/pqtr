# Pipeline Analysis

[back](../README.md)

Empirical findings from batch processing and optimization research.

---

## Current Architecture

```
Linear RAW → ColorMatrix (3x3) → HueSatMap → ToneCurve → 3D LUT
```

The pipeline matches DCP (DNG Camera Profile) structure with an added 3D LUT for residual correction.

### Stage 1: Color Matrix
3x3 matrix maps camera gamut to standard colorspace on linear data.
- Mixes 32% Green into Red (richer earth tones)
- Reduces pure Green intensity to 80%

### Stage 2: HueSatMap
Hue-specific adjustments, particularly yellow-greens.
- Brightness reduced ~40%, Saturation reduced ~55%
- "Vibrant" Sony look comes from *removing* radioactive raw greens

### Stage 3: Tone Curve
Sigmoidal S-curve for contrast and dynamic range compression.
- Lifts blacks: input 0 → output 23
- Compresses highlights: input 255 → output 147

### Stage 4: 3D LUT (17³)
Residual correction for anything the parametric stages miss.

---

## Batch Results Summary

10 images processed, comparing `tail.png` (our output) to `tune.jpg` (camera JPEG target).

### Optimizer Behavior

| Image | Scene | HEAD→TAIL Change | Assessment |
|-------|-------|------------------|------------|
| DSC00144 | Bridge, overcast | Minor cooling | OK |
| DSC00159 | Opera House, blue sky | Slight desaturation | OK |
| DSC00202 | Bird in foliage | Green adjustment | OK |
| DSC00234 | Storefront | Minor changes | OK |
| DSC00235 | Dog indoors, warm light | Cooling | OK |
| DSC00458 | Bar interior, mixed light | Texture differences | OK |
| DSC00501 | Monument, bright day | Heavy desaturation | Over-corrected |
| **DSC00521** | **House, blue sky** | **Blue→Pink sky** | **BROKEN** |
| DSC01531 | Garden foliage | High-frequency noise | Marginal |
| DSC01559 | People indoors | Cooling | Wrong direction |

### Critical Failure: DSC00521

The sky shifted from blue to pink/magenta. The target JPEG has a normal blue sky. This is a pathological failure in the optimizer or LUT.

**Possible causes:**
1. LUT interpolation artifacts in blue/cyan region
2. SPSA stuck in local minimum sacrificing hue for overall loss
3. Spectral loss weights luminance over chroma

---

## Consistent Patterns in Diff Images

Diff images show `|tail - target| × 5` amplified error.

### 1. Sky Gradient Banding
**Observed in:** DSC00159, DSC00501, DSC00521

Magenta/purple bands appear at the top of sky regions where blue transitions to white.

**Indicates:**
- Highlight rolloff curve differs from camera
- Hue rotation near clipping point
- Possibly gamut mapping differences in near-white blues

### 2. Green Channel Prominence in Foliage
**Observed in:** DSC00202, DSC00234, DSC00501, DSC01531

Strong green differences in vegetation areas.

**Indicates:**
- Camera does hue-specific saturation (greens pushed toward yellow/vivid)
- Our HueSatMap may not match camera's exact hue targeting
- Current green dampening may be too aggressive or wrong hue range

### 3. Chromatic Edge Artifacts
**Observed in:** DSC00144, DSC00234, DSC00501, DSC00521

Red/cyan fringing on high-contrast edges.

**Indicates:**
- Sharpening approach differs (camera may be chroma-aware)
- CA correction mismatch between our decode and camera's
- Edge enhancement in camera JPEG we're not replicating

### 4. Shadow Color Casts
**Observed in:** DSC00235, DSC01559

Blue/purple tints in shadow regions.

**Indicates:**
- Camera lifts shadows while desaturating
- Our tone curve may lift without the desaturation component
- Cross-channel behavior in shadows differs

### 5. Indoor/Artificial Lighting
**Observed in:** DSC00458, DSC01559

Color temperature differences under non-daylight conditions.

**Indicates:**
- Camera may do scene-adaptive WB adjustments
- Our single-illuminant WB insufficient for mixed lighting
- Possible local WB correction in camera

---

## Optimizer Tendencies

Across the batch, the SPSA optimizer shows systematic biases:

### Desaturation Bias
Most tails are less saturated than heads, even when targets are vivid. The optimizer finds it easier to reduce saturation than match specific hues.

### Cool Temperature Shift
Consistent tendency toward cooler color temperature. DSC01559 went cooler when the target is clearly warmer.

### Hue Instability in Blues
Sky regions show the most instability. The 3D LUT may create unexpected interpolation results in the blue→cyan→white gradient space.

---

## Process Adjustment Candidates

Based on empirical evidence:

### High Priority
1. **Fix blue/sky handling** - DSC00521 pink sky is unacceptable
   - Investigate LUT values in blue region
   - Add hue constraints to prevent blue→magenta shifts
   - Consider separate sky/highlight handling

2. **Chroma-aware sharpening** - Edge artifacts indicate our sharpening operates on luminance+chroma together when it should be luminance-only or chroma-preserving

### Medium Priority
3. **Hue-specific saturation refinement** - Green handling needs tuning
   - May need to shift target hue range for dampening
   - Consider adaptive saturation based on scene content

4. **Shadow desaturation** - Add slight desaturation when lifting shadows
   - Mimics camera behavior in low-light regions
   - Reduces the blue/purple shadow cast

### Lower Priority
5. **Highlight rolloff shape** - Softer transition to white in sky gradients

6. **Spectral loss rebalancing** - Consider weighting chroma errors higher to prevent hue drift

---

## Open Questions

1. Is the 17³ LUT too coarse for smooth sky gradients? Would 33³ help?
2. Should we constrain the LUT to prevent extreme hue shifts?
3. Is SPSA the right optimizer, or would something with better global search help?
4. Do we need per-scene optimization, or can we find universal parameters?

---

## Regional Loss Discovery (Nov 28)

Implemented 4×4 regional geos grid. Key finding on DSC00202:
- **Global loss:** 0.14%
- **Regional loss:** 0.32%
- **Discrepancy:** 2.3× higher regional than global

This confirms **statistical cancellation** - regional errors cancel when averaged globally.

### Implementation

```cpp
constexpr int GRID_SIZE = 4;  // 4×4 = 16 cells

struct TargetFeatures {
    StyleFeatures global;                              // 10D
    std::array<StyleFeatures, 16> regions;             // 16 × 10D
    cv::UMat lch;                                      // Pre-converted LCH
};

float computeProgressiveLoss(candidate, target, mode, globalWeight=0.3f) {
    // Global: 30% weight
    // Regional mean: 70% weight
    return globalWeight * global + (1-globalWeight) * localMean;
}
```

### Sampling Strategy

For efficiency, sample 8 of 16 cells (corners + center):
```
[0]  ·   ·  [3]
 ·  [5] [6]  ·
 ·  [9][10]  ·
[12] ·   · [15]
```

See [geos.md](./geos.md) for spectral loss theory.

---

## LUT Covariance Problem (KEY INSIGHT)

**Observation:** Regional errors persist even after dial optimization converges.

**Root Cause:** The 3D LUT estimation uses **global binning** that loses spatial context.

### The Problem

Camera processing is **spatially variant** - the same RGB input produces different outputs depending on location in the scene:

```
Scene A (sunlit foliage):  RGB(50,100,30) → Camera → (60,95,35)
Scene B (shaded foliage):  RGB(50,100,30) → Camera → (55,90,40)
```

Our LUT estimation bins ALL (50,100,30) pixels together and averages:
```
LUT[50,100,30] = mean of [(60,95,35), (55,90,40)] = (57.5, 92.5, 37.5)
```

This average is **wrong for both regions**.

### Why This Happens

Cameras apply spatially-aware processing:
1. **Local tone mapping** - Shadows treated differently than highlights
2. **Face detection** - Skin tones handled specially
3. **Sky detection** - Blues protected from oversaturation
4. **Adaptive white balance** - Mixed lighting scenes

### Potential Solutions

| Approach | Complexity | Benefit |
|----------|------------|---------|
| **Luminance-split LUTs** | Low | Separate LUTs for shadows/mids/highlights |
| **Regional LUTs** | Medium | 4 LUTs for quadrants, blend at boundaries |
| **Weighted binning** | Low | Weight contributions by spatial coherence |
| **Bilateral LUT** | High | Include spatial position in LUT lookup |

### Recommended Next Step

**Luminance-split LUTs** - Most tractable:

```cpp
// Estimate 3 LUTs based on pixel luminance
LUT_shadows   (L < 0.3)
LUT_midtones  (0.3 ≤ L ≤ 0.7)
LUT_highlights (L > 0.7)

// Apply with soft blending
output = blend(LUT_s(px), LUT_m(px), LUT_h(px), px.L)
```

This captures the most common spatial variance (exposure-dependent processing) without full regional complexity.

See [tune.md](./tune.md#two-link-architecture) for current LUT implementation.

---

## Meaningful Improvement Threshold

Exit optimization phases early when progress stalls.

### Parameters

```cpp
constexpr int STALL_THRESHOLD = 20;                    // Iterations without improvement
constexpr float MIN_RELATIVE_IMPROVEMENT = 0.01f;      // 1% relative change
constexpr float MIN_ABSOLUTE_IMPROVEMENT = 0.0001f;    // 0.01% absolute change
```

### Logic

Only reset stall counter if improvement is **meaningful**:
```cpp
bool meaningful = (relativeImprove >= 0.01f) || (absImprove >= 0.0001f);
if (meaningful) stall = 0;
else stall++;
```

This prevents wasted iterations on sub-perceptible refinements.

See [geos.md](./geos.md) for SPSA algorithm details.

---

## References

- [DCamProf Documentation](http://rawtherapee.com/mirror/dcamprof/dcamprof.html)
- [DCP Files Structure](https://dcptool.sourceforge.net/DCP%20FIles.html)
- [RawPedia Color Management](https://rawpedia.rawtherapee.com/Color_Management)
