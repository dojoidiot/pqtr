# Next Steps

## Current State (2025-12-10)

### Incremental Camera Profiles - IMPLEMENTED

Camera profiles now learn incrementally from each processed image:

```
~/.pqtr/var/profiles/Sony_ILCE-7M4_Standard.json
```

**Note:** DRO excluded from profile key - it's spatially-varying (per-pixel local tone mapping) and can't be captured in a global LUT. All DRO levels mix into one profile. Accept ~11% error floor for high-DR scenes.

**Lifecycle:**
1. Cold start: identity LUT, full dial optimization
2. Learning: each image accumulates into 17³ LUT grid
3. Converged: profile frozen when delta < 0.1% and coverage > 70%
4. Frozen: just apply LUT, skip optimization

**Key files:**
- `RAWS/inc/raws.hpp` - CameraLut struct with convergence tracking
- `RAWS/src/main/raws.cpp` - tune(), save(), load(), snapshot(), computeDelta()
- `LABS/src/main/tune.cpp` - Profile integration in PHASE 0

### Research Validated ✓

| Image | Scene | 3D LUT | Final | Notes |
|-------|-------|--------|-------|-------|
| DSC00202 | Urban | 5.2% | **3.6%** | Excellent |
| DSC00012 | Indoor | ~7% | **5.2%** | Good |
| DSC00144 | High-DR | 11.6% | **11.1%** | DRO floor |

### Current Issues

**HSV LUT disabled** - 73° hue shifts, needs debugging

**ACEO bypassed** - Falls through to SPSA (test block unreachable)

**Profile LUT application** - TODO: Wire CameraLut to Link's lut3d module (currently using legacy poly_coeffs fallback)

---

## Implementation Priority

### 1. Wire CameraLut to lut3d module

Currently profiles accumulate but aren't applied. Need to:
1. Add `setLut()` method to Link's Lut3d module
2. Call it in tune.cpp PHASE 0 when profile has coverage > 30%
3. Test: run same image twice, second run should start with lower error

### 2. Test incremental learning

```bash
cd LABS
# Process first image
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune var/pics/DSC00144.ARW preview

# Check profile created
cat ~/.pqtr/var/profiles/*.json | head -20

# Process second image (should accumulate)
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune var/pics/DSC00202.ARW preview

# Check coverage increased
cat ~/.pqtr/var/profiles/*.json | grep coverage
```

### 3. Debug HSV LUT (optional)

The 73° hue shifts suggest estimation bug. Low priority - 3D LUT alone gets <5%.

### 4. Implement axis contrast loss (deferred)

For R-C axis collapse on DSC01531. Not blocking - profiles should help.

---

## Architecture Summary

```
TUNE Flow:
  1. Load profile (key: make_model_style_dro)
  2. Apply accumulated LUT (if coverage > 30%)
  3. Optimize dials (skip if frozen)
  4. Accumulate into profile
  5. Save profile (auto-freeze at convergence)

Profile Convergence:
  - Minimum 10 samples
  - Delta < 0.1% (average cell change)
  - Coverage > 70%
  → frozen = true, skip future optimization
```

---

## What's Working

- ✅ CameraLut accumulator with convergence tracking
- ✅ Profile save/load to JSON
- ✅ Key generation from EXIF (camera + style + DRO)
- ✅ Integration in tune.cpp PHASE 0
- ✅ Auto-freeze when converged

## What's TODO

- ⏳ Apply profile LUT via Link's lut3d module
- ⏳ Test end-to-end profile learning
- ⏳ HSV LUT debugging (low priority)

---

## Build & Test

```bash
bash wire.sh && make labs

cd LABS
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune var/pics/DSC00144.ARW preview
```
