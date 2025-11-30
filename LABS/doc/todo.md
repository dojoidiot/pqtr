# LABS TODO

## Current: Base Curve Learning

### Discovery (2024-11-30)

The "washed out" problem is NOT an optimizer issue - it's a **pipeline capability gap**.

**Measured:** Our max std_L = 0.1303, target = 0.2244 (42% shortfall)

**Root cause:** Camera JPEGs apply a base tone curve (chosen by photographer via picture style) BEFORE any adjustments. We start from a flat baseline.

**Key insight:** The base curve represents **photographer intent** - it's what they SAW when composing the shot and what they EXPECT as their editing baseline.

See: `doc/base_curve.md` for full analysis.

---

## Discussion Topics for Next Session

### 1. Base Curve Architecture

Where in the pipeline should the base curve be applied?

| Option | Location | Pros | Cons |
|--------|----------|------|------|
| A | After demosaic, before color matrix | Physically correct | Complex interaction with WB |
| B | After color matrix, before dials | Clean separation | May need per-channel curves |
| C | Modify dial ranges | Simple | Less flexible |
| D | Early 3D LUT | Captures all transforms | Expensive, opaque |

### 2. Learning Strategy

How do we learn base curves from RAW+JPEG pairs?

| Option | Method | Complexity | Quality |
|--------|--------|------------|---------|
| A | Parametric (contrast, gamma, points) | Low | Good |
| B | 1D LUT per channel | Medium | Better |
| C | 3D LUT | High | Best |
| D | Neural network | Very high | Overkill? |

### 3. EXIF Picture Style (VERIFIED)

Picture style IS available in EXIF:

```
Sony:  -CreativeStyle    → "Standard", "Vivid", etc.
Canon: -PictureStyle     → "Standard", "Portrait", etc.
Nikon: -PictureControlName
Fuji:  -FilmSimulation
```

Test images confirmed: `Creative Style: Standard` on all A7III shots.

### 4. Fallback Hierarchy

```
1. Exact: Sony/ILCE-7M3/{serial}/vivid.json  (bespoke tuning)
2. Model: Sony/ILCE-7M3/vivid.json
3. Make:  Sony/default.json
4. Universal: default.json
```

### 5. Service Model

**"YOUR Camera Tuned"** - Premium service for pros.

- Pro sends their camera body
- We shoot color card with THEIR sensor
- Deliver bespoke profile for their serial number
- Captures per-unit manufacturing variance

### 6. Equipment Needed

- X-Rite ColorChecker Classic (24 patch) - ~$70
- Constant D50/D65 light source
- Tripod + controlled environment

---

## Completed

- [x] 19D feature vector (sigma, LCH, percentiles, shadow chroma)
- [x] Weighted L2 loss with trained weights (cnst.json)
- [x] Built `bounds` diagnostic - found achievable limits
- [x] Documented base curve concept (`doc/base_curve.md`)
- [x] Training infrastructure (train-greedy, train-prms, train-exhaustive)
- [x] Hybrid mode: ACEO → SPSA polish
- [x] Full ACEO (45 dials) with covariance
- [x] `--full` mode for single-pass optimization

## Blocked (Until Base Curve)

- [ ] cnst/prms/cvar optimization (limited by achievable range)
- [ ] Batch tune quality (will remain "washed out")
- [ ] Sky banding artifacts
- [ ] Skin tone matching

## Deferred

- [ ] Re-enable regional refinement
- [ ] Per-dial learning rates (vs per-block)
