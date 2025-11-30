# ACEO: Adaptive Covariance Evolving Optimiser

[back](../README.md)

## Overview

ACEO is a covariance-aware optimization strategy for the LABS dial system. Based on CMA-ES (Covariance Matrix Adaptation Evolution Strategy), ACEO learns the covariance structure of the 45-dial search space and adapts its search to align with the loss landscape.

**Full ACEO** includes all 45 style dials in a unified eigenspace optimization.

## Empirical Findings

Covariance analysis was performed on 10 sample images using SPSA optimization with `--skip-lut`. The results strongly support covariance-aware optimization.

### Results Summary (Historical - 36 dials)

| Metric | Value |
|--------|-------|
| Images analyzed | 10 |
| Variable dials | 36 (original subset) |
| Strong correlations (|r| > 0.3) | **359 pairs** |
| Maximum correlation | **r = 0.979** |

*Note: Full ACEO now uses all 45 dials. New covariance analysis pending.*

### Top Dial Correlations

| Dial A | Dial B | Correlation |
|--------|--------|-------------|
| red_sat | cyan_sat | +0.979 |
| red_hue | yellow_sat | -0.972 |
| cyan_hue | purple_lum | -0.962 |
| green_hue | purple_hue | +0.953 |
| density | cyan_lum | +0.947 |
| red_hue | orange_sat | -0.917 |
| cyan_hue | magenta_lum | -0.917 |
| red_lum | cyan_lum | +0.903 |
| purple_sat | magenta_lum | -0.892 |
| yellow_lum | magenta_sat | +0.890 |

### Interpretation

The selective_color block (24 dials for 8 hue ranges) shows heavy coupling. When SPSA perturbs `red_sat` independently, it fights the coupled response from `cyan_sat`. This creates tilted valleys in the loss landscape that SPSA bounces off instead of following.

**Verdict: Strong covariance detected. ACEO beneficial.**

## The Covariance Problem

SPSA treats all dials as potentially coupled but searches with random hypercube corner perturbations (±1 per dial). If dials covary strongly, SPSA wastes steps bouncing off tilted valleys in the loss landscape.

Image processing dials are not independent:
- Exposure affects contrast perception
- Saturation affects hue purity
- Complementary hue channels (red/cyan, yellow/blue) counter-rotate

These couplings come from pipeline math and human perception. If dials covary, the loss landscape has tilted elliptical contours. An optimizer that knows the tilt can step along valleys instead of across them.

## Three Approaches to Covariance

### Option 1: Observe As We Go

Learn covariance during optimization.

- Start with identity matrix (assume independent)
- Update covariance estimate from successful steps
- Gradually adapt

**Pro**: No upfront cost, adapts to each image
**Con**: Slow to learn, might converge before covariance is well-estimated

### Option 2: Quick Probe at Start

Run a brief stepwise test before main optimization.

- Perturb each dial, measure how others' optima shift
- Build covariance estimate in ~72 evaluations (2 per dial)
- Use to initialize search

**Pro**: Cheap, image-specific
**Con**: Covariance might vary at different dial positions

### Option 3: Offline from Sample Pics (Recommended)

Run full optimizations on a corpus of images, record dial values, compute covariance across all.

- One-time cost (completed)
- Produces a prior covariance matrix
- Use as initialization for all future images

**Pro**: Amortized cost, captures camera/pipeline-specific structure
**Con**: Assumes covariance is stable across images

## Prior Covariance Matrix

The empirical correlation matrix is stored in `etc/aceo_full.json`. This 45×45 matrix covers all style dials and can be used to:

1. Initialize CMA-ES covariance matrix
2. Transform search space to decorrelate dials
3. Guide step directions during optimization

**Bootstrapping:** When `etc/aceo_full.json` doesn't exist, ACEO uses identity matrix (no prior correlations) and can accumulate covariance during runs.

## SPSA + ACEO: Complementary Pair

SPSA and ACEO form a complementary optimization pair. SPSA explores the full dial space and builds covariance; ACEO uses that covariance as a prior for efficient eigenspace search.

### SPSA Covariance Contribution

SPSA now accumulates dial samples during optimization using Welford's online algorithm. This captures the full 45D search space because:

- **SCENE_LINEAR mode** explores dials 0,1,2,8,9 (exposure, WB, clipping)
- **DISPLAY mode** explores dials 3-7, 10-44 (contrast, color, style)
- Combined samples from both phases produce a complete covariance matrix

### The Bootstrap Workflow

```bash
# Phase 1: SPSA bootstrap (explores full 45D space)
tune img1.ARW preview --save-area tmp --optimizer spsa --save-cov tmp/cov.json
tune img2.ARW preview --save-area tmp --optimizer spsa --save-cov tmp/cov.json

# Phase 2: ACEO refinement (uses SPSA-built prior)
tune img3.ARW preview --save-area tmp --optimizer aceo --with-cov tmp/cov.json --save-cov tmp/cov.json
```

The `bin/cvar.sh` script automates this workflow.

### Why This Works

| | SPSA | ACEO |
|--|------|------|
| Evaluations per iteration | 2 | λ (population, typically 10-20) |
| Covariance contribution | Builds 45×45 correlation matrix | Uses prior, refines in eigenspace |
| Search pattern | Phased blocks (full dial coverage) | 12D eigenspace projection |
| Memory | O(n) | O(n²) for covariance matrix |
| Role | Bootstrap exploration | Efficient refinement |

