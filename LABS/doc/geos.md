# GeoS: Geodesic Spectrum Analysis

[back](../README.md)

## Purpose

This document describes the theoretical foundation for **color/tone** style matching. GeoS provides content-invariant aesthetic comparison by treating image style as a geometric point on a high-dimensional hypersphere.

GeoS captures the "vibe" or "mood" of an image - warm/cool, saturated/muted, high-key/low-key, contrast, color harmony. It does **not** capture sharpness or texture (see [diff.md](./diff.md) for frequency-based sharpness metrics).

The theory in this document underpins the **spectral loss** in [diff.md](./diff.md) and the **SPSA optimizer** in [tune.md](./tune.md) (Stage 1: Color/Tone).

---

## Core Insight: Style as Geometry

Two images with the same "vibe" have similar statistical fingerprints even if their pixels differ entirely. GeoS encodes this fingerprint as a point on a high-dimensional unit sphere. Style similarity becomes angular proximity.

**Key Properties:**
- Content-invariant (works across different scenes)
- Geometric-invariant (no alignment required)
- Dimension-invariant (works regardless of image size)
- Brightness-invariant (normalized representation)

---

## Mathematical Foundation

### Step 1: Color Space Transform (Safe LCH)

Convert the image to LCH (Lightness, Chroma, Hue).

**The Achromatic Singularity Problem:** In achromatic regions (grays), Hue becomes undefined and can swing wildly, introducing noise into the optimization. GeoS applies Chroma-Weighting to the Hue channel:

$$H_{safe} = H \cdot \tanh(k \cdot C)$$

As Chroma ($C$) approaches 0, the Hue contribution approaches 0, preventing noise from driving the optimization.

### Step 2: Spectral Decomposition (SVD)

Reshape the image to matrix $A \in \mathbb{R}^{N \times 3}$ where N is the pixel count.

$$A = U \Sigma V^T$$

| Component | Interpretation |
|-----------|----------------|
| $\Sigma$ (Singular Values) | Energy spectrum (contrast magnitude) |
| $U$ (Left Singular Vectors) | Spatial-chromatic correlations |

### Step 3: Feature Extraction

Build the style vector $\vec{v}$ from statistical descriptors:

```
v = [
    σ₁, σ₂, σ₃,           # Singular values (energy distribution)
    μ_L, μ_C,             # Mean lightness/chroma
    std_L, std_C,         # Contrast/saturation spread
    skew_L,               # High-key vs low-key distribution
    cov(L, C),            # Brightness-saturation correlation
    cov(H_safe, C)        # Hue-saturation correlation (color harmony)
]
```

**Dimension:** 10 features (expandable)

### Step 4: Hypersphere Projection

Normalize to unit length:

$$|\psi\rangle = \frac{\vec{v}}{||\vec{v}||_2}$$

This projection ensures that:
- Absolute scale differences are removed
- Only the relative distribution of features matters
- Two images with proportionally similar features map to nearby points

### Step 5: Geodesic Distance (The Loss Metric)

The loss function measures angular distance on the hypersphere:

$$\mathcal{L} = 1 - |\langle \psi_{candidate} | \psi_{target} \rangle|^2$$

**Properties:**
- $\mathcal{L} = 0$: Identical style (parallel vectors)
- $\mathcal{L} = 1$: Orthogonal styles (maximum difference)
- Continuous and differentiable (suitable for optimization)

---

## Why This Approach Works

### Comparison with Alternatives

| Method | Problem | GeoS Solution |
|--------|---------|---------------|
| Pixel MSE | Fails when images differ structurally | Statistical fingerprint ignores spatial arrangement |
| Histogram Matching | Loses spatial relationships | SVD captures spatial-chromatic correlations |
| Neural Style Transfer | Slow, GPU-heavy, artifacts | SVD on 512×512 proxy, <5ms |
| Perceptual Loss (VGG) | Black-box, heavy dependency | Interpretable statistical features |
| SSIM + Color Diff | Requires geometric alignment | Content-invariant by design |

### What GeoS Captures (Color/Tone)

