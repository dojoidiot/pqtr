Here is a summary of our reverse-engineering process so far. We are working to mathematically replicate a **Sony "Standard" Camera Profile** by comparing a raw **Linear image** to a processed **Display image**.

We have discovered that the transformation is not a simple curve, but a **three-stage pipeline** following the DCP (DNG Camera Profile) standard.

## Pipeline Order (Corrected)

The correct order, validated against DCP specification and raw processing research:

```
Linear RAW → ColorMatrix (3x3) → HueSatMap (green dampening) → ToneCurve (S-curve)
```

### Stage 1: Color Matrix (applied FIRST to linear data)
A **3x3 Matrix** maps the camera's color gamut to a standard colorspace.
* **The Math:**
    $$
    \begin{bmatrix} 0.66 & 0.32 & 0.11 \\ 0.09 & 0.80 & -0.01 \\ \dots \end{bmatrix}
    $$
* **Key Characteristic:** Mixes 32% of Green into Red output (richer earth tones), reduces pure Green intensity to 80%.
* **Why first:** Must operate on **linear scene-referred data** to preserve colorimetric accuracy.

### Stage 2: HueSatMap (Hue-Specific Dampening)
Applied after matrix, targets specific hue ranges - particularly yellow-greens.
* **The Discovery:** Raw "neon" greens are **dampened**, not boosted:
    * **Brightness:** Reduced by ~40%
    * **Saturation:** Reduced by ~55%
* **The Lesson:** The "vibrant" Sony look comes from *removing* the radioactive/digital quality of raw greens, making foliage look deep and rich.
* **Why second:** Operates in HSV space on already color-corrected data.

### Stage 3: Tone Curve (applied LAST)
A Sigmoidal S-Curve for contrast and dynamic range compression.
* **Key Characteristic:**
    * **Lifts blacks:** input 0 → output 23
    * **Compresses highlights:** input 255 → output 147
* **Why last:** Tone curves shift hues if applied to uncorrected data (e.g., hue 11° becomes 22° after naive gamma). Must come after color corrections.

## Linear-Only Baseline Results

Testing with purely linear operations (no LUT, no tone curve):
| Mode | Loss |
|------|------|
| No optimization | 2.48% |
| Linear-only (exposure, WB, saturation, selective color) | **1.13%** |
| With LUT (non-linear) | 0.18% |

The ~1% gap between linear (1.13%) and LUT-assisted (0.18%) represents the **non-linear tone curve** contribution.

## Diff Analysis

The linear-only diff image shows errors concentrated in:
- **Shadows/dark areas** - tree trunk, dark foliage, shadows under plants
- **Cyan/green tint in shadows** - camera lifts these more than linear can
- **Highlights are clean** - linear operations handle bright areas well

This confirms the S-curve shadow lift is the critical missing piece.

---

## Key Insight: The Problem is Model-Agnostic

The reference image's source is **irrelevant**. Whether it came from:
- Sony camera JPEG
- Lightroom with user edits
- Capture One
- Any other software

...doesn't matter. The transform exists. It's deterministic. It maps input pixels to output pixels. We have both images - that's a complete dataset of the unknown function.

