# Next Steps

## Current State (2025-12-04)

**BASE** = e9eae97 (verified baseline)
**HEAD** = 2ece566 (axis contrast integrated, disabled)

### Baseline Metrics
| Image | Loss | Notes |
|-------|------|-------|
| DSC00144 | 13.7% | Hard image - narrow gamut |
| DSC00202 | 3.2% | Excellent |
| DSC01531 | 8.1% | Foliage/DRO |

## Hypothesis 3: Axis Contrast Preservation

**Status:** Code integrated, DISABLED (weight=0), ready to test

**Finding:** DSC01531's R-C axis collapses -65% during optimization. Red window frames lose saturation while greens boost. This is the "averaging" problem.

### To Enable and Test:

1. Find where `evaluateCombinedLoss` is called in optimization loop (task.cpp or aceo.cpp)

2. At startup, measure target axis contrast:
```cpp
cv::UMat targetProxy = resizeProxy(targetBGR);
AxisContrast targetAxis = measureAxisContrast(targetProxy);
```

3. Pass to loss function:
```cpp
float loss = evaluateCombinedLoss(body, targetStyle, targetLapVar,
    FREQ_WEIGHT, &targetAxis, 0.15f);  // 15% axis weight
```

4. Test on DSC01531:
```bash
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune \
  ~/base/pics/pqtr-test/DSC01531.ARW preview \
  --save-area tmp/var/tune/DSC01531
```

5. Compare R-C axis before/after with axis_balance test

### Expected Outcome:
- DSC01531: R-C axis preserved, loss should decrease
- DSC00202: No regression (already good, low axis contrast)
- DSC00144: No change (problem is not axis collapse)

## Other Notes

- DSC00144's 13.7% is NOT an axis problem - axes are preserved
- DSC00202 over-saturates B-Y axis (+122%) - may need ceiling
- Hypothesis test code backed up in `/tmp/hypothesis_backup/`

## Files to Review

- `doc/idea.md` - Full hypothesis documentation with empirical data
- `src/main/part/geos/diff.hpp` - AxisContrast struct
- `src/main/part/geos/spsa.cpp` - evaluateCombinedLoss with axis support
- `src/test/geos/axis_balance.cpp` - Test harness
