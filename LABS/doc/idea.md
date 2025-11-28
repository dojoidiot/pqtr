# Future Ideas

[back](../README.md)

Theoretical enhancements not yet in scope. For empirical findings, see [analysis.md](./analysis.md).

---

## Luminance-Split LUTs

**Current:** Single 17³ 3D LUT estimated via global binning.

**Problem:** Camera processing is spatially variant - same RGB produces different outputs based on scene luminance. Global binning averages these, creating errors for all regions. See [analysis.md](./analysis.md#lut-covariance-problem-key-insight) for details.

### Approach

Estimate 3 separate LUTs based on pixel luminance:

```cpp
LUT_shadows   (L < 0.3)
LUT_midtones  (0.3 ≤ L ≤ 0.7)
LUT_highlights (L > 0.7)

// Apply with soft blending
output = blend(LUT_s(px), LUT_m(px), LUT_h(px), px.L)
```

### Benefits

| Aspect | Single LUT | Luminance-Split |
|--------|------------|-----------------|
| Shadow accuracy | Averaged | Dedicated |
| Highlight accuracy | Averaged | Dedicated |
| Storage | 17³ × 3 = 14.7K | 17³ × 3 × 3 = 44.1K |
| Complexity | Low | Medium |

### Considerations

- 3× storage increase acceptable (44KB vs 15KB in tune.json)
- Soft blending at boundaries prevents discontinuities
- Captures most common spatial variance without regional complexity
- May not help with face/sky detection (content-aware, not luminance-aware)

---

## Multi-Scale Edge Analysis

**Current:** Single-scale Laplacian variance for sharpness matching.

**Problem:** Different sharpness characteristics (fine texture vs coarse edges) may require different dial settings.

### Approach

```cpp
edge_loss = w1 * laplacian_var(scale_1) +  // fine detail (1px)
            w2 * laplacian_var(scale_2) +  // medium edges (2px)
            w3 * laplacian_var(scale_3)    // coarse structure (4px)
```

### Benefits

| Scale | Captures | Dial Sensitivity |
|-------|----------|------------------|
| Fine (1px) | Noise, micro-texture | denoise_luma, sharpen_amount |
| Medium (2px) | Edge definition | sharpen_radius |
| Coarse (4px) | Structure, contrast edges | sharpen_amount |

### Considerations

- Adds ~3× compute per evaluation (still fast at ~6ms)
- Weights (0.5, 0.3, 0.2) need tuning via real-world testing
- May not significantly improve results for only 2 dials
- Worth pursuing if single-scale shows limitations in practice

---

## Additional geos Feature Descriptors

**Current:** 10-dimensional feature vector (SVD + statistical moments).

**Problem:** Some style differences may not be captured by current features.

### Approach

Expand feature set:

| Feature | Captures |
|---------|----------|
| Histogram entropy | Tonal complexity |
| Color histogram peaks | Dominant colors |
| Spatial color variance | Color distribution uniformity |
| Hue histogram spread | Color palette width |

### Considerations

- More features = higher-dimensional optimization
- SPSA scales well, but convergence may slow
- Current 10 features may already be sufficient
- Add only if style matching shows gaps

---

## Perceptual Feature Weighting

**Current:** Geodesic distance treats all features equally.

**Problem:** Human perception may be more sensitive to some features than others.

### Approach

Weight features by perceptual importance:

```cpp
v_weighted = [w1*σ1, w2*σ2, ..., wn*fn]
```

Where weights reflect human perception sensitivity.

### Considerations

- Requires perceptual studies to determine weights
- Current uniform weighting works well in practice
- Pursue if optimization converges but results look perceptually wrong

---

## Binary Sidecar Format

**Current:** JSON sidecars (human-readable).

**Problem:** JSON parsing overhead for batch processing.

### Approach

Optional binary format:

```
[4 bytes: magic "LABS"]
[4 bytes: version]
[4 bytes: dial count]
[N × 4 bytes: float32 dial values]
[variable: metadata as msgpack]
```

### Benefits

- Faster load/save (~10×)
- Smaller files (~5×)
- Better for batch processing

### Considerations

- JSON is fine for current use case
- Binary only if batch processing thousands of files
- Keep JSON as canonical, binary as optional export

---

## Regional LUTs

**Current:** Global LUT applied uniformly.

**Problem:** Even luminance-split LUTs may not capture spatial variations like face detection or sky protection.

### Approach

Estimate 4 quadrant LUTs with boundary blending:

```cpp
LUT_TL, LUT_TR, LUT_BL, LUT_BR  // 4 quadrants

// Bilinear blend based on pixel position
output = bilinear_blend(LUTs, px.x, px.y)
```

### Considerations

- 4× storage increase
- More complex estimation (need spatial coherence)
- May overfit to specific image layout
- Consider only if luminance-split proves insufficient

---

## RAWS: Per-Image Color Matrix ✅ IMPLEMENTED

**Status:** Implemented 2024-11. Tag 0x7800 in encrypted SR2SubIFD.

**Original Problem:** RAWS used a hardcoded color matrix causing pink/magenta cast.

### Implementation

1. Parse DNGPrivateData (tag 0xc634) from IFD0 to get SR2 IFD location
2. Read SR2 encryption parameters (tags 0x7200, 0x7201, 0x7221)
3. Decrypt SR2SubIFD using Dave Coffin's algorithm (128-element pad array)
4. Extract ColorMatrix from tag 0x7800 (9 × int16, fixed-point /1024)
5. Fall back to hardcoded matrix only if tag missing

**Key Discovery:** Documentation incorrectly stated tag 0x7310 - that's BlackLevel. ColorMatrix is at **0x7800**.

### Measured Impact

Example: DSC00202.ARW vs hardcoded fallback (from DSC00458.ARW):

| Position | Per-Image | Fallback | Delta |
|----------|-----------|----------|-------|
| [0,0] | 1361 | 1344 | +17 |
| [0,2] | -94 | -76 | -18 |
| [1,0] | 66 | -9 | **+75** |
| [1,1] | 1149 | 1224 | -75 |
| [2,0] | 77 | 7 | **+70** |
| [2,1] | -180 | -41 | **-139** |
| [2,2] | 1159 | 1090 | +69 |

Significant per-image variation confirms value of reading actual matrix.

### Code Location

- `RAWS/src/main/part/sony/prepare.cpp` - SR2SubIFD decryption and ColorMatrix extraction

---

## Camera Metadata Hints

**Current:** RAWS extracts metadata but LABS only uses it for display/comparison.

**Opportunity:** Camera metadata contains hints about photographer intent that could inform LABS module behavior.

### Currently Extracted (RAWS)

| Category | Fields | Current Use |
|----------|--------|-------------|
| **Shooting** | iso, shutter_speed, aperture, focal_length, lens_model | Metadata display |
| **Style** | creative_style, dro, contrast, saturation, sharpness | Preview comparison |
| **Processing** | wb_rggb, color_matrix, black/white_level | RAWS decoder |
| **Lens** | distortion_params, distortion_knot_count | Undistort module |

### Hint Opportunities for LABS

| Hint | EXIF/MakerNote | Target Module | Use Case |
|------|----------------|---------------|----------|
| **ISO** | EXIF 34855 | Detail | High ISO → gentler sharpening, noise-aware path |
| **DRO level** | Sony 0xb04f | ToneMapping | "Lv5" → aggressive shadow lift expected |
| **Creative Style** | Sony 0xb020 | GlobalColor | "Vivid" → higher saturation, "Portrait" → skin protection |
| **Contrast** | Sony 0x2004 | ToneMapping | +3 → steeper curve expected |
| **Saturation** | Sony 0x2005 | GlobalColor | Direct hint for saturation dial |
| **Sharpness** | Sony 0x2006 | Detail | Direct hint for sharpen_amount |
| **Focal length** | EXIF 37386 | Geometric | Vignette correction (lens-dependent) |
| **Aperture** | EXIF 33437 | Geometric | Vignette intensity varies with f-stop |

### Not Yet Extracted (potential value)

| Tag | Description | Potential Use |
|-----|-------------|---------------|
| **Face regions** | Sony 0x0201 | Protect skin tones, local exposure |
| **Scene type** | EXIF 41729 | Auto-detected scene → style hints |
| **Metering mode** | EXIF 37383 | Exposure intent (spot vs matrix) |
| **Flash fired** | EXIF 37385 | WB expectations, harsh light handling |
| **Focus distance** | MakerNotes | DOF-aware sharpening |
| **Long exposure NR** | Sony tag | Camera already applied NR to JPEG |

### Implementation Approaches

**Approach 1: Hint-as-Dial-Init**

Use style settings as starting points for tune optimization:

```cpp
// In tune, before SPSA optimization
if (hint.contrast > 0)
    dial_tone_contrast = 0.5f + hint.contrast * 0.05f;  // +2 → 0.60
if (hint.saturation > 0)
    dial_saturation = 0.5f + hint.saturation * 0.05f;
```

Benefits:
- Faster tune convergence (closer to target)
- Respects photographer intent
- No architectural changes to pipe

**Approach 2: Hint-as-Branch**

Enable/disable processing paths based on metadata:

```cpp
// In Detail module
if (info["iso"] > 6400)
    enable_noise_aware_sharpening();  // Gentler, edge-preserving

// In GlobalColor module
if (info["creative_style"] == "Portrait")
    enable_skin_protection();  // Reduce saturation in skin hues
```

Benefits:
- Adaptive processing per-image
- Better handling of edge cases (high ISO, portraits)
- Requires module modifications

### Considerations

- **Scope boundary**: RAWS extracts, LABS interprets (clean separation)
- **Hint vs Override**: Hints inform defaults, user dials override
- **Tune interaction**: Hints could bias tune starting point, not constrain search
- **Cross-camera**: Sony-specific tags need abstraction for Canon/Nikon

### Priority

1. **ISO → Detail** - Most impactful, high-ISO images need different sharpening
2. **Style settings → Dial init** - Direct mapping, fast convergence
3. **Face regions** - High value but complex (regional processing)

---

## Color Science Metadata

**Current:** RAWS extracts basic color data but misses several DNG-standard tags that define proper color processing.

**Opportunity:** Standard color science metadata defines mathematically correct processing that cameras expect RAW converters to implement.

### DNG Standard Tags

The [DNG Specification](https://www.kronometric.org/phot/processing/DNG/dng_spec_1.4.0.0.pdf) defines a complete color science framework:

| Tag | Name | Purpose | Currently Used |
|-----|------|---------|----------------|
| 0xc621 | ColorMatrix1 | Camera RGB → XYZ (Illuminant 1) | Hardcoded |
| 0xc622 | ColorMatrix2 | Camera RGB → XYZ (Illuminant 2) | No |
| 0xc623 | CameraCalibration1 | Per-unit calibration adjustment | No |
| 0xc624 | CameraCalibration2 | Per-unit calibration adjustment | No |
| 0xc65a | CalibrationIlluminant1 | Light source for Matrix1 (e.g., A=2856K) | No |
| 0xc65b | CalibrationIlluminant2 | Light source for Matrix2 (e.g., D65=6500K) | No |
| 0xc714 | ForwardMatrix1 | WB'd camera RGB → XYZ D50 | No |
| 0xc715 | ForwardMatrix2 | WB'd camera RGB → XYZ D50 | No |
| 0xc625 | ReductionMatrix1 | XYZ → camera RGB (inverse path) | No |
| 0xc62a | BaselineExposure | Expected EV adjustment | No |
| 0xc6fc | ProfileToneCurve | Expected rendering curve | No |

### Sony-Specific Tags

| Tag | Name | Purpose | Currently Used |
|-----|------|---------|----------------|
| 0x7800 | ColorMatrix | Camera RGB → sRGB | ✅ Now reads per-image |
| 0x7313 | WB_RGGB | As-shot white balance | Yes |
| 0x7010 | ToneCurve | Linearization curve points | Yes (fixed curve) |
| 0x7037 | DistortionCorrParams | Lens distortion spline | Yes |
| 0x2011 | VignettingCorrection | Vignette correction on/off | No |
| 0x2012 | LateralChromaticAberration | CA correction on/off | No |
| 0x0115 | ColorSpace | sRGB vs AdobeRGB selection | No |

### Dual-Illuminant Matrix Interpolation

DNG spec requires interpolating ColorMatrix1/2 based on white balance CCT:

```cpp
// DNG 1.2+ required algorithm
float cct_wb = colorTemperature(as_shot_neutral);
float cct_1 = illuminantCCT(CalibrationIlluminant1);  // e.g., 2856K (A)
float cct_2 = illuminantCCT(CalibrationIlluminant2);  // e.g., 6500K (D65)

// Interpolate using inverse CCT (mired scale)
float t = (1/cct_wb - 1/cct_1) / (1/cct_2 - 1/cct_1);
t = clamp(t, 0, 1);

ColorMatrix = lerp(ColorMatrix1, ColorMatrix2, t);
```

**Impact:** Without this, daylight shots use tungsten matrix or vice versa → color cast.

### Forward Matrix Path (Alternative)

DNG provides two paths for color conversion:

**Path A: ColorMatrix** (camera RGB → XYZ → sRGB)
```
Camera RGB → ColorMatrix → XYZ → Bradford adapt → XYZ D65 → sRGB matrix → linear sRGB
```

**Path B: ForwardMatrix** (white-balanced RGB → XYZ D50 → sRGB)
```
Camera RGB → WB → ForwardMatrix → XYZ D50 → D50→D65 adapt → sRGB matrix → linear sRGB
```

DNG spec: *"If ForwardMatrix tags are included, use those. Otherwise use ColorMatrix."*

Sony ARW files typically don't include ForwardMatrix, so Path A applies.

### Chromatic Adaptation

When using ColorMatrix path with illuminant ≠ D65:

```cpp
// Bradford chromatic adaptation: XYZ(source) → XYZ(D65)
// Only needed if ColorMatrix output is not D65-adapted
cv::Matx33f bradford_A_to_D65 = {
    0.8446965f, -0.1366786f,  0.2296475f,
   -0.0363200f,  1.0296515f,  0.0068627f,
    0.0f,        0.0f,        1.2093654f
};
XYZ_D65 = bradford_A_to_D65 * XYZ_A;
```

**Question for validation:** Does Sony's 0x7310 matrix already output D65-adapted values, or raw XYZ needing adaptation?

### Placement: Decoder vs Pipe

| Item | Placement | Rationale |
|------|-----------|-----------|
| **ColorMatrix interpolation** | RAWS | Fundamental colorimetry, must happen before any processing |
| **Chromatic adaptation** | RAWS | Part of correct XYZ→sRGB path |
| **Vignette correction** | RAWS | Optical artifact, not stylistic |
| **CA correction** | RAWS | Optical artifact, not stylistic |
| **BaselineExposure** | LABS hint | Affects exposure dial starting point |
| **ProfileToneCurve** | LABS hint | Affects tone mapping target |
| **ColorSpace selection** | LABS | Affects output gamut (sRGB vs AdobeRGB) |

### Implementation Priority

1. ~~**Read ColorMatrix from 0x7800**~~ - ✅ DONE (was incorrectly documented as 0x7310)
2. **Dual-illuminant interpolation** - If Sony provides Matrix1/2, interpolate by WB temp
3. **Vignette correction** - Extract 0x2011, apply in RAWS
4. **CA correction** - Extract 0x2012, apply in RAWS
5. **BaselineExposure → LABS** - Pass as exposure hint

### TODO: Research Questions

1. Does Sony embed CalibrationIlluminant tags, or just a single matrix?
2. Is Sony's 0x7310 matrix D65-referenced or camera-illuminant-referenced?
3. What vignette correction data does Sony embed (coefficients or just on/off)?
4. Does Sony's CA correction need coefficients or is it algorithmic?

### References

- [DNG Specification 1.6.0.0](https://paulbourke.net/dataformats/dng/dng_spec_1_6_0_0.pdf)
- [Developing RAW by Hand](https://www.odelama.com/photo/Developing-a-RAW-Photo-by-hand/Developing-a-RAW-Photo-by-hand_Part-2/)
- [Sony MakerNote Tags](https://exiftool.org/TagNames/Sony.html)
- [darktable color calibration](https://docs.darktable.org/usermanual/4.0/en/module-reference/processing-modules/color-calibration/)

---

## Pipeline Order Validation

**Status:** Order confirmed correct (2024-11 review).

### Current Pipeline

```
RAWS (Decoder)              LABS (Processing)           LABS (Display)
──────────────              ─────────────────           ──────────────
RAW Bayer                   Scene-Linear sRGB           Scene-Linear
    │                            │                           │
    ├─► BLC (Bayer)             ├─► Exposure                ├─► sRGB Gamma
    ├─► WB (Bayer)              ├─► White Balance           ├─► Clip [0,1]
    ├─► Demosaic                ├─► Tone Mapping            ├─► 8-bit
    ├─► Color Matrix            ├─► Global Color            │
    ├─► Undistort               ├─► Selective Color         ▼
    └─► Crop                    └─► Detail              PNG (sRGB)
         │                           │
         ▼                           ▼
    Scene-Linear sRGB           Scene-Linear sRGB
    [0, 1+] HDR headroom       [0, 1+] HDR headroom
```

### Validated Decisions

| Decision | Correct | Rationale |
|----------|---------|-----------|
| BLC before WB | Yes | Must subtract offset before applying gains |
| WB on Bayer | Yes | Balance channels before interpolation reduces demosaic artifacts |
| Color Matrix after Demosaic | Yes | Need RGB pixels to transform primaries |
| Processing in Linear | Yes | Exposure/color math is correct in radiometric space |
| Gamma at output only | Yes | Preserves HDR headroom through processing chain |
| HDR headroom [0,1+] | Yes | Scene highlights can exceed 1.0, clipped only at display |

### Terminology

"Scene-linear sRGB" is correct:
- **Scene-linear**: Photometrically proportional to scene luminance
- **sRGB**: The color primaries (gamut) and D65 white point, NOT the gamma

### TODO: Deep Research Validation

Final validation against authoritative sources:

1. **dcraw / LibRaw** - Verify order matches established RAW processors
2. **DNG Specification** - Adobe's canonical RAW processing order
3. **darktable / RawTherapee** - Cross-reference open-source implementations
4. **Bruce Lindbloom** - Color science reference for matrix placement
5. **ICC Profile Spec** - PCS requirements for color transforms

Specific questions to verify:
- Is WB-on-Bayer universally preferred, or do some pipelines WB after demosaic?
- Does chromatic adaptation (Bradford) belong before or after color matrix?
- Should undistort happen before or after demosaic for best quality?

---

## Architecture: RAWS vs TUNE Separation of Concerns

**Status:** Documented 2024-11. Core architectural principle.

### The Boundary

```
RAWS (Extraction)                    TUNE/LABS (Optimization)
─────────────────────────────        ─────────────────────────────
Decompress sensor data               Linear transforms (exposure, curves)
Black level subtraction              Display-referred transforms
Normalize to [0,1]                   Tone mapping, gamma
White balance (camera-reported)      Color grading, saturation
Demosaic                             Geometric LUTs
ColorMatrix → standard colorspace    Style matching

OUTPUT: Scene-referred               OUTPUT: Display-referred
        Linear RGB                           sRGB (or target space)
        Camera-agnostic                      Reference-matched
```

### Key Insight

**RAWS output will NOT look like a camera JPEG.** This is correct behavior, not a bug.

Scene-referred linear data:
- Has no tone curve (looks flat, low contrast)
- Has no saturation boost (looks desaturated)
- Has no manufacturer color science (looks "neutral")
- Has HDR headroom (values can exceed 1.0)

Camera JPEGs have:
- Proprietary tone curves
- Color rendering / "film simulation" / creative styles
- Contrast and saturation adjustments
- Clipped to display range

**The camera JPEG is just one possible style.** TUNE's job is to find the transforms that match *any* reference - camera JPEG, film emulation, or custom look.

### Validation Criteria

**RAWS is correct if:**
1. Same camera → TUNE finds consistent transforms across images
2. Different cameras → TUNE finds different transforms, but all match their reference
3. TUNE error rates are low → extraction is providing usable canonical data

**RAWS is NOT validated by:**
- Visual match to camera JPEG (that's TUNE's job)
- "Pleasing" colors (subjective, style-dependent)
- Looking like other RAW converters' default output (they apply hidden curves)

### Why This Matters

If RAWS tried to match camera JPEG directly:
- Would bake in one manufacturer's style
- Different cameras would need different RAWS code paths
- TUNE would have nothing to optimize
- Custom styles would be impossible

By keeping RAWS neutral and canonical:
- All style decisions are in TUNE/LABS
- Same pipeline works for any camera (once extraction is implemented)
- Any reference can be matched (JPEG, film, custom)
- Full creative control preserved

### Practical Implication

When RAWS output looks "wrong" (flat, pink, desaturated):
1. **Don't** chase visual match to JPEG in RAWS code
2. **Do** verify data is mathematically correct (black levels, WB gains, matrix values)
3. **Do** run TUNE and check if it achieves low error rates
4. **If** TUNE succeeds → RAWS is working, "wrong" appearance is expected
5. **If** TUNE fails → investigate RAWS data quality

---

## RAWS: Baseline Appearance Notes

**Status:** Expected behavior. Documented for reference.

**Observation:** RAWS output appears cyan/pink compared to embedded JPEG preview.

### Understanding (2024-11)

This is **not a bug** but the expected appearance of scene-referred linear data before style transforms.

**What RAWS outputs:**
- Scene-linear RGB in standard colorspace
- Camera white balance applied (neutrals are neutral)
- No tone curve, no saturation boost, no contrast

**What camera JPEG has:**
- Sony's proprietary color science
- DRO (Dynamic Range Optimizer) shadow lift
- Creative Style processing
- sRGB gamma curve

The visual difference is entirely explained by missing display transforms.

**Validation:** TUNE achieves low error rates on real images → decompression and extraction are correct.

### Investigation Record

Ruled out as causes (2024-11 debugging session):
- Bayer pattern interpretation (RGGB confirmed correct)
- WB gain mapping
- ColorMatrix application
- Linearization curve
- ARW2 decompression interleaving

Confirmed working:
- ARW2 decompression (verified via stripes test)
- Per-image ColorMatrix extraction (matches exiftool)
- Canonical pipeline order

### If Future Issues Arise

Only investigate RAWS if:
1. TUNE cannot converge (error rates stay high)
2. Systematic artifacts appear (banding, wrong colors in specific regions)
3. Cross-camera comparison shows one camera worse than others

Approach:
1. Compare raw pixel values against dcraw/LibRaw LINEAR output (not default)
2. Verify black level subtraction values
3. Check WB gain normalization

---

## See Also

- [analysis.md](./analysis.md) - Empirical findings and research
- [tune.md](./tune.md) - Current optimization implementation
- [geos.md](./geos.md) - Spectral loss theory
- [edge.md](./edge.md) - Frequency loss theory
