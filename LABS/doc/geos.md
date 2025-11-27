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

Traditional gradient descent requires computing partial derivatives for each of the 35 dials. This means 35+ evaluations per iteration.

**SPSA (Simultaneous Perturbation Stochastic Approximation)** estimates the gradient of all 35 parameters with only **2 evaluations** per iteration.

### The Algorithm

At iteration $k$:

1. **Generate perturbation:** $\Delta_k \in \{-1, +1\}^{35}$ (Bernoulli random)

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

---

## Block-Wise Optimization (v2)

The original 35D simultaneous SPSA struggled with gradient variance. Block-wise optimization exploits the dial structure for faster convergence.

### Dial Layout

```
Index   Dial                    Block
─────────────────────────────────────
[0]     exposure                │
[1]     temperature             │ Block A (8)
[2]     tint                    │ ColorCorrection +
[3]     contrast                │ ToneMapping
[4]     highlights              │
[5]     shadows                 │
[6]     black                   │
[7]     white                   │
─────────────────────────────────────
[8]     vibrance                │
[9]     saturation              │ Block B (3)
[10]    colourDensity           │ GlobalColor
─────────────────────────────────────
[11-13] red H/S/L               │
[14-16] orange H/S/L            │
[17-19] yellow H/S/L            │ Block C (24)
[20-22] green H/S/L             │ SelectiveColour
[23-25] cyan H/S/L              │
[26-28] blue H/S/L              │
[29-31] purple H/S/L            │
[32-34] magenta H/S/L           │
```

### Four-Phase Strategy (BLOCKWISE mode)

| Phase | Block | Dials | Ratio | Purpose |
|-------|-------|-------|-------|---------|
| 1 | A | 8 | 30% | Establish base exposure/tone |
| 2 | B | 3 | 15% | Color saturation |
| 3 | A+B | 11 | 30% | Joint refinement |
| 4 | C | 24 | 25% | Per-hue polish |

**Rationale:** Exposure/contrast affect everything downstream. Optimize these first, then layer in saturation, then refine jointly. Per-hue adjustments are fine-tuning after globals converge.

### Mode Selection

```cpp
tune::Config config;
config.geos_mode = tune::GeosMode::BLOCKWISE;  // 4-phase (default)
config.geos_mode = tune::GeosMode::FULL_35D;   // All 35 simultaneously
```

**BLOCKWISE** converges faster (typically <300 iterations to 1% loss).
**FULL_35D** works but needs more iterations due to gradient variance.

---

## Limitations: Camera Color Response

### The Saturation Gap

Testing revealed a fundamental limitation: the optimizer converges to ~0.87% geodesic loss but the output still looks less saturated than the camera preview.

**Comparison (DSC00202.ARW):**

| Aspect | Camera Preview (Target) | Optimized Output |
|--------|------------------------|------------------|
| Greens | Vibrant, punchy | Muted, cyan-shifted |
| Log/ground | Warm brown | Pinkish cast |
| Overall | Bright, contrasty | Darker |

### Why Phase 4 (SelectiveColour) Didn't Help

Phase 4 ran 125 iterations with 24 HSL dials but loss stayed flat at 0.87%. The per-hue linear adjustments couldn't bridge the gap.

**Root cause:** The camera's JPEG engine applies transformations our dials can't replicate:

1. **Nonlinear color response** - The camera "pops" certain hues with a nonlinear curve, not just linear saturation boost
2. **Tone curve affecting color** - Camera S-curve increases perceived saturation in midtones
3. **Possibly a 3D LUT** - Input→output color mapping isn't separable by channel

### What We Can Tune vs What We Can't

| Tunable | Not Tunable (Yet) |
|---------|-------------------|
| Exposure compensation | Nonlinear color response curves |
| White balance | Camera-specific color science |
| Global saturation/vibrance | Per-hue nonlinear boosts |
| Linear HSL per channel | 3D LUT transformations |
| Tone curve shape | Camera's proprietary rendering |

### Future Directions

To close the saturation gap:

1. **Parametric color grading curve** - Add a dial-controlled curve that affects saturation nonlinearly (like Lightroom's tone curve)
2. **Better RAW color matrix** - Estimate the camera's color rendering more accurately in the decoder
3. **Per-hue saturation curves** - Instead of linear HSL shift, allow nonlinear saturation boost per hue range
4. **Learn camera profiles** - Train on camera JPEG vs RAW pairs to extract the implicit color transformation

The geodesic loss successfully matches the statistical fingerprint, but statistical similarity doesn't guarantee perceptual match when the target uses nonlinear transformations our linear dials can't express.

---

## See Also

- [edge.md](./edge.md) - Edge: Frequency loss (sharpness)
- [tune.md](./tune.md) - Orchestrates GeoS + Edge
- [diff.md](./diff.md) - Loss metrics implementation
- [data.md](./data.md) - Style sidecar format
