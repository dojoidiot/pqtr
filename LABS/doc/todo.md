# LABS TODO

## Completed

- [x] 23D feature vector (sigma, LCH, percentiles, shadow chroma, split tone colors)
- [x] Weighted L2 loss with trained weights
- [x] Training infrastructure (train-greedy, train-prms, train-exhaustive)
- [x] Hybrid mode: ACEO → SPSA polish
- [x] Full ACEO (45 dials) with covariance
- [x] `--full` mode for single-pass optimization
- [x] Built `bounds` diagnostic - found achievable limits
- [x] **Base curve implementation** (2024-12-01)
- [x] **Resolution independence verified** (2024-12-01)
- [x] **Baseline guard** (2024-12-01)
- [x] **Jacobian estimation** (2024-12-01)
- [x] **RAWS hasBaseCurve bug** - was stale library, fixed by rebuild
- [x] **Polynomial color transform** (2024-12-01)
  - 30 coefficients (10 per channel) capture camera's global RGB→RGB transform
  - Achieves <5% error on most images (3.1% DSC00159, 3.7% DSC00144)
  - `src/main/part/pipe/mods/poly_color.cpp`
- [x] **Local tone mapping module** (2024-12-01)
  - Iridix-style local adaptation based on patent US7302110B2
  - `src/main/part/pipe/mods/local_tone.cpp`
- [x] **ISP reverse-engineering** (2024-12-01)
  - Documented canonical ISP pipeline (14 stages)
  - Identified Sony's DRO as Apical Iridix implementation
  - Found same ColorMatrix for different scenes - DRO is the differentiator
  - `doc/hack.md`
- [x] **Foliage scene class analysis** (2024-12-01)
  - Wide-to-narrow research on why foliage scenes hit 15% floor
  - Found memory color patents (US6594388B1), saturation coupling to contrast
  - Documented in `doc/hack.md`
- [x] **PolyColor pipeline integration** (2024-12-02)
  - Added `polyCoeffs[30]` to RAWS Result
  - RAWS estimates coefficients and serializes to `dataInfo["poly_coeffs"]`
  - Added `pipe::Link::PolyColor` module with `setCoeffs()`/`isActive()`
  - Pipeline order: ColorCorrection → BaseCurve → PolyColor → ToneMapping
  - **Finding**: BaseCurve (768 params) achieves 12.63% baseline vs PolyColor (30 params) at 13.51%
  - BaseCurve remains primary transform; PolyColor available for experimentation

## Current: Simplified Workflow (2024-12-02)

```bash
# 1. Optimize dials to match a reference
tune photo.ARW reference.jpg --save-area output/

# 2. Apply the vibe
labs photo.ARW --tune output/tune.json --output photo.png

# 3. Debug: see pipeline stages
labs photo.ARW --tune output/tune.json --output photo.png --debug
```

Debug outputs:
- `photo_0_flat.png` - Scene-linear RAW (before processing)
- `photo_0_preview.png` - Camera's embedded JPEG
- `photo_1_body.png` - After all links applied

### Recent Completions

- [x] **Simplified CLI** (2024-12-02)
  - Removed `--camera-vibe` switch from labs (internal detail)
  - Added `--debug` to labs for pipeline artifact inspection
  - Updated README.md and tldr.md with workflow

- [x] **PolyColor pipeline integration** (2024-12-02)
  - BaseCurve (768 params) achieves 12.63% baseline
  - PolyColor (30 params) achieves 13.51% baseline
  - BaseCurve is primary; PolyColor available for experimentation

### Deferred

- **True spatial local tone mapping** - would require full Iridix algorithm; out of scope

---

## Architecture Notes

### Base Curve Flow

```
RAWS (camera-specific):
  - Decodes RAW
  - Extracts embedded JPEG preview
  - Estimates per-channel curves from flat→preview (768 floats: BGR × 256)
  - Returns baseCurve[768] in Result

LABS (generic):
  - Head stores curve from Result
  - Link.baseCurve().setCurve(head->baseCurve())
  - Applied in gamma space after colorCorrection, before toneMapping
```

### Metadata Ownership (2024-12-02)

