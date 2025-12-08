# Next Steps

## Current State (2025-12-08)

### Research Validated ✓

| Image | Scene | 3D LUT | Final | Notes |
|-------|-------|--------|-------|-------|
| DSC00202 | Urban | 5.2% | **3.6%** | Excellent - matches research |
| DSC00012 | Indoor | ~7% | **5.2%** | Good |
| DSC00144 | High-DR | 11.6% | **11.1%** | DRO floor - expected |

**Key findings:**
1. **3D LUT does the heavy lifting** - Captures camera's global color transform
2. **<5% achievable on most images** - DSC00202 at 3.6% proves the system works
3. **~11% is the DRO floor** - High dynamic range scenes trigger spatially-varying processing we can't match globally
4. **Dials fine-tune, not transform** - Only ~0.5-1.5% improvement over LUT

### Current Issues

**HSV LUT disabled** - Was producing 73° hue shifts (too large), needs debugging

**ACEO bypassed** - Falls through to SPSA (original test block crashed)

**Memory corruption** - Crashes during cleanup after `[OK]`. Optimization completes successfully.

---

## Research Synthesis

### Core Finding: Architecture is Sound

| Aspect | Status |
|--------|--------|
| Pipeline order | Correct (matches darktable, Lightroom, DNG spec) |
| 45 dials | Complete Lightroom parity |
| RAWS→VIEW→POPS staging | Correct |

**The issue isn't missing capability - it's optimization target selection.**

### The Strategic Shift

**Stop:** Per-image optimization against embedded JPEGs
**Start:** Per-camera calibration + user vibes

```
CALIBRATION (once per camera):
  50-100 images → average dial settings → Sony_A7IV.vibe

PROCESSING (per image):
  RAWS → flat → apply camera vibe → done
```

This matches how darktable, Lightroom, and camera manufacturers work.

### Why DRO Creates a 15% Error Floor

Sony's DRO is **spatially-varying** (per-pixel local tone mapping based on neighborhood).
Our polynomial is **per-pixel global**: `f(R,G,B) → (R',G',B')`.

**No per-pixel global function can match spatially-varying processing.**

| Image | Scene | Polynomial Error |
|-------|-------|------------------|
| DSC00202 | Urban | 2.6% |
| DSC00144 | Mixed | 12.8% |
| DSC01531 | Foliage | 15%+ |

**Accept it.** Let dial optimization close the gap.

### Axis Contrast Preservation

**Problem:** Holistic optimization averages opponent color pairs instead of preserving both.

**Empirical finding (2025-12-04):**

| Image | Loss | Issue |
|-------|------|-------|
| DSC01531 | 8.1% | R-C axis collapsed -65% (wash-out) |
| DSC00202 | 3.2% | Over-saturation, not collapse |
| DSC00144 | 13.7% | Not an axis problem |

**Opponent axes** (from color science):
- R ↔ C (Red ↔ Cyan) - temperature
- G ↔ M (Green ↔ Magenta) - tint
- B ↔ Y (Blue ↔ Yellow) - strongest perceptual
- O ↔ T (Orange ↔ Teal) - cinematic

**Solution:** Add `axisContrastLoss()` to penalize collapsing axes when both poles exist in target.

### VIEW vs POPS Separation

**VIEW** (5 dials): Absolute tone structure
- Exposure, contrast, black, white, toe, shoulder
- Loss: percentiles, std_L, skew_L

**POPS** (40 dials): Relative color relationships
- "Greens pop 20% more than neutrals" (not "green chroma = 0.45")
- Loss: color ratios, not absolute values

**Current problem:** All 45 dials optimize against one loss. VIEW dials fight POPS dials.

**Fix:** Stage-aware optimization with separate loss functions per stage.

### Camera Processing Model

Cameras precompute LUTs at factory calibration:

```
Creative Style → selects one of ~10 precomputed {color_matrix, saturation_lut, tone_curve}
DRO Setting → selects local TM strength from {off, lv1..lv5, auto}
Scene Mode → may override style selection
```

The "magic" is **LUT selection**, not per-pixel optimization. We reverse-engineer the **result**.

---

## Implementation Priority

### 1. Fix aceo.cpp crash (BLOCKING)

Location: `LABS/src/main/part/geos/aceo.cpp:576`

The greedy optimization test block causes segfault in `writeDials()`. Options:
1. Remove the test block (restore normal ACEO/SPSA flow)
2. Debug memory corruption / stale pointer

### 2. Test HSV LUT

Once crash fixed:
```bash
cd LABS && LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune var/pics/DSC00144.ARW preview
```

Should see: `[geos] HSV LUT estimated (36x12 grid)`

### 3. Implement axis contrast loss

1. Add `measureAxisContrast()` to diff.cpp
2. Add `axisContrastLoss()` to spsa.cpp
3. Weight: 0.15-0.20
4. Test on DSC01531 (expect R-C preservation)

### 4. Camera calibration mode

```bash
./bin/tune --calibrate --camera "Sony A7IV" --images var/calibration/*.ARW
# Outputs: etc/vibes/Sony_A7IV.vibe
```

### 5. Strict VIEW/POPS separation

- VIEW phase: Only tone dials, only tone loss
- POPS phase: Only color dials, only color loss
- Freeze dials between phases

---

## What To Stop Doing

1. Per-image polynomial estimation against JPEGs
2. Joint optimization of all 45 dials (causes interference)
3. Expecting <5% error on DRO-heavy images
4. Chasing visual JPEG match in RAWS code

---

## HSV LUT Implementation

| Component | Details |
|-----------|---------|
| Grid | 36 hue × 12 sat bins |
| Per-cell | (ΔH, ΔS, ΔV) deltas |
| Interpolation | Bilinear |
| Position | After 3D LUT, before ToneMapping |

**Files:**
- `LABS/src/main/part/pipe/mods/hsv_lut.cpp` (new)
- `LABS/inc/pipe.hpp` (HsvLut class)
- `LABS/src/main/part/pipe/link.cpp` (HsvLutImpl)
- `LABS/src/main/part/geos/data.cpp` (serialization)
- `LABS/src/main/part/geos/task.cpp` (estimation trigger)

---

## Recovery

Tag `pre-hsv-lut` at commit a0f9e80 (before HSV LUT changes).

## Build

```bash
./wire.sh && make
```

## Test

```bash
cd LABS
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune \
  var/pics/DSC00144.ARW preview \
  --save-area tmp/var/tune \
  --full --optimizer hybrid --fine --logs
```
