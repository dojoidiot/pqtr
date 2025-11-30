# GeoS: Style Matching System

[back](../README.md)

## The Name

**GeoS = Geometric Style Space**

- **Geometric**: The structured loss landscape. The geometry of optimization.
- **Style**: The color/tone fingerprint. The feature space.

GeoS is the optimizer-independent model for style matching. It defines the space, the loss, and the visualization. Optimizers plug in underneath.

---

## The Search Space

All dial optimization happens in a 45D hypercube where each dial is a dimension in [0, 1].

```
Dial 1:  [0 ─────────────────── 1]
Dial 2:  [0 ─────────────────── 1]
  ...
Dial 45: [0 ─────────────────── 1]
```

Every point in this hypercube represents a complete set of dial settings.

**45 style dials:** ColorCorrection (3) + ToneMapping (7) + GlobalColor (3) + SplitTone (4) + SelectiveColor (24) + Detail (4)

---

## The Feature Space (19D)

The 45 dials produce an image. That image is reduced to **19 features**:

```
[0-2]   σ₁, σ₂, σ₃       # SVD singular values (energy distribution)
[3]     μ_L               # Mean luminance (brightness)
[4]     μ_C               # Mean chroma (saturation)
[5]     std_L             # Luminance std (CONTRAST - critical)
[6]     std_C             # Chroma std (saturation spread)
[7]     skew_L            # Luminance skewness (high-key vs low-key)
[8-9]   cov_LC, cov_HC    # Correlations (color harmony)
[10-11] μ_a, μ_b          # Lab a*/b* means (COLOR CAST - critical)
[12-15] L_p10, L_p25, L_p75, L_p90  # Luminance percentiles (TONE CURVE)
[16-17] C_p50, C_p90      # Chroma percentiles (saturation level)
[18]    C_shadow          # Shadow chroma (preserve color in darks)
```

### Feature Groups

| Index | Features | Purpose |
|-------|----------|---------|
| 0-2 | σ₁, σ₂, σ₃ | Energy/contrast magnitude via SVD |
| 3-4 | μ_L, μ_C | Mean brightness and saturation |
| 5-7 | std_L, std_C, skew_L | Contrast, saturation spread, key |
| 8-9 | cov_LC, cov_HC | Color harmony correlations |
| 10-11 | μ_a, μ_b | Color cast (Lab a*/b* axes) |
| 12-15 | L_p10, L_p25, L_p75, L_p90 | Tone curve shape |
| 16-17 | C_p50, C_p90 | Saturation levels |
| 18 | C_shadow | Shadow color preservation |

### Critical Features (High Weight)

- **std_L** (index 5): Contrast. Camera JPEGs have more contrast than flat RAW.
- **L percentiles** (12-15): Tone curve shape. Where are shadows/highlights?
- **μ_a, μ_b** (10-11): Color cast. Pink/green or yellow/blue shifts.

### Feature Extraction

1. **Color Space Transform**: Convert to LCH and Lab color spaces.

2. **Spectral Decomposition**: SVD on image matrix extracts singular values (energy distribution).

3. **Statistical Descriptors**: Means, standard deviations, skewness, covariances.

4. **Percentiles**: Luminance at 10th, 25th, 75th, 90th percentiles for tone curve.

5. **Shadow Analysis**: Chroma in darkest 25% of pixels.

---

## The Coupling

Each dial can affect multiple features. Each feature is affected by multiple dials.

```
45 dials ──┬──► 19 features
           │
        coupled
```

Examples:
- Exposure dial → affects μ_L, std_L, skew_L, σ₁σ₂σ₃, L percentiles
- Saturation dial → affects μ_C, std_C, cov_LC, cov_HC, C percentiles
- Contrast dial → affects std_L, σ₁σ₂σ₃, skew_L, L percentiles

The mapping is many-to-many. This is why dials are dependent—they all pull on the same 19 features from different directions.

---

## The Loss

**Weighted L2 loss** in 19D feature space:

```
Loss = Σ weights[i] × (feature[i] - target[i])²
```

Each feature has a learned weight from `etc/cnst.json`. Critical features (std_L, percentiles, color cast) have high weights (~5.0).

- Loss = 0 → identical style
- Loss > 0 → style difference, weighted by feature importance

### Why Weighted L2?

| Approach | Pros | Cons |
|----------|------|------|
| Cosine (geodesic) | Scale-invariant, elegant | All features equal weight |
| L2 (Euclidean) | Simple | Some features matter more |
| **Weighted L2** | Feature importance from training | Needs weight training |

We train weights via batch analysis: features with high variance across images get higher weights.

### Properties

