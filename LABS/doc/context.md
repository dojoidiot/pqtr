# Context: Three-Phase Architecture

## Current State (2024-12-02)

### The Solution: Polynomial + Dials

After extensive research into Sony's ISP and reverse-engineering attempts, the solution is a **three-phase architecture**:

```
RAW
 ↓
[Phase 0: Camera Math] - 30-coefficient polynomial transform
     • Estimated from flat→preview pixel correspondence
     • Captures color matrix + tone curve + cross-channel effects
     • Deterministic, no optimization
     • Results: 2-13% error depending on scene
 ↓
[Phase 1: Camera Vibe] - 45 dials optimized to match preview
     • Starts from poly output (not flat)
     • Closes gap on remaining differences
     • Results: matches or slightly improves poly error
 ↓
[Phase 2: User Vibe] - 45 dials optimized to match user edit
     • Only runs if target != preview
     • Captures creative intent
 ↓
Output
```

### Results (2024-12-02)

| Image | After Poly | After Dials | Notes |
|-------|-----------|-------------|-------|
| DSC00202 | 2.6% | 3.2% | Excellent - green foliage, neutral wood |
| DSC00144 | 12.8% | 13.7% | Higher error - different scene characteristics |
| DSC01531 | 8.1% | 7.9% | Good - foliage scene, better than expected |

### Key Insight

The polynomial transform captures the **global RGB→RGB mapping** that the camera applies:
- Color matrix (linear terms)
- Tone curve nonlinearity (quadratic terms)
- Cross-channel interactions (product terms)

This is **deterministic** - estimated directly from pixel correspondence, no optimization needed.

The 45 dials then handle the **residual differences** - small adjustments that the polynomial can't capture (DRO spatial effects, creative style nuances).

## Pipeline Architecture

```
tune <source.ARW> <target|preview> --save-area <dir>
  Phase 0: Camera Math - apply polynomial from GEAR
  Phase 1: Camera Vibe - optimize dials to match preview
  Phase 2: User Vibe - optimize dials to match target (if not preview)
  Output: tune.json with poly_coeffs + dials

labs <source.ARW> --tune <tune.json> --output <out.png> [--debug]
  Loads poly_coeffs and dials from tune.json
  Debug outputs: head.png (preview), tail.png (output), diff.png
```

## Key Files

- `GEAR/src/main/raws.cpp` - estimates polyCoeffs[30] from flat→preview
- `LABS/src/main/part/pipe/mods/poly_color.cpp` - applies polynomial transform
- `LABS/src/main/part/geos/data.cpp` - serializes poly_coeffs to/from JSON
- `LABS/src/main/tune.cpp` - three-phase optimizer
- `LABS/src/main/labs.cpp` - pipeline runner

## Test Command

```bash
cd /home/z/base/code/pqtr/LABS

# Run tune (creates tune.json with poly_coeffs + dials)
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune var/pics/DSC00202.ARW preview --save-area tmp/var/tune/DSC00202

# Run labs with debug output
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/labs var/pics/DSC00202.ARW --tune tmp/var/tune/DSC00202/tune.json --output tmp/var/tune/DSC00202/out.png --debug
```

## Jacobian Retraining

The Jacobian (`etc/jacob.json`) was trained assuming dials start from flat baseline. With polynomial applied first, the starting point is different. Consider retraining for optimal convergence, though current results are functional.

## What Didn't Work (Historical)

1. **BaseCurve estimation** - Per-channel curves added color casts (yellow/green)
2. **Skip baseCurve** - Optimizer found bad local minima (pink/magenta)
3. **Camera-like initialization** - Optimizer reduced contrast, added tints
4. **Direct 3D LUT** - 96% of cells empty, sparse coverage fails

The polynomial approach succeeds because it:
- Uses all pixels (dense coverage)
- Captures cross-channel effects (not just per-channel)
- Is estimated directly (not optimized)