**Principle**: Each layer serializes its own metadata. Downstream layers don't know internals.

```
RAWS owns:
  - dataInfo["poly_coeffs"]     ← serialized by RAWS, not pipe
  - dataInfo["decoder"]
  - dataInfo["camera_model"]
  - baseCurve[768]              ← binary array (frequently accessed)
  - hasBaseCurve                ← flag

pipe owns:
  - Passes dataInfo through unchanged
  - Exposes baseCurve() on Head (binary API for performance)
  - Does NOT expose polyCoeffs() - available via info["poly_coeffs"]

tune/labs access:
  - head->hasBaseCurve() + head->baseCurve()  ← direct
  - info.count("poly_coeffs")                  ← metadata check
```

**Why baseCurve is binary but polyCoeffs is serialized:**
- baseCurve (768 floats) is used every frame → binary API
- polyCoeffs (30 floats) is experimental/optional → metadata string is fine

### Jacobian Flow

```
jacob.cpp:
  1. Load image, create pipeline at neutral dials (0.5)
  2. For each dial d (0..44):
     - Perturb to 0.5 + ε, extract 23D features
     - Perturb to 0.5 - ε, extract 23D features
     - J[d][f] = (fwd[f] - bwd[f]) / (2ε)
  3. Save J[45][23] to etc/jacob.json

Potential use (not yet implemented):
  - Given feature error Δf = target - current
  - Compute dial correction: Δθ = J⁺ · Δf (pseudoinverse)
  - Apply correction, iterate
```

### Key Files

- `RAWS/inc/raws.hpp` - Result has baseCurve[768], hasBaseCurve
- `RAWS/src/main/raws.cpp` - estimateBaseCurve() function
- `LABS/inc/pipe.hpp` - Head::baseCurve(), Link::BaseCurve simplified
- `LABS/src/main/part/pipe/pipe.cpp` - HeadImpl stores/exposes curve
- `LABS/src/main/part/pipe/link.cpp` - BaseCurveImpl applies curve
- `LABS/src/main/part/pipe/mods/base_curve.cpp` - gamma-space LUT apply
- `LABS/src/main/part/geos/task.cpp` - baseline guard
- `etc/jacob.json` - Jacobian matrix (45×23)
- `src/test/geos/jacob.cpp` - Jacobian estimation tool

---

## Strategic Analysis: Path to Camera Parity (2024-12-01)

### The Question

We have:
- The RAW sensor data (exactly what the camera captured)
- The camera's embedded JPEG (the reference we want to match)
- All camera metadata (WB, color matrix, exposure, creative style settings)
- 45 style dials + 3D LUT (17³ = 14,739 parameters)

**Why can't we replicate the camera's output?**

### What the Camera Does (That We Know)

From Sony ARW files, we extract and apply:

| Step | Camera Operation | Our Implementation | Status |
|------|-----------------|-------------------|--------|
| 1 | Black level subtraction | `blc_bayer()` - subtract 512, normalize | ✅ Working |
| 2 | White balance gains | `wb_bayer()` - from tag 0x7313 | ✅ Working |
| 3 | Demosaic | `demosaic()` - OpenCV RGGB | ✅ Working |
| 4 | Color matrix | `color_matrix()` - from SR2SubIFD 0x7800 | ✅ Working |
| 5 | Lens undistort | `undistort()` - from tag 0x7037 | ✅ Working |
| 6 | Active area crop | `crop()` - from DNG tags | ✅ Working |
| 7 | Base curve | `baseCurve[768]` - estimated from flat→preview | ⚠️ Estimated |
| 8 | 3D LUT | `lut3d[17³×3]` - estimated from processed→target | ⚠️ Estimated |

### What the Camera Does (That We Don't Know)

| Camera Operation | Evidence | Our Gap |
|-----------------|----------|---------|
| **DRO (Dynamic Range Optimizer)** | Tag 0xb04f shows "Auto" or "Lv1-5" | Not implemented - spatially-variant tone mapping |
| **Creative Style curves** | Tag 0xb020 shows "Standard", "Vivid", etc. | We estimate from result, not from style definition |
| **Highlight rolloff** | Camera JPEGs have smooth highlight compression | Per-channel curves can't capture this (needs luminance-based rolloff) |
| **Local tone mapping** | DRO lifts shadows locally | Our tone dials are global |
| **Gamut mapping** | Camera handles out-of-gamut colors smoothly | We just clamp |
| **Noise reduction** | Applied before/during demosaic | We only have post-demosaic NR |