SPSA's phased optimization naturally explores all dials. ACEO's eigenspace projection requires a prior to know which directions matter. Together: SPSA builds the map, ACEO navigates it.

## Decision Criteria

**Measured.** Empirical covariance from 10 sample images shows:

- 359 strong correlation pairs (|r| > 0.3)
- Max correlation 0.979
- Selective_color block heavily coupled

**Conclusion**: ACEO will improve convergence quality over SPSA.

## Out of Scope

**Geometric dials (6)** are excluded from ACEO optimization:
- crop_top, crop_right, crop_bottom, crop_left
- scale (zoom)
- tiltAngle (rotation)

These are user composition choices, not style parameters. The user frames the shot; ACEO matches the style.

## Status

**Implemented.** Three optimizer modes available:
- `--optimizer spsa` - Phased SPSA (default)
- `--optimizer aceo` - CMA-ES eigenspace
- `--optimizer hybrid` - ACEO for direction/pop, then SPSA for polish

### Implementation Details (Full ACEO - 45 dials)

Full ACEO optimizes all 45 style dials in a unified eigenspace:

| Block | Dials | Count |
|-------|-------|-------|
| ColorCorrection | exposure, temperature, tint | 3 |
| ToneMapping | contrast, highlights, shadows, toe, shoulder, black, white | 7 |
| GlobalColor | vibrance, saturation, density | 3 |
| SplitTone | shadow_temp, shadow_tint, highlight_temp, highlight_tint | 4 |
| SelectiveColor | 8 hues × (hue, sat, lum) | 24 |
| Detail | sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma | 4 |
| **Total** | | **45** |

The 45D dial space is expected to have ~12 effective dimensions (99% variance):

| Variance Captured | Dimensions (expected) |
|-------------------|----------------------|
| 80% | ~5 |
| 95% | ~8 |
| 99% | ~12 |

- `src/main/part/geos/aceo.hpp` - ACEO interface, 45-dial mapping
- `src/main/part/geos/aceo.cpp` - Eigenspace implementation:
  - Jacobi eigendecomposition of prior correlation (45×45)
  - Project 45D → 12D eigenspace
  - CMA-ES with eigenvalue-weighted sampling
  - CSA (Cumulative Step-size Adaptation)
  - Online covariance accumulator (Welford's algorithm)

### Performance Comparison

| Image | ACEO | SPSA |
|-------|------|------|
| DSC00202 | 0.19% (220 evals) | 0.09% (100 evals) |

### Why SPSA Wins (currently)

SPSA uses **phased optimization** aligned with **loss function structure**:
1. Phase 1: ToneMapping (exposure/contrast)
2. Phase 2: GlobalColor (saturation/density)
3. Phase 3: Regional refinement

ACEO uses **eigenspace** aligned with **dial correlation structure** - captures how dials move together across images, but not how they affect the loss.

### HYBRID Mode (Implemented)

**The hybrid approach combines ACEO's direction-finding with SPSA's polish:**

1. **Phase 1: ACEO** (half iterations)
   - Use eigenspace search for fast convergence to correct "pop"
   - ACEO excels at finding the right direction

2. **Phase 2: SPSA** (remaining iterations)
   - Polish from ACEO's position
   - SPSA excels at photographic quality refinement

```bash
# Use hybrid mode
tune img.ARW preview --save-area tmp --full --optimizer hybrid --with-cov etc/aceo_full.json
```

This gives ACEO's speed at finding color vibrancy + SPSA's photographic quality polish.

### Online Covariance Accumulator

ACEO includes a built-in covariance accumulator using Welford's algorithm for numerically stable online computation:

```cpp
struct CovarianceAccumulator {
    void update(const VectorN& sample);       // Add sample (45D)
    bool getCorrelation(MatrixN& corr);       // Extract correlation matrix (45×45)
    bool blendWithPrior(prior, alpha, result); // Mix accumulated + prior
    bool saveToJson(const std::string& path); // Persist to file
};
```

During optimization, top-μ samples from each generation are accumulated. This enables:
- Learning covariance from optimization runs (no external scripts)
- Adaptive eigenspace (blend prior + observed)
- Building better priors from ACEO-optimized samples
- Bootstrapping from identity when no prior exists

**Usage:**
```bash
# Bootstrap: run ACEO with identity prior, save learned covariance
tune img1.ARW preview --save-area ./out --optimizer aceo --save-cov tmp/cov1.json

# Chain: blend with prior, accumulate more samples
tune img2.ARW preview --save-area ./out --optimizer aceo --with-cov tmp/cov1.json --save-cov tmp/cov2.json
tune img3.ARW preview --save-area ./out --optimizer aceo --with-cov tmp/cov2.json --save-cov tmp/cov3.json

# Final becomes the production prior
cp tmp/cov3.json etc/aceo_full.json

# Subsequent runs use etc/aceo_full.json automatically
tune photo.ARW preview --save-area ./out --optimizer aceo
```

**Blending:** When `--with-cov` is specified, accumulated samples are blended with the prior using α = min(1, n/500) where n is the sample count. More samples = more weight on accumulated.

## Files

- `etc/aceo_full.json` - Prior covariance matrix (45×45)
- `etc/aceo.json` - Legacy prior (36×36, historical)
- `bin/cvar.sh` - Covariance builder script (SPSA bootstrap → ACEO refinement)
- `opt/cov.sh` - Legacy covariance measurement script

## See Also

- [spsa.md](./spsa.md) - Current optimization model
- [geos.md](./geos.md) - Loss function and feature space
- [tune.md](./tune.md) - Tuning pipeline
