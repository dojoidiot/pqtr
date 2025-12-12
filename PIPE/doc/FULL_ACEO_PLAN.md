# Full ACEO Plan

## Status: Phase 4 (Evaluation)

All code complete. Currently evaluating results.

**Observation:**
- ACEO hits correct "pop" (color vibrancy) but looks raw
- SPSA produces photographic quality but wrong pop
- Investigating: step size, generations, smoothness term, or hybrid approach

## Architecture

**Single holistic optimization:**
```
ACEO(45 dials) → single eigenspace (~12D) → combined loss (spectral + frequency)
```

**Excluded:** Geometric dials (crop, scale, tilt) - user composition choices

## Dial Set (45 total)

| Block | Count | Dials |
|-------|-------|-------|
| Scene-Linear | 5 | exposure, temperature, tint, black_point, white_point |
| ToneMapping | 7 | contrast, highlights, shadows, toe_pivot, shoulder_pivot, toe_strength, shoulder_strength |
| GlobalColor | 3 | vibrance, saturation, density |
| SplitTone | 4 | shadow_temp, shadow_tint, highlight_temp, highlight_tint |
| SelectiveColor | 24 | 8 hues × (hue_shift, saturation, luminance) |
| Detail | 4 | sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma |

## Completed Work

### Phase 0: Online Covariance Accumulator
- [x] Welford's algorithm for numerically stable online mean/covariance
- [x] Collects top-μ samples each generation during optimization
- [x] `blendWithPrior()` for adaptive eigenspace
- [x] `saveToJson()` for persisting learned covariance

### Phase 1: Extend to 45 Dials
- [x] Update `ACEO_DIAL_MAP` in `aceo.hpp` to include all 45 dials
- [x] Add scene-linear dials: exposure, temperature, tint, black_point, white_point
- [x] Add detail dials: sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma
- [x] Update `GEOS_DIAL_COUNT` from 41 to 45 in `spsa.hpp`
- [x] Add detail dials to `readDials()`/`writeDials()` in `spsa.cpp`
- [x] Update `aceo.cpp` types: `VectorN` (45), `MatrixN` (45×45), `EIGEN_DIM=12`
- [x] Identity fallback when `etc/aceo_full.json` not found (bootstrapping)
- [x] Generate `etc/aceo_full.json` (45×45 covariance matrix)
- [x] Analyze eigenstructure (~12D for 99% variance confirmed)

### Phase 2: Unified ACEO Optimizer
- [x] Update `aceo.hpp` with 45-dial mapping
- [x] Load 45×45 covariance from `etc/aceo_full.json`
- [x] Single eigenspace projection (45D → ~12D)
- [x] Combined loss function (spectral + 0.15×frequency)

### Phase 3: Simplify Pipeline
- [x] Add `--full` mode to tune.cpp
- [x] Single link instead of linear+display
- [x] Skip LUT estimation in --full mode
- [x] Edge dials optimized holistically (not separate EDGE phase)

### Phase 4: Evaluate (IN PROGRESS)
- [x] Compare with current SPSA pipeline
- [ ] Investigate ACEO "raw" quality vs SPSA "photographic" quality
- [ ] Potential: hybrid approach (ACEO for pop, SPSA for polish)

## Key Files

```
etc/aceo_full.json              # 45×45 covariance matrix
src/main/part/geos/aceo.hpp     # ACEO interface
src/main/part/geos/aceo.cpp     # ACEO implementation (combined loss)
src/main/part/geos/spsa.cpp     # evaluateCombinedLoss()
src/main/part/geos/task.cpp     # Edge pass skipped in holistic mode
src/main/tune.cpp               # --full mode
bin/cvar.sh                     # Covariance building script
```

## Usage

```bash
# Build covariance (SPSA bootstrap + ACEO refinement)
./bin/cvar.sh var/pics

# Run holistic optimization
tune photo.ARW preview --save-area tmp --full --optimizer aceo --with-cov etc/aceo_full.json

# With visual output
tune photo.ARW preview --save-area tmp --full --fine --optimizer aceo --with-cov etc/aceo_full.json
```

## Open Question

**Why does ACEO hit correct pop but look raw, while SPSA looks photographic but wrong pop?**

The covariance captures the right direction (ACEO can follow it to correct pop), but SPSA's gradient-free random walk finds smoother paths. ACEO may be "teleporting" to mathematically optimal points that lack photographic polish.

Options:
1. Smaller ACEO steps / more generations
2. Smoothness term in loss function
3. Hybrid: ACEO for direction, SPSA for polish
