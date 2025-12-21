# FLOW Pipeline Context - Root Cause Analysis

## Goal
Match camera JPEG output from scene-linear RAW data.

## Current Pipeline
```
HEAD (GPU) -> TONE (CPU) -> TUNE (CPU) -> output
   |            |            |
   v            v            v
scene-linear  lum-matched  color-corrected
```

## What Works
- **HEAD**: GPU RAW decode is correct (BLC, WB, demosaic, CST, warp)
- **TONE**: Histogram matching improves global luminance distribution
- Basic structure produces reasonable output

## What Doesn't Work
- **Artifacts persist** through all stages (shadows, highlights)
- **TUNE produces global effects** despite having spatial features
- Metrics improve but **visual quality doesn't change**

## Failed Approaches

### 1. Additive Delta Output
- `output = input + delta`
- **Problem**: Can't darken below input, causes washing out

### 2. Multiplicative Gain
- `output = input * exp(tanh(z))`
- **Problem**: Still global effects, same artifacts

### 3. Pixel MSE Loss
- Train on `(output - target)^2`
- **Problem**: Network learns to average, causes washing out

### 4. Histogram EMD Loss
- Match color channel histograms
- **Problem**: Computed but never backpropagated properly

### 5. Blurred Color Loss
- Blur both output and target, compute MSE
- **Problem**: Still global - no per-pixel gradient signal

### 6. Spatial Features (17 inputs)
- RGB, HSV, position (x,y), multi-scale context, variance
- **Problem**: Features present but training doesn't use them
- Random sampling + global loss = no spatial learning

## Root Cause

**The loss function is spatially incoherent.**

We have:
- Per-pixel features (spatial position, local context)
- Per-pixel output (RGB gain)
- But training samples random pixels independently
- And loss is computed globally (histogram, blurred average)

The network has no way to learn: "pixel at position (x,y) with context C should be treated THIS way" because the loss gives no position-dependent gradient.

## What Camera JPEGs Actually Do

1. **Global tone curve** (S-curve) - TONE partially handles this
2. **Color matrix/profile** - needs camera-specific calibration
3. **Local tone mapping** - shadow lift, highlight compression
4. **Adaptive exposure** - different for faces, sky, shadows
5. **Scene detection** - portrait mode, landscape, etc.

## Fundamental Questions

1. **Is per-pixel NN the right approach?**
   - Camera uses segmentation + region-based processing
   - Not a continuous learned function

2. **Do we need semantic understanding?**
   - "This is sky" vs "This is skin" vs "This is shadow"
   - Current pipeline has no scene understanding

3. **Is the reference signal sufficient?**
   - We only have the final JPEG, not intermediate stages
   - No per-pixel "ground truth" for how to process

4. **Should TONE and TUNE be separate?**
   - Maybe one stage that does both
   - Or neither - just learn end-to-end from HEAD

## Possible Paths Forward

### A. Abandon Per-Pixel NN
Use traditional image processing:
- Global tone curve (fit to histogram)
- Fixed color matrix
- Bilateral filtering for local contrast
- No learning, just parameter fitting

### B. Patch-Based Learning
Instead of random pixels:
- Train on image patches (e.g., 32x32)
- Loss computed per-patch, not globally
- Spatial coherence naturally emerges

### C. Segmentation First
- Segment image into regions (sky, shadow, skin, etc.)
- Learn different transforms per region type
- Requires segmentation model

### D. Direct Regression
- Train on full images, not samples
- Use perceptual loss (VGG features)
- Requires GPU training

### E. Guided Filtering
- Use reference JPEG structure to guide transform
- Edge-aware smoothing of color differences
- Not learning, but structured correction

### F. Start Over with LUTE
- Camera profile learning (DCP-style)
- Separate global tone from local effects
- Matrix + LUT approach, not NN

## Files

- `src/main/flow/part/head.cpp` - GPU RAW decode (working)
- `src/main/flow/part/tone.cpp` - Histogram matching (creates artifacts)
- `src/main/flow/part/tune.cpp` - Color NN (doesn't help)
- `src/test/flow/flow.cpp` - Test harness
- `inc/tune.hpp` - Network architecture

## Current State (2024-12-21)

TONE has shadow+highlight protection and 60% blend factor.
TUNE has 17 spatial features and multiplicative gain.
Both images (DSC00144, DSC00202) still show same artifacts.

**Recommendation**: Path F (LUTE) or Path A (traditional).
The per-pixel NN approach is fundamentally mismatched to the problem.