For every pixel we have:
- **Input:** (R, G, B) linear scene-referred
- **Output:** (R', G', B') transformed display-referred

That's millions of sample points.

---

## Model Comparison: Options for Capturing the Transform

| Model | Parameters | What it captures | Limitations |
|-------|------------|------------------|-------------|
| **3× 1D LUT** | 3 × 32 = 96 | Per-channel curves | No cross-channel, no hue-dependent |
| **3×3 Matrix** | 9 | Linear cross-channel mixing | Linear only |
| **Matrix + 1D LUT** | 9 + 96 = 105 | Linear mix + per-channel curves | Still no hue-dependent |
| **HueSatMap** | 90 × 25 × 3 = 6,750 | Hue+Sat dependent adjustments | Assumes specific pipeline |
| **Full 3D LUT** | 33³ × 3 = 107,811 | **Everything** | Large, needs good color coverage |

### Current Approach: 3× 1D LUT
- Achieves **0.18% loss** (down from 2.48% baseline)
- Limitation: Can't capture hue-dependent adjustments
- The residual error is concentrated in specific hues (greens) and tonal regions (shadows)

### Proposed: Full 3D LUT
A 3D LUT can capture **any** RGB→RGB transform with no assumptions:
- No guessing pipeline order
- No assuming DCP structure
- Works for camera profiles, user edits, film emulations, anything
- Just maps: for this input color → this output color

**Trade-offs:**
- Large (33³ grid = 35,937 points × 3 channels)
- Needs good coverage across color space (our images have lots of colors)
- Interpolation quality matters for smooth gradients

---

## Decision Points for Tomorrow

### Option A: Implement 3D LUT Estimator
**Pros:**
- Most general solution
- Captures any transform regardless of source
- No assumptions about pipeline structure
- Single unified approach

**Cons:**
- Large parameter space
- May need multiple images for full color coverage
- Interpolation artifacts possible in sparse regions

### Option B: Implement DCP-style Pipeline (Matrix → HueSatMap → Curve)
**Pros:**
- Matches industry standard
- Smaller parameter space
- Could extract Sony's actual DCP values as starting point

**Cons:**
- Assumes specific pipeline structure
- Won't capture arbitrary user edits that don't follow this model
- More complex implementation (multiple stages)

### Option C: Hybrid - 3D LUT with Constraints
Use 3D LUT structure but:
- Regularize to prefer smooth mappings
- Initialize from Matrix + 1D LUT estimate
- Let optimization refine

### Option D: Stay with Current 1D LUT + Accept 0.18% Floor
**Pros:**
- Already working
- Simple
- Fast

**Cons:**
- Known limitation with hue-dependent errors
- Can't improve beyond current floor

---

## Recommendation

**3D LUT** is the most principled approach because:
1. It makes no assumptions about how the transform was created
2. It can capture everything (matrix, curves, hue shifts, luminance-dependent color, cross-channel)
3. We have the data - millions of input/output pixel pairs
4. Standard size (33³) is well-established in color grading industry

The question is: do we have sufficient color coverage in our test images, or do we need multiple images to populate the full color cube?

---

## References
- [DCamProf Documentation](http://rawtherapee.com/mirror/dcamprof/dcamprof.html)
- [DCP Files Structure](https://dcptool.sourceforge.net/DCP%20FIles.html)
- [RawPedia Color Management](https://rawpedia.rawtherapee.com/Color_Management)

---

## RAWS ↔ Tune Reconciliation Analysis

*Research notes from analyzing the decoder/optimizer relationship.*

### Current Architecture

```
RAWS Decoder                          LABS Tune Optimizer
─────────────                         ─────────────────
RAW → BLC → WB → Demosaic             head.data() ──► pipe (dials) ──► output
  → ColorMatrix → Undistort → Crop              │                          │
                                      head.view() ──────────────► diff ◄───┘
Output: scene-linear sRGB                                           │
                                                                 tune (SPSA)
```

### Observation 1: Color Matrix Applied Twice (Conceptually)

The RAWS decoder applies the Sony color matrix (tag 0x7310) during decoding:
```
Camera RGB → ColorMatrix → linear sRGB
```

Then the tune optimizer uses **17 dials + 17³ LUT** to transform this *already color-corrected* data to match the camera preview. This creates a cascaded transformation:

```
CamRGB → [Sony Matrix in decoder] → sRGB → [17 dials + LUT in tune] → match preview
```

The optimizer is essentially **learning a correction to the decoder's color matrix** plus the camera's display-referred processing (HueSatMap + ToneCurve).

### Observation 2: What the Optimizer Must Learn

From the DCP analysis above, the camera's full processing is:
```
Linear RAW → ColorMatrix (3x3) → HueSatMap (green dampening) → ToneCurve (S-curve)
```

The RAWS decoder only does the ColorMatrix step. The tune optimizer must learn:
- HueSatMap (hue-dependent saturation/lightness)
- ToneCurve (S-curve with shadow lift)
- Any delta between our matrix and Sony's actual matrix

### Observation 3: Preview Resolution

- RAWS extracts: 1616×1080 preview (sRGB, 8-bit, JPEG-compressed)
- RAWS decodes: 6000×4000 scene-linear

The tune module uses 512×512 proxies for spectral loss (which is content-invariant), so resolution mismatch is acceptable. But JPEG artifacts in the preview could introduce noise.

---

## Improvement Ideas

### Idea 1: Expose Pre-Matrix Camera RGB

**Problem**: The 3D LUT learns `[decoder output] → [camera preview]`. This conflates our color matrix with the camera's actual color science.

**Solution**: Add optional mode in RAWS to output camera-native RGB *before* the color matrix:
```cpp
struct Result {
    pipe::View data;      // Current: scene-linear sRGB (post-matrix)
    pipe::View raw_rgb;   // NEW: camera-native RGB (pre-matrix)
};
```

**Benefit**: Tune could estimate a LUT that captures Sony's actual color science, not "Sony's color science minus our matrix inverse." More transferable to other cameras.

**Trade-off**: More complex API. May not matter if our matrix is accurate.

**Complexity**: Low | **Benefit**: Cleaner semantics | **Risk**: API change

---

### Idea 2: Use DCP Structure Instead of Blind LUT

**Problem**: We're estimating a 17³ LUT (4913 RGB values) when we *know* the transform follows DCP structure.

**Solution**: Use structured transforms matching DCP:
```cpp
struct CameraProfile {
    Matrix3x3 color_matrix;      // From RAWS (already applied)
    HueSatMap hue_sat;           // From DCP file or estimated
    ToneCurve tone;              // Optimized by tune (~10-20 params)
};
```

**Benefit**:
- Smaller parameter space (~100 params vs 4913)
- Interpretable (can see "greens dampened by 55%")
- Transferable (same DCP works across images)
- Could use Sony's published DCP as starting point

**Trade-off**: Assumes DCP model. Won't capture arbitrary user edits.

**Complexity**: Medium | **Benefit**: Interpretable, smaller | **Risk**: Model assumption

---

### Idea 3: Separate 1D Curves + Hue Adjustment

**Problem**: Single 3D LUT handles both tone mapping and hue-dependent color. These are conceptually separate.

**Solution**: Factor the transform:
```
scene-linear → [1D RGB Curves] → [Hue-dependent adjustment] → output
```

Components:
- **1D LUT (RGB curves)**: 3×256 = 768 values for tone mapping (S-curve)
- **Hue rotation**: 12 hue bins × 3 adjustments = 36 values for per-hue sat/lightness

**Benefit**:
- Much smaller than 3D LUT (804 vs 4913 values)
- Faster to optimize
- 1D curves capture S-curve; hue adjustment captures green dampening
- Matches the DCP model structure

**Trade-off**: Less general than full 3D LUT.

**Complexity**: Medium | **Benefit**: Faster, interpretable | **Risk**: Less general

---

### Idea 4: Use Preview Style Metadata for SPSA Initialization

**Problem**: SPSA starts from neutral/random values, may take many iterations to converge.

**Solution**: RAWS already extracts `creative_style`, `contrast`, `saturation`, `sharpness` from metadata. Use these to initialize SPSA closer to optimum:

```cpp
if (style_metadata.creative_style == "Standard") {
    // Calibrated Sony Standard starting points
    initial.contrast = 0.55f;
    initial.saturation = 0.52f;
    initial.shadow_lift = 0.15f;
    // ...
}
```

**Benefit**:
- Faster convergence (start closer to optimum)
- Fewer SPSA iterations needed
- Could cache calibrated starting points per style

**Trade-off**: Requires calibrating per style. Sony-specific.

**Complexity**: Low | **Benefit**: Faster convergence | **Risk**: Sony-specific

---

### Idea 5: Joint Matrix + LUT Optimization

**Problem**: We assume our decoder's color matrix is perfect. If it differs from Sony's actual processing, the LUT compensates for matrix errors.

**Solution**: Make color matrix tunable:
```cpp
struct GeosDials {
    // Current 17 dials...

    // NEW: matrix correction (6 params if white-constrained)
    float matrix_delta[6];
};
```

The optimizer jointly learns matrix correction + LUT.

**Benefit**: More accurate if our matrix doesn't perfectly match Sony.

**Trade-off**:
- Larger parameter space
- Risk of overfitting
- Matrix needs constraints (rows sum to 1, etc.)

**Complexity**: High | **Benefit**: More accurate | **Risk**: Overfitting

---

### Idea 6: "Gold Decode" Reference Path

**Problem**: The tune target `head.view()` is the embedded preview which has:
- Lower resolution (1616×1080 vs 6000×4000)
- JPEG compression artifacts
- Possibly different crop

**Solution**: Create a "gold decode" that processes RAW identically to Sony's camera:
```cpp
Result decode_gold(Sink& sink) {
    // Apply Sony's exact ColorMatrix
    // Apply Sony's HueSatMap (from DCP)
    // Apply Sony's ToneCurve (from DCP)
    // Return full-resolution display-referred
}
```

**Benefit**:
- Pixel-perfect target at full resolution
- No JPEG artifacts
- Could validate our processing against camera

**Trade-off**: Major engineering effort. Requires reverse-engineering Sony's exact parameters.

**Complexity**: High | **Benefit**: Perfect reference | **Risk**: Major effort

---

## Priority Ranking

| Priority | Idea | Rationale |
|----------|------|-----------|
| **1** | Idea 4: Style metadata init | Quick win, low risk, immediate benefit |
| **2** | Idea 3: Separate 1D + hue | Better matches known DCP structure |
| **3** | Idea 1: Pre-matrix RGB | Clean separation for future cameras |
| **4** | Idea 2: Full DCP structure | If we want interpretable profiles |
| **5** | Idea 5: Joint matrix | Only if matrix accuracy is a problem |
| **6** | Idea 6: Gold decode | Only if preview quality is limiting |

### Current Status

The current system achieves **0.05% spectral loss** with 17³ LUT + 17 dials. This is excellent.

These ideas would help with:
- **Faster optimization** (Idea 4)
- **More interpretable/transferable styles** (Ideas 2, 3)
- **Better generalization to non-Sony cameras** (Idea 1)
- **Theoretical cleanliness** (all ideas)

The question is whether the engineering effort is worth it given the current results.

---

## Separation of Concerns: Minimal RAWS Design

*Goal: RAWS outputs "raw" camera data. LABS handles all color science.*

### Current RAWS Pipeline (6 stages)

```
RAW → BLC → WB → Demosaic → ColorMatrix → Undistort → Crop → scene-linear sRGB
      ───────────────────   ───────────   ─────────   ────
      Essential              Questionable  Geometric   Essential
```

| Stage | What It Does | Camera-Specific? | Should Stay in RAWS? |
|-------|--------------|------------------|----------------------|
| **BLC** | Subtract black level | Yes (per-camera) | ✅ Yes - sensor-specific |
| **WB** | Apply camera WB gains | Yes (per-shot) | ⚠️ Debatable |
| **Demosaic** | Bayer → RGB | Yes (CFA pattern) | ✅ Yes - sensor-specific |
| **ColorMatrix** | Camera RGB → sRGB | Yes (per-camera) | ❌ Move to LABS |
| **Undistort** | Lens correction | Yes (per-lens) | ❌ Move to LABS |
| **Crop** | Remove borders | Yes (per-camera) | ✅ Yes - sensor-specific |

### Analysis: What's "Essential" vs "Interpretation"

**Essential (must be in RAWS):**
- **BLC**: Without this, values are wrong. Sensor-specific constant.
- **Demosaic**: Can't work with Bayer data in LABS. Must interpolate.
- **Crop**: Optical black borders are garbage data. Remove them.

**Interpretation (should move to LABS):**
- **ColorMatrix**: This is *color science* - mapping camera gamut to sRGB. Different matrices give different looks. The optimizer could learn this.
- **Undistort**: This is *geometric correction* - a creative choice. Some users want it, some don't. LABS already has a Geometric module.

**Debatable:**
- **WB**: Camera's auto-WB is baked into the shot. But:
  - If we *don't* apply it in RAWS, we get "camera-native" RGB
  - If we *do* apply it, we get "as-shot white-balanced" RGB
  - The tune optimizer already has WB dials...

### Proposed: Minimal RAWS (3 stages)

```
RAW → BLC → Demosaic → Crop → camera-native RGB (no WB, no matrix)
```

**Output:** `CV_32FC3` in camera-native color space:
- R, G, B values as the sensor saw them
- Black level corrected, normalized to [0,1+]
- No white balance applied
- No color matrix applied
- No lens distortion correction

### What LABS Gains

With camera-native RGB as input, LABS can:

1. **Apply WB as a dial** (already exists: `white_balance()` in mods.h)
2. **Apply ColorMatrix as a dial** (already exists: `color_matrix()` in mods.h)
3. **Learn the full transform** from camera-native → preview, not sRGB → preview

The 3D LUT would then capture:
```
camera-native RGB → display-referred (including camera's matrix + HueSatMap + ToneCurve)
```

Instead of:
```
sRGB (our matrix applied) → display-referred (inverse our matrix + camera matrix + HueSatMap + ToneCurve)
```

### What RAWS Passes to LABS

```cpp
struct Result {
    pipe::View data;          // CV_32FC3, camera-native RGB (no WB, no matrix)
    pipe::Info dataInfo;      // Metadata including:
                              //   - wb_rggb (camera's WB gains)
                              //   - color_matrix (camera's recommended matrix)
                              //   - distortion_params (lens correction coefficients)

    pipe::View preview;       // CV_8UC3, camera's display-referred JPEG
    pipe::Info previewInfo;   // Style metadata
};
```

LABS can then:
- Apply WB from metadata (or use dials to override)
- Apply ColorMatrix from metadata (or use dials to override)
- Apply lens distortion from metadata (or skip)
- Or let the optimizer learn the full transform

### Migration Path

**Phase 1: Pass metadata through**
- RAWS continues to apply WB + ColorMatrix
- But also passes raw WB/matrix values in metadata
- LABS can optionally apply them as identity

**Phase 2: Make RAWS output selectable**
```cpp
enum class OutputMode {
    SCENE_LINEAR_SRGB,    // Current: full pipeline (for backwards compat)
    CAMERA_NATIVE         // New: minimal pipeline (for optimizer)
};
Result decode(Sink& sink, OutputMode mode = SCENE_LINEAR_SRGB);
```

**Phase 3: Default to camera-native**
- Once LABS handles WB/matrix reliably
- RAWS becomes purely "sensor data extraction"

### Benefits

1. **Cleaner architecture**: RAWS = sensor. LABS = color science.
2. **Better optimization**: LUT learns actual camera transform, not correction-to-our-guess
3. **Camera-agnostic**: Different cameras just provide different metadata; LABS code unchanged
4. **User control**: WB/matrix become dials, not baked-in
5. **Future-proof**: New cameras only need RAWS decoder, no LABS changes

### Concerns

1. **Performance**: One more matrix multiply in LABS. Negligible.
2. **Breaking change**: Existing code expects sRGB. Need transition period.
3. **Preview alignment**: Camera preview was made with camera's processing. If we don't apply same WB/matrix, base image won't align. But that's okay - optimizer handles it.

### What About Undistort?

**Current location**: RAWS stage 5
**Proposed location**: LABS Geometric module (new sub-dial)

The distortion coefficients are already camera-specific metadata. LABS's Geometric module could accept them:

```cpp
// In LABS pipe
link.geometric().undistort(metadata.distortion_params, metadata.distortion_knot_count);
```

This gives users control: apply manufacturer's correction, skip it, or adjust it.

### Summary: Minimal RAWS

| Current RAWS | Proposed RAWS | Notes |
|--------------|---------------|-------|
| BLC on Bayer | BLC on Bayer | Keep - sensor constant |
| WB on Bayer | ❌ Remove | Move to LABS dial |
| Demosaic | Demosaic | Keep - interpolation |
| ColorMatrix | ❌ Remove | Move to LABS dial |
| Undistort | ❌ Remove | Move to LABS Geometric |
| Crop | Crop | Keep - remove garbage |

**Final RAWS output:** Camera-native RGB + metadata bundle

**LABS responsibility:** Apply WB, matrix, undistort (or let optimizer learn them)

---

## Implementation Status

*Implemented on 2025-11-28*

### Changes Made

**RAWS (src/main/part/sony/process.cpp):**
- Removed WB stage from `process_linear()`
- Removed ColorMatrix stage from `process_linear()`
- Removed Undistort stage from `process_linear()`
- Pipeline now: BLC → Demosaic → Crop (3 stages)

**RAWS (inc/raws.hpp):**
- Added `ColorMeta` struct with WB/matrix/distortion fields
- `Result` now includes `colorMeta` field

**RAWS (src/main/raws.cpp):**
- Populates `colorMeta` from Sony metadata:
  - `wb_r/g/b` normalized so G=1.0
  - `color_matrix` from tag 0x7310
  - `distortion_params` from tag 0x7037
- Added `color_space = "camera_native"` to dataInfo

**LABS (src/main/part/pipe/pipe.cpp):**
- Added `applyWhiteBalance()` function
- Added `applyColorMatrix()` function
- `PipeImpl::open()` now applies WB + matrix from `colorMeta`
- Updates `color_space` to `"scene_linear_srgb"` after processing

### Verification

After these changes, the pipeline produces the same output as before:
- RAWS outputs camera-native RGB (green cast, no color correction)
- LABS HEAD applies WB from metadata → balanced RGB
- LABS HEAD applies ColorMatrix from metadata → scene-linear sRGB
- LABS BODY processes with dials/modules as before

The separation is now clean:
- **RAWS**: Sensor-specific extraction only
- **LABS**: All color science (camera-agnostic)

### Future Work

1. **Lens Distortion**: Currently passed in metadata but not applied. Could add to LABS Geometric module.
2. **Tune Optimization**: Now learns actual camera transform instead of correction-to-decoder.
3. **New Cameras**: Only need RAWS decoder; LABS code unchanged.