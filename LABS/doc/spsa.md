# SPSA: The Optimization Model

[back](../README.md)

## The Problem

We have 45 style dials. Each dial is a dimension in a 45D hypercube, with values in [0, 1].

We have a reference image (the camera JPEG) that defines the target style. We want to find dial settings that make our processed image match this target.

## The Baseline: Independent Dials

If dials were independent:
- Each dial has one optimal value
- The difference between current and optimal can be expressed as an angle
- Finding the optimum means driving that angle to zero
- We could solve 45 separate 1D searches

This is the simple case. It is not reality.

## The Reality: Dependent Dials

Dials are coupled. Changing dial A shifts the optimal value of dial B.

This means:
- There is no fixed "target angle" per dial
- The optimal value of each dial depends on all other dials
- We cannot optimize dials independently

To even define dependence, we need a global optimum as reference. The reference image provides this. Without it, the dials are just 45 free parameters with no coupling defined.

## The Unified View

Instead of 45 separate angles, think of one angle in 45D space:

- Your current dial settings define a point in the hypercube
- The optimal dial settings define another point
- The difference between them is a vector
- Optimization drives this vector toward zero

One search. One angle. 45 coupled dials.

## What SPSA Does

SPSA (Simultaneous Perturbation Stochastic Approximation) finds dial settings that minimize the angle between candidate and target, accounting for dial interdependence.

The key insight: instead of computing 45 partial derivatives (one per dial), SPSA estimates the full gradient with just 2 evaluations:

1. Perturb all 45 dials randomly by ±c
2. Evaluate loss at both perturbations
3. The difference reveals the gradient direction
4. Update all dials along that direction

This treats the 45D problem as one unified search, not 45 separate ones. The random perturbations naturally capture the coupled structure—sometimes moving along valleys of dependence, sometimes across them, averaging out to the true descent direction.

## Phased Optimization

SPSA uses phased optimization aligned with loss function structure:

1. **Phase 1: ToneMapping** - Exposure, contrast, tone curve dials
2. **Phase 2: GlobalColor** - Saturation, vibrance, density dials
3. **Phase 3: Regional refinement** - Selective color HSL dials

This phasing builds covariance information that can be used by ACEO.

## Covariance Accumulation

SPSA accumulates dial samples during optimization using Welford's online algorithm:

```bash
# Save covariance from SPSA run
tune img.ARW preview --save-area tmp --optimizer spsa --save-cov tmp/cov.json
```

The covariance captures which dials move together across samples, enabling ACEO eigenspace search.

## The Goal

Find dial settings where the angle (loss) is as close to zero as possible.

Zero angle = candidate matches target in 12D feature space = style matched.

## See Also

- [geos.md](./geos.md) - Full mathematical foundation (12D features, cosine loss)
- [aceo.md](./aceo.md) - CMA-ES eigenspace optimization using SPSA-built covariance
- [tune.md](./tune.md) - How SPSA fits into the tuning pipeline
