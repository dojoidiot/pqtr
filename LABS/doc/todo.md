# LABS TODO

## Current: Visual Testing

Testing 12D feature vector and hybrid mode.

## Done

- [x] 12D feature vector (added mu_a, mu_b for color cast penalty)
- [x] Hybrid mode: ACEO → SPSA polish (`--optimizer hybrid`)
- [x] Full ACEO (45 dials) with edge dials unified holistically
- [x] `etc/aceo_full.json` (45×45 covariance matrix)
- [x] Eigenstructure validated (~12D for 99% variance)
- [x] Combined loss (spectral + 0.15×frequency) for holistic optimization
- [x] `--full` mode for single-pass 45-dial optimization
- [x] SPSA bootstrap for initial covariance
- [x] ACEO/CMA-ES covariance training
- [x] Store trained covariance in `etc/aceo.json`

## Deferred

- [ ] Re-enable regional refinement (after visual quality sorted)
