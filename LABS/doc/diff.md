# Diff Tool Specification

[back](../README.md)

## Purpose

The `diff` tool computes loss metrics between images for the `tune` optimizer. It provides two complementary metrics:

- **Spectral loss**: Measures color/tone similarity (content-invariant)
- **Frequency loss**: Measures sharpness similarity

These correspond to the two automated stages of tuning. See [tune.md](./tune.md) for the three roles of tuning.

---

## Two Metrics for Two Properties

| Metric | Measures | Used By | Content-Invariant |
|--------|----------|---------|-------------------|
| **Spectral** | Color, tone, mood | SPSA optimizer (35 dials) | Yes |
| **Frequency** | Sharpness, texture | Edge optimizer (4 dials) | Yes |

### Why Two Metrics?

A single metric cannot capture both:
- Spectral features (SVD, statistics) ignore spatial frequency → blind to sharpness
- Frequency features (Laplacian) ignore color distribution → blind to tone

---

## Spectral Loss (Color/Tone)

Measures style similarity using geodesic distance on a feature hypersphere. See [geos.md](./geos.md) for full theory.

### Feature Extraction

1. Resize to 512×512 proxy
2. Convert to LCH with chroma-weighted hue
3. Compute SVD singular values
4. Build style vector:

```
style = [σ₁, σ₂, σ₃, μ_L, μ_C, std_L, std_C, skew_L, cov(L,C), cov(H,C)]
```

5. Normalize to unit hypersphere: `ψ = style / ||style||`

### Loss Function

```
spectral_loss = 1 - |⟨ψ_candidate | ψ_reference⟩|²
```

- Loss = 0: Identical color/tone style
- Loss = 1: Maximally different styles

### Properties

- **Content-invariant**: Works across different scenes
- **Geometry-invariant**: No alignment required
- **Fast**: <5ms per image (512×512 proxy)

---

## Frequency Loss (Sharpness)

Measures sharpness similarity using Laplacian variance.

### Laplacian Variance

The Laplacian operator highlights edges and fine detail:

```cpp
cv::Laplacian(gray, laplacian, CV_64F);
variance = laplacian.var();
```

Higher variance = sharper image (more edges, more detail).

### Loss Function

```
frequency_loss = |var(candidate) - var(reference)| / var(reference)
```

- Loss = 0: Identical sharpness
- Loss > 0: Different sharpness levels

### Properties

- **Content-invariant**: Measures overall sharpness, not specific edges
- **Fast**: <2ms per image
- **Intuitive**: Directly maps to perceived sharpness

---

## Command-Line Usage

```bash
# Compute both metrics
./diff candidate.png reference.png
# Output:
# Spectral Loss: 0.0156 (1.56%)
# Frequency Loss: 0.0234 (2.34%)

# Spectral only
./diff candidate.png reference.png --spectral
# Output: Spectral Loss: 0.0156

# Frequency only
./diff candidate.png reference.png --frequency
# Output: Frequency Loss: 0.0234

# Extract style features (for debugging)
./diff image.png --extract-style
# Output: Style vector: [σ₁=0.82, σ₂=0.31, ...]

# Visual diff (pixel difference, scaled)
./diff candidate.png reference.png --visual-diff output.png --scale 5.0
```

### Options

| Option | Description |
|--------|-------------|
| `--spectral` | Output spectral loss only |
| `--frequency` | Output frequency loss only |
| `--extract-style` | Show style feature vector |
| `--visual-diff FILE` | Output visual difference image |
| `--scale N` | Amplification for visual diff (default: 5.0) |

---

## Visual Difference Output

For debugging, generates an amplified pixel difference image:

1. Convert both images to float
2. Compute absolute difference per pixel
3. Amplify by scale factor (default: 5×)
4. Output as viewable image

Note: Visual diff shows pixel differences, not the metrics used for optimization.

---

## API Interface

```cpp
namespace pqtr {

// Spectral features for color/tone
struct SpectralFeatures {
    float singular_values[3];  // σ₁, σ₂, σ₃
    float mean_L, mean_C;      // Lightness, chroma means
    float std_L, std_C;        // Lightness, chroma std dev
    float skew_L;              // Lightness skew
    float cov_LC;              // Lightness-chroma covariance
    float cov_HC;              // Hue-chroma covariance
    float psi[10];             // Normalized hypersphere projection
};

// Combined metrics result
struct DiffMetrics {
    float spectral_loss;       // [0, 1] geodesic distance
    float frequency_loss;      // [0, ∞) relative variance difference
};

class Diff {
public:
    // Compute both metrics
    DiffMetrics compute(
        const cv::UMat& candidate,
        const cv::UMat& reference
    );

    // === Spectral (Color/Tone) ===

    SpectralFeatures extractStyle(const cv::UMat& image);

    float spectralLoss(
        const SpectralFeatures& a,
        const SpectralFeatures& b
    );

    float spectralLoss(
        const cv::UMat& candidate,
        const cv::UMat& reference
    );

    // === Frequency (Sharpness) ===

    float laplacianVariance(const cv::UMat& image);

    float frequencyLoss(
        const cv::UMat& candidate,
        const cv::UMat& reference
    );

    // === Visual Debugging ===

    cv::UMat visualDiff(
        const cv::UMat& candidate,
        const cv::UMat& reference,
        float scale = 5.0
    );

private:
    cv::UMat resizeProxy(const cv::UMat& image, int size = 512);
    cv::UMat convertToSafeLCH(const cv::UMat& image);
    void computeSVD(const cv::UMat& lch, float* singular_values);
    void projectToHypersphere(SpectralFeatures& features);
};

} // namespace pqtr
```

---

## Performance

| Metric | Time | Notes |
|--------|------|-------|
| Spectral loss | <5ms | 512×512 proxy, SVD |
| Frequency loss | <2ms | Single Laplacian + variance |
| Visual diff | <10ms | Full resolution |

---

## See Also

- [geos.md](./geos.md) - Spectral loss theory
- [tune.md](./tune.md) - How metrics are used in optimization
- [test.md](./test.md) - Test cases for both metrics