1. **Energy Distribution:** How contrast is distributed across the image (singular values)
2. **Tonal Character:** High-key vs low-key images (lightness skew)
3. **Color Intensity:** Overall saturation level (mean chroma)
4. **Color Harmony:** Which hues carry the most saturation (hue-chroma covariance)
5. **Mood Correlation:** Whether bright areas are also saturated (lightness-chroma covariance)

### What GeoS Does NOT Capture

1. **Sharpness:** Edge definition, fine detail (use frequency loss instead)
2. **Texture:** Surface patterns, grain (spatial frequency domain)
3. **Geometry:** Framing, composition (user-controlled)

See [tune.md](./tune.md) for the complete three-role tuning model.

---

## SPSA Optimization

### Why SPSA?

Traditional gradient descent requires computing partial derivatives for each of the 45 dials. This means 45+ evaluations per iteration.

**SPSA (Simultaneous Perturbation Stochastic Approximation)** estimates the gradient of all 45 parameters with only **2 evaluations** per iteration.

### The Algorithm

At iteration $k$:

1. **Generate perturbation:** $\Delta_k \in \{-1, +1\}^{45}$ (Bernoulli random)

2. **Evaluate loss at two points:**
   - $L^+ = \mathcal{L}(\theta_k + c_k \Delta_k)$
   - $L^- = \mathcal{L}(\theta_k - c_k \Delta_k)$

3. **Estimate gradient:**
   $$\hat{g}_k = \frac{L^+ - L^-}{2 c_k} \cdot \frac{1}{\Delta_k}$$

4. **Update parameters:**
   $$\theta_{k+1} = \theta_k - a_k \hat{g}_k$$

### Hyperparameters

Based on high-dimensional optimization research:

| Parameter | Value | Purpose |
|-----------|-------|---------|
| $a_0$ | 0.16 | Initial learning rate |
| $c_0$ | 0.05 | Initial perturbation size |
| $\alpha$ | 0.602 | Learning rate decay |
| $\gamma$ | 0.101 | Perturbation decay |
| $A$ | 100 | Stability constant |

**Decay schedules:**
- $a_k = a_0 / (k + 1 + A)^\alpha$
- $c_k = c_0 / (k + 1)^\gamma$

### Multi-Start Strategy

To avoid local minima, run SPSA from multiple random initializations (typically 5) and keep the best result.

---

## Implementation Considerations

### Proxy Resolution

Feature extraction uses 512×512 thumbnail proxies:
- Full-resolution SVD is expensive
- Statistical properties are preserved at lower resolution
- Execution time <5ms per image

### Bounds Handling

All dial values are constrained to $[0, 1]$:
- After each update, clip values: $\theta = \max(0, \min(1, \theta))$
- Gradient estimation remains valid at boundaries

### Convergence Criteria

Stop optimization when:
- Loss falls below threshold (e.g., 0.01)
- Maximum iterations reached (e.g., 500)
- Loss improvement stalls (<0.001 over 50 iterations)

---

## Theoretical Guarantees

### SPSA Convergence

Under standard assumptions (smooth loss function, bounded gradients), SPSA converges to a local minimum with probability 1. The convergence rate is:

$$\mathbb{E}[||\theta_k - \theta^*||^2] = O(k^{-1/3})$$

### Geodesic Metric Properties

The geodesic loss function satisfies:
- **Non-negativity:** $\mathcal{L} \geq 0$
- **Identity:** $\mathcal{L} = 0 \iff$ identical style vectors
- **Symmetry:** $\mathcal{L}(a, b) = \mathcal{L}(b, a)$
- **Smoothness:** Continuous first and second derivatives

---

## References

- Spall, J.C. (1992). "Multivariate Stochastic Approximation Using a Simultaneous Perturbation Gradient Approximation." IEEE Transactions on Automatic Control.
- Sra, S. (2012). "A Short Note on Parameter Approximation for von Mises-Fisher Distributions." Computational Statistics.

---

## See Also

- [diff.md](./diff.md) - Spectral mode implementation
- [tune.md](./tune.md) - SPSA algorithm implementation
- [data.md](./data.md) - .vibe file format specification
