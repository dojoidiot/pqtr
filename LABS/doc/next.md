# Next Steps

## Current State (2025-12-05)

**BASE** = 435b907 (HYBRID fix committed)

### Latest Metrics

| Image | Poly | Final | Notes |
|-------|------|-------|-------|
| DSC00144 | 12.8% | **12.0%** | Hard image - narrow gamut |
| DSC00202 | 2.6% | **3.7%** | Green foliage, neutral wood |
| DSC01559 | 7.2% | **8.0%** | Higher resolution test |

### HYBRID Fix (Committed)

SPSA was resetting dials to neutral (`initNeutral`) at start, throwing away ACEO's progress in HYBRID mode. Changed to `readDials` to preserve current values.

Commit: 435b907 "Fix HYBRID mode: SPSA now preserves ACEO progress"

### Hypothesis 3: Axis Contrast (Removed)

Tested with 15% weight - made things worse. Code removed in fix commit.

## Next Ideas to Explore

### 1. Two-Pass Optimization

First pass: lock tone dials, only optimize color.
Second pass: lock color dials, only optimize tone.
Prevents color and tone from fighting each other.

### 2. Regional Weighting

Weight the loss by region saturation. Highly saturated regions (like green foliage) should contribute more to saturation dial gradients. Currently all pixels weighted equally.

### 3. Preserve Contrast Constraint

Add hard constraint that contrast dial can't drop below threshold. Prevents the "flattening" problem seen in DSC00144.

### 4. Separate Loss Functions for Dial Groups

- Color dials optimize against chroma features only
- Tone dials optimize against luminance features only
- Currently everything optimizes against everything, causing interference

## Observations

- **DSC00144 (narrow gamut)**: Boosting saturation globally flattens contrast to compensate. Needs decoupled optimization.

- **DSC00202 (green subject)**: Selective color works (green pops), but global tone dials lift background to match overall luminance. Needs regional awareness.

- **DSC01559 (high-res)**: 6000x4000 image, different lens characteristics.

## Test Outputs

```
tmp/var/tune/DSC00144/
  head.png    - camera preview (reference)
  tail.png    - pipeline output
  diff.png    - difference x5
  result.png  - final output
  tune.json   - dial settings

tmp/var/tune/DSC00202/
tmp/var/tune/DSC01559/
```
