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

Traditional gradient descent requires computing partial derivatives for each of the 17 dials. This means 17+ evaluations per iteration.

**SPSA (Simultaneous Perturbation Stochastic Approximation)** estimates the gradient of all 17 parameters with only **2 evaluations** per iteration.

### The Algorithm

At iteration $k$:

1. **Generate perturbation:** $\Delta_k \in \{-1, +1\}^{17}$ (Bernoulli random)

2. **Evaluate loss at two points:**
   - $L^+ = \mathcal{L}(\theta_k + c_k \Delta_k)$
   - $L^- = \mathcal{L}(\theta_k - c_k \Delta_k)$

3. **Estimate gradient:**
   $$\hat{g}_k = \frac{L^+ - L^-}{2 c_k} \cdot \frac{1}{\Delta_k}$$

4. **Update parameters:**
   $$\theta_{k+1} = \theta_k - a_k \hat{g}_k$$

---

## Coarse-to-Fine Phases: HUGE → MIDS → TINY

SPSA optimization proceeds through three phases, each with different hyperparameters:

```
┌─────────────────────────────────────────────────────────────────┐
│                     GEOS PHASE PROGRESSION                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  HUGE ──────────► MIDS ──────────► TINY                         │
│  (explore)        (refine)         (converge)                   │
│                                                                  │
│  Large steps      Medium steps     Small steps                  │
│  Find basin       Approach min     Precise lock                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Phase Parameters

| Phase | $a_0$ | $c_0$ | $A$ | Purpose |
|-------|-------|-------|-----|---------|
| **HUGE** | 0.50 | 0.15 | 50 | Aggressive exploration, escape local minima |
| **MIDS** | 0.16 | 0.05 | 100 | Moderate refinement within basin |
| **TINY** | 0.05 | 0.01 | 100 | Precise convergence to minimum |

All phases use: $\alpha = 0.602$, $\gamma = 0.101$

### Phase Transitions

**HUGE → MIDS:** Triggered when optimization stalls (loss improvement < 0.001 over 30 iterations). This indicates the algorithm has found a basin and should stop exploring.

**MIDS → TINY:** Triggered when loss falls below 0.02 (2%). At this point, large steps risk overshooting; fine-tuning begins.

### Decay Schedules

Within each phase, gains decay from the phase start iteration:
- $a_k = a_0 / (k_{phase} + 1 + A)^\alpha$
- $c_k = c_0 / (k_{phase} + 1)^\gamma$

The phase-local iteration counter $k_{phase}$ resets at each phase transition, giving fresh momentum.

### Multi-Start Strategy

To avoid local minima, run SPSA from multiple initializations (default: 5) and keep the best result:
- Start 0: Neutral (all dials at 0.5)
- Starts 1-4: Random values in [0.2, 0.8]

Total iterations are divided evenly across starts.

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

## 3D LUT Integration

### Bridging the Saturation Gap

Early testing revealed that linear dials alone converged to ~0.87% loss but output still looked less saturated than the camera preview. The camera's JPEG engine applies nonlinear transformations that linear dials can't replicate.

**Solution: 17³ 3D LUT estimation**

Before GEOS optimization, we estimate a 3D lookup table that captures the camera's nonlinear color response:

1. **Sample pairs** from target and candidate at 17³ grid points in RGB space
2. **Interpolate** missing values using tetrahedral interpolation
3. **Apply LUT** after all linear dial adjustments

This captures:
- Nonlinear per-hue saturation boosts
- S-curve effects on color
- Channel-interdependent transforms

### Current Results

With 17³ LUT + 17 linear dials + 2 detail dials:

| Metric | Before | After |
|--------|--------|-------|
| Spectral loss | 2.48% | **0.05%** |
| Frequency loss | 81% | **<1%** |

The LUT handles what linear dials can't; the dials handle what needs user control.

---

## See Also

- [edge.md](./edge.md) - Edge: Frequency loss (sharpness)
- [tune.md](./tune.md) - Orchestrates GeoS + Edge
- [diff.md](./diff.md) - Loss metrics implementation
- [data.md](./data.md) - Style sidecar format
