# Edge: Frequency-Based Sharpness Optimization

[back](../README.md)

## Purpose

This document describes the theoretical foundation and algorithm for **sharpness** optimization. Edge provides content-invariant texture matching by comparing frequency-domain characteristics.

Edge captures the "crispness" of an image - sharp edges vs soft/dreamy, noise reduction level, fine detail preservation. It does **not** capture color or tone (see [geos.md](./geos.md) for spectral color/tone metrics).

The theory in this document underpins the **frequency loss** in [diff.md](./diff.md) and the **greedy optimizer** in [tune.md](./tune.md).

---

## Core Insight: Sharpness as Frequency Energy

Two images with the same sharpness characteristics have similar high-frequency energy distributions. Edge measures this using Laplacian variance - a content-invariant metric for edge energy.

**Key Properties:**
- Content-invariant (works across different scenes)
- Color-invariant (operates on luminance only)
- Robust to brightness/contrast changes
- Fast to compute (~2ms per image)

---

## Mathematical Foundation

### Step 1: Luminance Extraction

Convert image to grayscale to isolate spatial frequency information:

$$L = 0.299R + 0.587G + 0.114B$$

Color information is handled by GeoS; Edge focuses purely on spatial structure.

### Step 2: Laplacian Filter

Apply the Laplacian operator to detect edges:

$$\nabla^2 L = \frac{\partial^2 L}{\partial x^2} + \frac{\partial^2 L}{\partial y^2}$$

Implemented as convolution with kernel:
```
[ 0  1  0]
[ 1 -4  1]
[ 0  1  0]
```

### Step 3: Variance Computation

The Laplacian variance captures overall edge energy:

$$\text{edge\_energy} = \text{Var}(\nabla^2 L)$$

Higher variance = sharper image (more defined edges).

### Step 4: Frequency Loss

The loss measures relative difference in edge energy:

$$\mathcal{L}_{edge} = \frac{|\text{Var}_{candidate} - \text{Var}_{target}|}{\text{Var}_{target}}$$

**Properties:**
- $\mathcal{L} = 0$: Identical sharpness
- $\mathcal{L} > 0$: Sharpness mismatch
- Scale-invariant (relative, not absolute)

---

## Why Laplacian Variance?

### Comparison with Alternatives

| Method | Problem | Laplacian Advantage |
|--------|---------|---------------------|
| High-pass FFT | Expensive, complex interpretation | Single scalar metric |
| Sobel magnitude | Sensitive to noise | 2nd derivative more robust |
| Focus measure (Brenner) | Designed for autofocus | Better for artistic sharpness |
| SSIM | Requires spatial alignment | Content-invariant |

### What Edge Captures

1. **Edge Definition:** How sharp transitions are between regions
2. **Detail Preservation:** Fine texture vs smoothed surfaces
3. **Noise Level:** High-frequency artifacts increase variance
4. **Denoise Effect:** Reduced variance from noise reduction

### What Edge Does NOT Capture

1. **Color/Tone:** Hue, saturation, brightness (use GeoS)
2. **Contrast:** Overall brightness range (use GeoS)
3. **Composition:** Framing, geometry (user-controlled)

---

## Greedy Optimization

### Why Greedy?

With only 4 dials (sharpen amount, sharpen radius, denoise luminance, denoise chroma), exhaustive search is feasible. Greedy optimization is:
- Simple and reliable
- No hyperparameters to tune
- Deterministic results
- ~2 seconds total

### The Algorithm

For each detail dial in order:

```
for dial in [sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma]:
    best_value = golden_section_search(
        dial,
        range=[0.0, 1.0],
        loss_fn=frequency_loss,
        tolerance=0.01
    )
    dial = best_value  # Fix and continue
```

### Golden Section Search

Efficient 1D optimization:
1. Bracket the minimum in [a, b]
2. Evaluate at golden ratio points: $c = b - (b-a)/\phi$ and $d = a + (b-a)/\phi$
3. Narrow bracket based on which point has lower loss
4. Repeat until $|b - a| < \text{tolerance}$

**Evaluations per dial:** ~15 (log convergence)

### Dial Order

The order matters for greedy optimization:

1. **Sharpen amount** - Primary sharpness control
2. **Sharpen radius** - Refine sharpening character
3. **Denoise luminance** - Balance sharpness vs noise
4. **Denoise chroma** - Final color noise cleanup

This order follows the processing pipeline and ensures each dial optimizes in context of previous dials.

---

## The 4 Detail Dials

| Dial | Range | Default | Effect |
|------|-------|---------|--------|
| `sharpen_amount` | 0-1 | 0.6 | Strength of unsharp mask |
| `sharpen_radius` | 0-1 | 0.4 | Size of sharpening kernel (0.5-3px) |
| `denoise_luma` | 0-1 | 0.3 | Luminance noise reduction |
| `denoise_chroma` | 0-1 | 0.5 | Color noise reduction |

### Dial Interactions

- **sharpen + denoise_luma:** Competing effects on edge energy
- **radius + amount:** Larger radius needs less amount for same effect
- **denoise_chroma:** Minimal impact on Laplacian (color-blind metric)

The greedy approach handles interactions implicitly by optimizing in sequence.

---

## Implementation Considerations

### Proxy Resolution

Like GeoS, Edge uses 512×512 proxies:
- Frequency characteristics scale with image size
- Proxy captures representative sharpness
- Execution time <2ms per evaluation

### Robustness

The Laplacian variance metric is robust to:
- Image content variations
- Brightness/contrast changes
- Color shifts (luminance-only)

But sensitive to:
- Extreme noise (inflates variance)
- Heavily defocused images (near-zero variance)

### Integration with GeoS

Edge optimization runs **after** GeoS color/tone optimization:
1. GeoS optimizes 35 color/tone dials
2. Edge optimizes 4 detail dials
3. Total: 39 creative dials optimized

The separation ensures sharpness optimization doesn't interfere with color matching.

---

## Performance

| Metric | Value |
|--------|-------|
| Dials optimized | 4 |
| Evaluations per dial | ~15 |
| Total evaluations | ~60 |
| Time per evaluation | ~30ms |
| **Total time** | **~2 seconds** |

---

## API Interface

```cpp
namespace edge {

struct EdgeResult {
    float sharpen_amount;
    float sharpen_radius;
    float denoise_luma;
    float denoise_chroma;
    float final_loss;
    int evaluations;
};

struct EdgeConfig {
    float tolerance = 0.01f;
    int max_evaluations = 100;
};

// Optimize 4 detail dials
EdgeResult optimize(
    const cv::UMat& source_linear,
    const cv::UMat& reference,
    const EdgeConfig& config = EdgeConfig()
);

// Compute frequency loss between two images
float frequency_loss(
    const cv::UMat& candidate,
    const cv::UMat& reference
);

// Compute Laplacian variance (sharpness metric)
float laplacian_variance(const cv::UMat& image);

} // namespace edge
```

---

## See Also

- [geos.md](./geos.md) - Spectral loss theory (color/tone)
- [diff.md](./diff.md) - Loss metrics (spectral + frequency)
- [tune.md](./tune.md) - Orchestrates GeoS + Edge optimization
- [data.md](./data.md) - Style sidecar format
