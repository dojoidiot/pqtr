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

## RAWS: Per-Image Color Matrix

**Current:** RAWS uses a hardcoded color matrix (prepare.cpp:544-547).

**Problem:** Baseline output has pink/magenta color cast because the hardcoded matrix is for one specific camera/illuminant combination. The tune optimizer compensates, but baseline is wrong.

### Root Cause

```cpp
// Hardcoded in RAWS/src/main/part/sony/prepare.cpp
metadata.color_matrix = cv::Matx33f(
    1344.0f / 1024.0f, -211.0f / 1024.0f,  -76.0f / 1024.0f,
      -9.0f / 1024.0f, 1224.0f / 1024.0f, -159.0f / 1024.0f,
       7.0f / 1024.0f,  -41.0f / 1024.0f, 1090.0f / 1024.0f);
```

Should read from Sony metadata tag 0x7310 (SR2SubIFD "Color Matrix").

### Fix (in RAWS)

1. Parse tag 0x7310 from SR2SubIFD
2. Extract 9 × int16 values (fixed-point /1024)
3. Build cv::Matx33f from actual file values
4. Fall back to hardcoded only if tag missing

### Impact

| Scenario | Current | Fixed |
|----------|---------|-------|
| Baseline output | Pink cast | Correct |
| Tune convergence | Compensates (works) | Faster (less work) |
| Cross-camera | Wrong colors | Correct |

### Considerations

- This is a RAWS fix, not LABS
- Current tune workflow works (optimizer compensates)
- Fix improves baseline quality and reduces tune iterations
- Would eliminate need for extreme WB adjustments

---

## See Also

- [analysis.md](./analysis.md) - Empirical findings and research
- [tune.md](./tune.md) - Current optimization implementation
- [geos.md](./geos.md) - Spectral loss theory
- [edge.md](./edge.md) - Frequency loss theory
