# Full ACEO Plan

## Vision

Replace the multi-stage pipeline with a single holistic ACEO optimization that includes ALL dials (color, tone, AND sharpness) in one eigenspace.

**Current pipeline:**
```
LINEAR(5 dials) → LUT → DISPLAY(36 dials) → EDGE(2 dials)
= 4 stages, separate optima, good numbers but poor visual acceptance
```

**Target pipeline:**
```
ACEO(45 dials)
= 1 stage, holistic optimum, possibly worse numbers but better visual acceptance
```

## Rationale

1. **Reference has all information** - We're matching a known target, not inventing transforms
2. **Covariance captures structure** - Eigenspace reduces 45D → ~12D
3. **LUT is a crutch** - Hides dial insufficiency; if ACEO can't match, we need more dials
4. **Sharpness affects color perception** - Separate optimization misses this coupling
5. **Numbers ≠ eyes** - Holistic optimization may find better perceptual trade-offs

## Dial Set (45 total)

All already captured in tune.json!

| Block | Count | Dials |
|-------|-------|-------|
| Scene-Linear | 5 | exposure, temperature, tint, black_point, white_point |
| ToneMapping | 7 | contrast, highlights, shadows, toe_pivot, shoulder_pivot, toe_strength, shoulder_strength |
| GlobalColor | 3 | vibrance, saturation, density |
| SplitTone | 4 | shadow_temp, shadow_tint, highlight_temp, highlight_tint |
| SelectiveColor | 24 | 8 hues × (hue_shift, saturation, luminance) |
| Detail | 4 | sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma |

**Note:** tune.json already contains all 45 dials.

## Out of Scope

**Geometric dials (6)** are excluded from optimization:
- crop_top, crop_right, crop_bottom, crop_left
- scale (zoom)
- tiltAngle (rotation)

These are user composition choices, not style parameters. The user frames the shot; ACEO matches the style.

## Implementation Steps

### Phase 0: Online Covariance Accumulator ✓ DONE

Added `CovarianceAccumulator` to `aceo.cpp`:
- [x] Welford's algorithm for numerically stable online mean/covariance
- [x] Collects top-μ samples each generation during optimization
- [x] `blendWithPrior()` for adaptive eigenspace
- [x] `saveToJson()` for persisting learned covariance

**Usage:**
```bash
ACEO_SAVE_COV=tmp/aceo_learned.json tune photo.ARW preview --optimizer aceo
```

This replaces the need for external Python scripts (`opt/cov.sh`).

### Phase 1: Extend to 45 Dials ✓ DONE

Code changes complete:
- [x] Update `ACEO_DIAL_MAP` in `aceo.hpp` to include all 45 dials
- [x] Add scene-linear dials: exposure, temperature, tint, black_point, white_point
- [x] Add detail dials: sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma
- [x] Update `GEOS_DIAL_COUNT` from 41 to 45 in `spsa.hpp`
- [x] Add detail dials to `readDials()`/`writeDials()` in `spsa.cpp`
- [x] Update `aceo.cpp` types: `VectorN` (45), `MatrixN` (45×45), `EIGEN_DIM=12`
- [x] Identity fallback when `etc/aceo_full.json` not found (bootstrapping)

Remaining:
- [ ] Run ACEO on multiple images, save covariance
- [ ] Generate `etc/aceo_full.json` (45×45 matrix)
- [ ] Analyze eigenstructure (expect ~12D for 99% variance)

### Phase 1b: Refine Covariance (ACEO-derived)
- [ ] Run Full ACEO on same images using v1 covariance
- [ ] Collect ACEO-optimized tune.json files
- [ ] Re-measure covariance → `etc/aceo_full_v2.json`
- [ ] Compare v1 vs v2 eigenstructure
- [ ] v2 should have cleaner correlations (less SPSA noise/artifacts)

### Phase 2: Unified ACEO Optimizer
- [ ] Update `aceo.hpp` with 45-dial mapping
- [ ] Load 45×45 covariance from `etc/aceo_full.json`
- [ ] Single eigenspace projection (45D → ~12D)
- [ ] Combined loss function (spectral + frequency, weighted)

### Phase 3: Simplify Pipeline
- [ ] Add `--full-aceo` mode to tune.cpp
- [ ] Single link instead of linear+display
- [ ] Skip LUT estimation
- [ ] Direct edge optimization in ACEO (not separate EDGE phase)

### Phase 4: Evaluate
- [ ] Compare with current SPSA+LUT pipeline
- [ ] Measure: final loss, visual acceptance, convergence speed
- [ ] If dial set insufficient, identify gaps → add targeted dials

## Key Files

```
doc/FULL_ACEO_PLAN.md          # This plan
etc/aceo.json                   # Current 36×36 covariance
etc/aceo_full.json              # Target 45×45 covariance (to create)
opt/cov.sh                      # Covariance measurement script (to modify)
src/main/part/geos/aceo.hpp     # ACEO interface (to extend)
src/main/part/geos/aceo.cpp     # ACEO implementation (to extend)
src/main/tune.cpp               # CLI (add --full-aceo mode)
```

## Recovery Instructions

**To resume this work in a new context:**

```
Read doc/FULL_ACEO_PLAN.md for the plan.
Current state: Phase 0+1 complete (45 dials), need to generate covariance.
Next step: Run ACEO on images, save covariance to etc/aceo_full.json.

Key context:
- Full ACEO implemented: 45 dials, 12D eigenspace
- Online CovarianceAccumulator (Welford's algorithm)
- Identity matrix used when etc/aceo_full.json not found (bootstrapping)
- Geometric dials (6) excluded - user composition choices
- Currently SPSA beats ACEO on numbers (0.09% vs 0.19%)
- Hypothesis: holistic 45-dial optimization will find better perceptual trade-offs

To bootstrap covariance:
  ACEO_SAVE_COV=etc/aceo_full.json tune photo.ARW preview --optimizer aceo
```

## Open Questions

1. **Loss weighting** - How to balance spectral vs frequency in combined loss?
2. **Perceptual metric** - Should we use a perceptual loss (SSIM, LPIPS) instead?
3. **Edge dial range** - Current edge dials may need rescaling for eigenspace
4. **Denoise** - Include denoise dial? (adds coupling with sharpness)

## Success Criteria

1. Single ACEO pass achieves ≤0.5% combined loss
2. Visual acceptance improves (subjective evaluation)
3. Convergence in ≤500 evaluations
4. If dials insufficient, clear signal of what's missing
