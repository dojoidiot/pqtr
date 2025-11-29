# GeoS: Geodesic Spectrum

[back](../README.md)

## The Name

**GeoS = Geodesic Spectrum**

- **Geodesic**: The angular loss on the hypersphere. The geometry of the search.
- **Spectrum**: The color mapping. The feature fingerprint. The style space.

GeoS is the optimizer-independent model for style matching. It defines the space, the loss, and the visualization. Optimizers plug in underneath.

---

## The Search Space

All dial optimization happens in a 17D hypercube where each dial is a dimension in [0, 1].

```
Dial 1:  [0 ─────────────────── 1]
Dial 2:  [0 ─────────────────── 1]
  ...
Dial 17: [0 ─────────────────── 1]
```

Every point in this hypercube represents a complete set of dial settings.

---

## The Feature Space

The 17 dials produce an image. That image is reduced to 10 features:

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

These 10 features are normalized to a unit vector on a 10D hypersphere.

### Feature Extraction

1. **Color Space Transform**: Convert to LCH (Lightness, Chroma, Hue). Apply chroma-weighting to hue: $H_{safe} = H \cdot \tanh(k \cdot C)$ to suppress noise in achromatic regions.

2. **Spectral Decomposition**: SVD on image matrix extracts singular values (energy distribution).

3. **Statistical Descriptors**: Compute means, standard deviations, skewness, and covariances.

4. **Hypersphere Projection**: Normalize to unit length: $|\psi\rangle = \frac{\vec{v}}{||\vec{v}||_2}$

### What Features Capture

| Feature | Meaning |
|---------|---------|
| σ₁, σ₂, σ₃ | Energy/contrast magnitude |
| μ_L | Average brightness |
| μ_C | Average saturation |
| std_L | Contrast spread |
| std_C | Saturation spread |
| skew_L | High-key vs low-key |
| cov(L, C) | Do bright areas have more saturation? |
| cov(H_safe, C) | Which hues carry the most color? |

---

## The Coupling

Each dial can affect multiple features. Each feature is affected by multiple dials.

```
17 dials ──┬──► 10 features
           │
        coupled
```

Examples:
- Exposure dial → affects μ_L, std_L, skew_L, σ₁σ₂σ₃
- Saturation dial → affects μ_C, std_C, cov(L,C), cov(H,C)
- Contrast dial → affects std_L, σ₁σ₂σ₃, skew_L

The mapping is many-to-many. This is why dials are dependent—they all pull on the same 10 features from different directions.

---

## The Loss

The loss is the cosine distance between candidate and target feature vectors:

```
Loss = 1 - |⟨ψ_candidate | ψ_target⟩|²
```

One angle. Measured in 10D feature space. Driven by 17 coupled dials.

- Loss = 0 → identical style (parallel vectors)
- Loss = 1 → orthogonal styles (maximum difference)

### Properties

- **Non-negativity:** Loss ≥ 0
- **Identity:** Loss = 0 iff identical style vectors
- **Symmetry:** Loss(a, b) = Loss(b, a)
- **Smoothness:** Continuous first and second derivatives (suitable for optimization)

---

## The Covariance Matrix

The covariance matrix Σ is 17×17—dial-to-dial correlations in the search space.

```
Search space:       17D hypercube (dials)
Covariance matrix:  17×17 (dial correlations)
Feature space:      10D hypersphere (where loss is computed)
```

It captures: when dial A moves toward its optimum, which other dials tend to move with it?

The features tell us how far off we are. The dial covariance tells us how to step efficiently.

### Contribution to Variance

We don't know each dial's contribution to variance at starting state. It depends on:
- Current dial values (nonlinear effects)
- Specific image content
- The target being matched

Some optimizers ignore this (SPSA). Others learn it (CMA-ES).

---

## Optimizer Strategies

GeoS defines the space and loss. Optimizers navigate:

| | SPSA | CMA-ES |
|--|------|--------|
| Search space | Hypercube [0,1]^17 | Hypercube [0,1]^17 |
| Loss function | Cosine in 10D feature space | Cosine in 10D feature space |
| Dial coupling | Implicit (felt through loss) | Explicit (learned Σ matrix) |
| Perturbation shape | Random hypercube corners (±1 per dial) | Ellipsoidal (adapted to coupling) |
| Variance model | None (uniform) | Learned Σ |

Same space. Same loss. Same coupling. Different navigation.

```
┌─────────────────────────────────────────┐
│              GEOS (space)               │
│                                         │
│   17D hypercube [0,1]^17 (dials)        │
│   10D hypersphere (features)            │
│   Many-to-many coupling                 │
│   Cosine loss = one angle to target     │
│                                         │
├─────────────────────────────────────────┤
│           OPTIMIZER (strategy)          │
│                                         │
│   SPSA: ignores covariance              │
│   CMA-ES: learns covariance             │
│   (future): other strategies            │
│                                         │
└─────────────────────────────────────────┘
```

---

## 3D LUT Integration

Linear dials alone converge to ~0.87% loss but output still looks less saturated than the camera preview. The camera's JPEG engine applies nonlinear transformations that linear dials can't replicate.

**Solution: 17³ 3D LUT estimation**

1. Sample pairs from target and candidate at 17³ grid points in RGB space
2. Interpolate missing values using tetrahedral interpolation
3. Apply LUT after all linear dial adjustments

This captures nonlinear per-hue saturation boosts, S-curve effects, and channel-interdependent transforms.

---

## Visualization

The GeoS dome is visualized as a geodesic orb.

**Structure:** Any geodesic sphere. Map 17 dial values to triangles, distributed as evenly as possible across the surface.

**Color:** Dial value [0,1] maps to the visible spectrum:

```
0.0  0.2  0.4  0.6  0.8  1.0
 │    │    │    │    │    │
 🔴   🟠   🟡   🟢   🔵   🟣
red  orange yellow green blue violet
```

**Update:** Push 17 new values, orb recolors. During optimization, the orb shifts. Convergence = colors stabilize.

No optimizer logic in the visualization. Just dial state → color. The dome shows **what**; the optimizer decides **how**.

---

## Status

**GeoS model**: Defined. 17D dial hypercube → 10D feature hypersphere → cosine loss.

**Current optimizer**: SPSA. Works. 0.05% loss, ~65 seconds. No covariance awareness.

**Validated**: ACEO (Adaptive Covariance Evolver Optimiser) with pre-trained Σ from sample pics. Empirical covariance analysis shows 359 strong dial correlations (|r| > 0.3), max r = 0.979. Strong covariance detected.

**Next step**: Implement ACEO optimizer using prior covariance matrix from `etc/aceo.json`.

---

## See Also

- [spsa.md](./spsa.md) - SPSA optimization strategy
- [aceo.md](./aceo.md) - ACEO optimization strategy (validated)
- [edge.md](./edge.md) - Frequency loss (sharpness)
- [tune.md](./tune.md) - Orchestrates GeoS + Edge
- [diff.md](./diff.md) - Loss metrics implementation