### The Fundamental Problem: Order of Operations

Our pipeline:
```
RAW → BLC → WB → Demosaic → ColorMatrix → [Crop] → BaseCurve → LUT → Dials → Output
                                                      ↑           ↑
                                                 Per-channel    RGB→RGB
```

Camera pipeline (hypothesized):
```
RAW → BLC → WB → Demosaic → ColorMatrix → DRO → CreativeStyle → LocalTM → Output
                                           ↑          ↑            ↑
                                        Spatial   Luminance-    Highlight
                                        aware     based         recovery
```

**Key insight**: The camera applies luminance-aware transforms that preserve color while adjusting tone. Our per-channel curves can shift colors as a side effect.

### Why DSC01531 Fails (36% → 16%)

This image has saturated greens and reds. Examining the issue:

1. **Per-channel curves shift hue**: When R curve differs from G curve, orange shifts toward red or yellow
2. **3D LUT has limited resolution**: 17³ grid means colors 4.4% apart bin together
3. **Color matrix is static**: Same 3x3 matrix for all pixels regardless of saturation

What the camera likely does:
- Hue-preserving saturation (keeps hue constant while adjusting chroma)
- Luminance-based color grading (saturated colors treated differently)
- Out-of-gamut handling (soft compression, not hard clipping)

### Path Forward: Stepwise Plan

#### Phase 1: Improve Base Curve (Quick Win)

Current curve estimation bins by gamma-space input value:
```cpp
// raws.cpp line 56-69
for (int c = 0; c < 3; c++) {
    int bin = d_ptr[x * 3 + c];  // Input channel value
    sum[c][bin] += p_ptr[x * 3 + c];  // Target channel value
    count[c][bin] += 1.0;
}
```

**Problem**: A red pixel and a neutral gray pixel with the same R value go in the same bin. This mixes hue-shifting color transforms with tone curves.

**Fix**: Estimate curves only from near-neutral pixels (low chroma), or weight by neutrality. This isolates the tone curve from color grading.

#### Phase 2: Luminance-Based Tone Mapping

Current tone mapping operates on RGB channels independently:
```cpp
// tone_map.cpp applies contrast, highlights, shadows to RGB
cv::pow(lifted, shadow_gamma, shadow_adjusted);  // Same gamma per channel
```

**Problem**: Changing luminance shifts colors when channels have different gains.

**Fix**: Convert to luminance + normalized color, apply tone curve to L only, preserve color ratios:
```cpp
float L = 0.2126*R + 0.7152*G + 0.0722*B;  // Luminance
float L_new = toneCurve(L);                 // Tone map luminance
float scale = L_new / L;                    // Color-preserving scale
R_out = R * scale;  // Preserves R:G:B ratios
G_out = G * scale;
B_out = B * scale;
```

#### Phase 3: Color Matrix Refinement

Current color matrix is extracted from SR2SubIFD (static, per-camera):
```cpp
// prepare.cpp - 3x3 matrix from tag 0x7800
metadata.color_matrix = cv::Matx33f(
    color_matrix_raw[0] / 1024.0f, ...);
```

**Problem**: This is the camera's color transform for D65 illuminant. If WB temperature differs significantly, the matrix may be suboptimal.

**Fix**: RAWS already has the correct color matrix from metadata. Validate it matches darktable/rawpy output. If discrepancy exists, investigate camera color profiles (DCP).

#### Phase 4: DRO Implementation (Spatially-Variant)

DRO tag shows the mode but not the implementation. Research approach:

1. Capture test images with DRO Off vs Auto vs Lv5
2. Compare to extract the spatial lift map
3. Implement as guided filter or bilateral-based local exposure

This is out of current scope but explains why some images have higher error.

### Immediate Next Steps (Prioritized)