- **Non-negativity:** Loss ≥ 0
- **Identity:** Loss = 0 iff all weighted feature differences are zero
- **Symmetry:** Loss(a, b) = Loss(b, a)
- **Smoothness:** Continuous first and second derivatives (suitable for optimization)
- **Interpretability:** Each term shows which feature contributes most to error

---

## The Covariance Matrix

The covariance matrix Σ is 45×45—dial-to-dial correlations in the search space.

```
Search space:       45D hypercube (dials)
Covariance matrix:  45×45 (dial correlations)
Feature space:      19D (where loss is computed)
```

It captures: when dial A moves toward its optimum, which other dials tend to move with it?

The features tell us how far off we are. The dial covariance tells us how to step efficiently.

### Trained Artifacts

| File | Purpose |
|------|---------|
| `etc/cnst.json` | Feature weights (19 values) |
| `etc/prms.json` | SPSA phase params (a0, c0 per block) |
| `etc/cvar.json` | 45×45 covariance matrix for ACEO |

### Contribution to Variance

We don't know each dial's contribution to variance at starting state. It depends on:
- Current dial values (nonlinear effects)
- Specific image content
- The target being matched

Some optimizers ignore this (SPSA). Others learn it (CMA-ES/ACEO).

---

## Optimizer Strategies

GeoS defines the space and loss. Optimizers navigate:

| | SPSA | ACEO | HYBRID |
|--|------|------|--------|
| Search space | Hypercube [0,1]^45 | Hypercube [0,1]^45 | Hypercube [0,1]^45 |
| Loss function | Weighted L2 in 19D feature space | Weighted L2 in 19D feature space | Weighted L2 in 19D feature space |
| Dial coupling | Implicit (felt through loss) | Explicit (prior Σ matrix) | Both (ACEO→SPSA) |
| Perturbation shape | Random hypercube corners | Ellipsoidal (eigenspace) | Ellipsoidal→Random |
| Variance model | None (uniform) | Prior + learned | Prior then uniform |
| Role | Polish, builds covariance | Fast direction/pop | Best of both |

Same space. Same loss. Same coupling. Different navigation strategies:
- **SPSA**: Phased exploration, builds covariance
- **ACEO**: Eigenspace search from prior
- **HYBRID**: ACEO for direction/pop (half iterations), SPSA for polish (remaining)

```
┌─────────────────────────────────────────┐
│              GEOS (space)               │
│                                         │
│   45D hypercube [0,1]^45 (dials)        │
│   19D feature space                     │
│   Many-to-many coupling                 │
│   Weighted L2 loss                      │
│                                         │
├─────────────────────────────────────────┤
│           OPTIMIZER (strategy)          │
│                                         │
│   SPSA:   phased, builds covariance     │
│   ACEO:   eigenspace from prior Σ       │
│   HYBRID: ACEO→SPSA (best of both)      │
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

**Structure:** Any geodesic sphere. Map dial values to triangles, distributed as evenly as possible across the surface.

**Color:** Dial value [0,1] maps to the visible spectrum:

```
0.0  0.2  0.4  0.6  0.8  1.0
 │    │    │    │    │    │
 🔴   🟠   🟡   🟢   🔵   🟣
red  orange yellow green blue violet
```

**Update:** Push new dial values, orb recolors. During optimization, the orb shifts. Convergence = colors stabilize.

No optimizer logic in the visualization. Just dial state → color. The dome shows **what**; the optimizer decides **how**.

---

## Status

**GeoS model**: Defined. 45D dial hypercube → 19D feature space → weighted L2 loss.

**Feature vector v3**: 19 features including percentiles, shadow chroma, and color cast.

**Optimizers**:
- **SPSA**: Phased optimization with full 45D exploration. Builds covariance via `--save-cov`.
- **ACEO**: Eigenspace search using prior covariance from `etc/cvar.json`.
- **HYBRID**: ACEO for direction/pop, then SPSA for polish. Best of both worlds.

**Current limitation**: Pipeline capability gap discovered.
```
Target std_L:   0.2244 (camera JPEG contrast)
Max achievable: 0.1303 (our dials at extremes)
Gap:            42% unreachable
```

**Root cause**: Camera JPEGs apply a base tone curve (per picture style) BEFORE adjustments. We start from flat baseline.

**Next step**: Learn per-camera base curves to expand achievable range. See [base_curve.md](./base_curve.md).

---

## See Also

- [tldr.md](./tldr.md) - Quick overview
- [spsa.md](./spsa.md) - SPSA optimization strategy
- [aceo.md](./aceo.md) - ACEO optimization strategy
- [edge.md](./edge.md) - Frequency loss (sharpness)
- [tune.md](./tune.md) - Orchestrates GeoS + Edge
- [diff.md](./diff.md) - Loss metrics implementation
- [base_curve.md](./base_curve.md) - Per-camera base curve learning
- [todo.md](./todo.md) - Current status and next steps
