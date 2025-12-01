# LABS TODO

## Completed

- [x] 23D feature vector (sigma, LCH, percentiles, shadow chroma, split tone colors)
- [x] Weighted L2 loss with trained weights (cnst.json)
- [x] Training infrastructure (train-greedy, train-prms, train-exhaustive)
- [x] Hybrid mode: ACEO → SPSA polish
- [x] Full ACEO (45 dials) with covariance
- [x] `--full` mode for single-pass optimization
- [x] Built `bounds` diagnostic - found achievable limits
- [x] **Base curve implementation** (2024-12-01)
  - RAWS estimates curve per-image from RAW→preview
  - Curve stored in raws::Result, passed via pipe::Head
  - Applied in gamma space after colorCorrection
  - Loss dropped: 17.3% → 13.1%
- [x] **Resolution independence verified** (2024-12-01)
  - Tested embedded preview (1616×1080) vs full-res sidecar JPG
  - Curves match: L2 < 0.002 when both at preview size
  - Higher resolution adds alignment noise, not signal
  - Conclusion: Preview is sufficient for curve estimation
- [x] **Baseline guard** (2024-12-01)
  - Optimizer was sometimes making images worse (spectral→combined loss mismatch)
  - Added guard in task.cpp: if final > baseline, restore neutral dials
  - Now guarantees optimizer never degrades quality

## Current: Refinement

Now that base curve is working, the optimizer has more headroom.

### Baseline Results (2024-12-01)

With base curve + baseline guard:

| Baseline Range | Count | Status |
|----------------|-------|--------|
| 3-5% | 3 | Guard preserved (optimizer overshoots) |
| 5-13% | 5 | Improved by optimizer |
| 35% | 1 | DSC01531 - complex colors (saturated greens/reds) |

**DSC01531 outlier (35.81%):** Not a curve failure - the image has complex cross-channel color relationships that per-channel curves can't capture. Needs color matrix estimation.

### Next Steps

1. **Re-run bounds analysis** with base curve active
   - Check if previously unreachable features are now achievable
   - Identify remaining gaps

2. **Retrain weights** (cnst.json)
   - Feature importance may have shifted
   - Batch tune with base curve, analyze residuals

3. **Batch quality assessment**
   - Run tune on all 10 test images
   - Compare before/after base curve
   - Document typical loss range

### Deferred

- [ ] Re-enable regional refinement
- [ ] Per-dial learning rates (vs per-block)
- [ ] Sky banding artifacts (may be resolved by base curve)
- [ ] Skin tone matching (may improve with better baseline)
- [x] **RAWS hasBaseCurve bug** - was stale library, fixed by rebuild

---

## Architecture Notes

### Base Curve Flow

```
RAWS (camera-specific):
  - Decodes RAW
  - Extracts embedded JPEG preview
  - Estimates per-channel curves from flat→preview (768 floats: BGR × 256)
  - Returns baseCurve[768] in Result

LABS (generic):
  - Head stores curve from Result
  - Link.baseCurve().setCurve(head->baseCurve())
  - Applied in gamma space after colorCorrection, before toneMapping
```

### Key Files Changed

- `RAWS/inc/raws.hpp` - Result has baseCurve[768], hasBaseCurve
- `RAWS/src/main/raws.cpp` - estimateBaseCurve() function
- `LABS/inc/pipe.hpp` - Head::baseCurve(), Link::BaseCurve simplified
- `LABS/src/main/part/pipe/pipe.cpp` - HeadImpl stores/exposes curve
- `LABS/src/main/part/pipe/link.cpp` - BaseCurveImpl applies curve
- `LABS/src/main/part/pipe/mods/base_curve.cpp` - gamma-space LUT apply
- `LABS/src/main/part/geos/task.cpp` - baseline guard (line 179)
