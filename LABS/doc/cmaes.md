# CMA-ES: Covariance-Aware Optimization

[back](../README.md)

## Context

SPSA treats all dials as potentially coupled but searches with random hypercube corner perturbations (±1 per dial). If dials covary strongly, SPSA wastes steps bouncing off tilted valleys in the loss landscape.

CMA-ES (Covariance Matrix Adaptation Evolution Strategy) learns the covariance structure and adapts its search to align with the landscape.

## The Covariance Question

Image processing dials are not independent:
- Exposure affects contrast perception
- Saturation affects hue purity
- These couplings come from pipeline math and human perception

If dials covary, the loss landscape has tilted elliptical contours. An optimizer that knows the tilt can step along valleys instead of across them.

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
- Build covariance estimate in ~34 evaluations (2 per dial)
- Use to initialize search

**Pro**: Cheap, image-specific
**Con**: Covariance might vary at different dial positions

### Option 3: Offline from Sample Pics

Run full optimizations on a corpus of images, record dial trajectories, compute covariance across all.

- One-time cost
- Produces a prior covariance matrix
- Use as initialization for all future images

**Pro**: Amortized cost, captures camera/pipeline-specific structure
**Con**: Assumes covariance is stable across images

## Recommended Approach

**Option 3 is most promising.**

Camera behavior is consistent. The coupling between exposure/contrast or saturation/hue is a property of the pipeline, not the scene. Build the covariance matrix once from sample pics. Use it as a warm start.

## SPSA vs CMA-ES Trade-offs

| | SPSA | CMA-ES |
|--|------|--------|
| Evaluations per iteration | 2 | λ (population, typically 10-20) |
| Covariance adaptation | None | Learned |
| Memory | O(n) | O(n²) for covariance matrix |
| Best when | Weak coupling | Strong coupling |

## Decision Criteria

**Measure first.** Run batch optimization, record dial trajectories, compute empirical covariance.

- If strong off-diagonal terms (|correlation| > 0.3): CMA-ES will help
- If nearly diagonal: SPSA is already near-optimal, complexity not worth it

## Status

**Proposed.** Pending empirical covariance measurement from sample pics.

## See Also

- [spsa.md](./spsa.md) - Current optimization model
- [geos.md](./geos.md) - Loss function and feature space
- [tune.md](./tune.md) - Tuning pipeline