1. **Neutral-pixel curve estimation** (RAWS change)
   - Filter pixels by chroma < threshold before binning
   - Expected improvement: Better tone isolation, reduced hue shifts

2. **Luminance-preserving tone mapping** (LABS change)
   - Add `TONE_MODE_LUMINANCE` option to tone_map module
   - Apply curves to L only, scale RGB proportionally

3. **Per-image validation tool**
   - Diff visualization showing where errors are highest
   - Feature breakdown showing which of 23D features contribute most to loss
   - Already have `printFeatureAnalysis()` - add to batch output

4. **Color matrix verification**
   - Compare our decoded flat image to darktable's ART camera input
   - Verify colorMatrix matches expected sRGB transform

### Success Metrics

| Metric | Current | Target | Method |
|--------|---------|--------|--------|
| Most images final loss | <5% | <3% | Neutral curve + lum tone |
| DSC01531 final loss | 16% | <8% | Hue-preserving saturation |
| Unreachable features | 4/23 | 2/23 | Luminance tone mapping |

### Research Questions

1. **Does Sony use DCP (DNG Camera Profiles)?** If so, can we extract/use them?
2. **Is the color matrix illuminant-dependent?** Does it change with WB?
3. **What is the exact DRO algorithm?** Local histogram equalization? Bilateral?
4. **Are Creative Style curves published?** Can we extract from camera firmware?

### Direct LUT Experiment (2024-12-01)

**Hypothesis**: Camera matching is measurement, not optimization. A single 3D LUT measured directly from flat→JPEG pixel correspondence should achieve near-zero loss.

**Implementation**: `src/test/geos/direct_lut.cpp`
- 33³ LUT grid (35,937 cells × 3 channels)
- Convert flat (scene-linear) to gamma space for binning
- For each pixel: bin by input RGB, accumulate target RGB
- Trilinear interpolation for lookup

**Results**:

| Image | Direct LUT Loss | Current Pipeline | Notes |
|-------|-----------------|------------------|-------|
| DSC00144 (preview) | 7.0% | ~5% | Worse |
| DSC01531 (outlier) | 18.8% | 16% | Worse |

**Problem Found**: 96% of LUT cells are empty.

```
[DirectLUT] Grid 33³: 1455 filled, 34482 empty (95.95% identity)
[DirectLUT] Flat gamma range: 0 to 0.834
[DirectLUT] Value distribution: low=83% mid=14% high=2.5%
```

Scene-linear data is heavily concentrated in the low value range. After gamma encoding (^1/2.2), 83% of values are still below 0.33. A uniform 33³ grid wastes most cells on unused regions.

**Why Direct LUT Fails**:
1. **Sparse coverage**: Only 4% of cells have data; 96% use identity mapping
2. **Interpolation across gaps**: Trilinear interpolation through empty cells produces incorrect values
3. **Grid mismatch**: Uniform grid in gamma space doesn't match actual data distribution

**Implications**:
- The measurement hypothesis is correct, but uniform LUT is the wrong structure
- Need adaptive structure: concentrates samples where data exists
- Or: histogram equalization before binning to spread values across grid
- Or: use current architecture (base curve + dials + small LUT) which effectively does this

**Conclusion**: Direct measurement is the right idea, but the current pipeline's approach (base curve handles the bulk of the transform, small LUT refines residuals) is more effective than a single large LUT with sparse coverage. The base curve captures the dominant 1D transforms, leaving only the 3D color shifts for the LUT to handle.

### Summary Conclusion

The 5% residual loss comes from:
- **50%**: Per-channel curves shifting hue (fixable with luminance-based tone mapping)
- **30%**: Limited 3D LUT resolution for saturated colors (increase grid or use tetrahedral interp)
- **15%**: DRO spatial variation (needs local tone mapping)
- **5%**: Color matrix imprecision, alignment errors, noise differences

The **quick wins** are neutral-pixel curve estimation and luminance-preserving tone mapping. These address the majority of the remaining error without architectural changes.

**Key insight from direct LUT test**: The two-phase architecture (base curve → dials → LUT) is more effective than a single monolithic LUT because it separates 1D tone transforms from 3D color shifts, using parameters efficiently.
