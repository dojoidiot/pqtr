# Future Ideas

[back](../README.md)

Ideas for future enhancement. Not in current scope.

---

## Edge: Multi-Scale Convolution Analysis

**Current:** Single-scale Laplacian variance for sharpness matching.

**Idea:** Multi-scale analysis to better capture different sharpness characteristics.

### Approach

```
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

### Implementation

```cpp
float multi_scale_loss(const cv::UMat& candidate, const cv::UMat& reference) {
    float loss = 0.0f;

    // Scale 1: Fine detail
    loss += 0.5f * scale_laplacian_var(candidate, reference, 1);

    // Scale 2: Medium edges
    loss += 0.3f * scale_laplacian_var(candidate, reference, 2);

    // Scale 3: Coarse structure
    loss += 0.2f * scale_laplacian_var(candidate, reference, 4);

    return loss;
}
```

### Considerations

- Adds ~3x compute per evaluation (still fast at ~6ms)
- Weights (0.5, 0.3, 0.2) need tuning via real-world testing
- May not significantly improve results for only 4 dials
- Worth pursuing if single-scale shows limitations in practice

---

## GeoS: Additional Feature Descriptors

**Current:** 10-dimensional feature vector (SVD + statistical moments).

**Idea:** Expand feature set for finer style discrimination.

### Potential Features

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

## Diff: Perceptual Uniformity

**Current:** Geodesic distance on feature hypersphere.

**Idea:** Weight features by perceptual importance.

### Approach

Instead of unit normalization, weight features:
```
v_weighted = [w1*σ1, w2*σ2, ..., wn*fn]
```

Where weights reflect human perception sensitivity to each feature.

### Considerations

- Requires perceptual studies to determine weights
- Current uniform weighting works well in practice
- Pursue if optimization converges but results look wrong

---

## Data: Binary Sidecar Format

**Current:** JSON sidecars (human-readable).

**Idea:** Optional binary format for performance.

### Benefits

- Faster load/save (~10x)
- Smaller files (~5x)
- Better for batch processing

### Format

```
[4 bytes: magic "LABS"]
[4 bytes: version]
[4 bytes: dial count]
[N * 4 bytes: float32 dial values]
[variable: metadata as msgpack]
```

### Considerations

- JSON is fine for current use case
- Binary only if batch processing thousands of files
- Keep JSON as canonical, binary as optional export
