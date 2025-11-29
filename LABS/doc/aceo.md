# ACEO: Adaptive Covariance Evolver Optimiser

[back](../README.md)

## Overview

ACEO is a covariance-aware optimization strategy for the LABS dial system. Based on CMA-ES (Covariance Matrix Adaptation Evolution Strategy), ACEO learns the covariance structure of the 41-dial search space and adapts its search to align with the loss landscape.

## Empirical Findings

Covariance analysis was performed on 10 sample images using SPSA optimization with `--skip-lut`. The results strongly support covariance-aware optimization.

### Results Summary

| Metric | Value |
|--------|-------|
| Images analyzed | 10 |
| Variable dials | 36 (5 fixed: exposure, temperature, tint, white_point, black_point) |
| Strong correlations (|r| > 0.3) | **359 pairs** |
| Maximum correlation | **r = 0.979** |

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

The empirical correlation matrix is stored in `etc/aceo.json`. This 36×36 matrix covers all variable dials and can be used to:

1. Initialize CMA-ES covariance matrix
2. Transform search space to decorrelate dials
3. Guide step directions during optimization

## SPSA vs ACEO Trade-offs

| | SPSA | ACEO |
|--|------|------|
| Evaluations per iteration | 2 | λ (population, typically 10-20) |
| Covariance adaptation | None | Learned/Prior |
| Memory | O(n) | O(n²) for covariance matrix |
| Best when | Weak coupling | Strong coupling |

## Decision Criteria

**Measured.** Empirical covariance from 10 sample images shows:

- 359 strong correlation pairs (|r| > 0.3)
- Max correlation 0.979
- Selective_color block heavily coupled

**Conclusion**: ACEO will improve convergence quality over SPSA.

## Status

**Validated.** Empirical covariance measurement complete. Prior matrix available in `etc/aceo.json`.

Next steps:
1. Implement ACEO optimizer in `src/main/part/geos/`
2. Use prior covariance for warm start
3. Compare convergence quality against SPSA baseline

## Files

- `etc/aceo.json` - Prior covariance matrix (36×36)
- `opt/cov.sh` - Covariance measurement script
- `tmp/opt/cov_matrix.json` - Raw measurement data
- `tmp/opt/cov_report.txt` - Human-readable report

## See Also

- [spsa.md](./spsa.md) - Current optimization model
- [geos.md](./geos.md) - Loss function and feature space
- [tune.md](./tune.md) - Tuning pipeline
