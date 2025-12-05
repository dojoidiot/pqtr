# LABS Pipeline Recommendations

Deep research synthesis based on darktable, Lightroom, camera ISP pipelines, and color science fundamentals.

---

## Executive Summary

Your architecture is **fundamentally sound**. The 45 dials cover Lightroom/darktable equivalents. The RAWS→FLAT→VIEW→POPS staging is correct. The issue isn't missing capability—it's **optimization target selection**.

**Key Recommendation**: Stop optimizing per-image against embedded JPEGs. Instead, calibrate per-camera once and apply as a portable vibe.

---

## 1. Pipeline Order Validation

### Industry Standard (darktable v3.6+, ACES, Lightroom)

```
RAW Sensor Data
    ↓
Black Level Correction (on Bayer)
    ↓
White Balance Gains (on Bayer)
    ↓
Demosaic (Bayer → RGB)
    ↓
Color Matrix (Camera RGB → Working RGB)
    ↓
[Scene-Linear Space - all edits here]
    ↓
Tone Mapping (Linear → Display Range)
    ↓
Creative Adjustments (Saturation, Split Tone, etc.)
    ↓
Gamma Encoding (Linear → sRGB 2.2)
    ↓
Output
```

### Your Pipeline (RAWS→FLAT→VIEW→POPS)

```
RAWS: Black level → WB → Demosaic → Color Matrix → Scene-Linear ✓
FLAT: Checkpoint (verify clean decode) ✓
VIEW: Exposure, Contrast, Black/White, Toe/Shoulder ✓
POPS: Saturation, Vibrance, Split Tone, Selective Color ✓
Output: Gamma encoding ✓
```

**Verdict**: Your pipeline order matches industry standard. No changes needed.

---

## 2. Dial Coverage Analysis

### Your 45 Dials vs Lightroom

| Category | Your Dials | Lightroom | Status |
|----------|-----------|-----------|--------|
| Exposure | 1 (exposure) | 1 | ✓ Complete |
| White Balance | 2 (temp, tint) | 2 | ✓ Complete |
| Tone | 6 (contrast, highlights, shadows, toe, shoulder, black, white) | 5 | ✓ Complete |
| Presence | 3 (vibrance, saturation, density) | 3 (vibrance, saturation, clarity) | ✓ Equivalent |
| Split Tone | 4 (shadow/highlight temp+tint) | 4 | ✓ Complete |
| HSL | 24 (8 hues × 3) | 24 (8 hues × 3) | ✓ Complete |
| Detail | 4 (sharpen amount/radius, denoise L/C) | 6 | ~80% |

**Missing from full Lightroom parity** (minor, not blocking):
- Tone Curve (explicit control points) - you have implicit via toe/shoulder
- Clarity/Texture (local contrast at different frequencies)
- Dehaze (atmospheric correction)
- Lens corrections (vignette, distortion, CA)

**Verdict**: Your dials are sufficient for style matching. The "magic 6" expands to 45 for full creative control.

---

## 3. The Core Problem: Per-Image vs Per-Camera

### What You're Doing Now

```
For each image:
  1. Decode RAW → flat
  2. Extract embedded JPEG
  3. Estimate polynomial from flat→JPEG
  4. Optimize 45 dials to match JPEG
```

### What Camera Manufacturers Do

```
Once per camera model (factory calibration):
  1. Characterize sensor spectral response
  2. Create color matrices for 2-3 illuminants
  3. Design tone curves for each "style" (Standard, Vivid, Landscape)
  4. Bake into firmware as LUTs

At capture time:
  1. Detect scene type (face, landscape, macro)
  2. Select appropriate LUT
  3. Apply (no per-image optimization)
```

### What Darktable Does

```
Once per camera model (community-sourced):
  1. Create base curve from reference images
  2. Store as camera-specific preset

Per image:
  1. Apply camera preset automatically
  2. User adjusts dials from known baseline
```

### Recommendation

**Shift from per-image optimization to per-camera calibration.**

```
CALIBRATION PHASE (once per camera):
  1. Collect 50-100 diverse images with embedded JPEGs
  2. Extract average dial settings that match camera output
  3. Save as "Sony_A7IV.vibe" or "Canon_R5.vibe"

PROCESSING PHASE (per image):
  1. RAWS → flat (deterministic)
  2. Apply camera vibe (fixed dials from calibration)
  3. Optional: User vibe on top
```

This matches industry practice and eliminates the per-image optimization cost.

---

## 4. Why DRO Creates a 15% Error Floor

### The Spatial Problem

Sony's DRO (Dynamic Range Optimizer) is **spatially-varying**:

```
Standard DRO: Divides image into blocks, analyzes local contrast
DRO+: Per-pixel mapping based on neighborhood (Iridix-style)
```

Your polynomial transform is **per-pixel global**:

```
Out = f(R, G, B)  // Same function everywhere
```

**No per-pixel function can match spatially-varying local tone mapping.**

### Evidence from Your Data

| Image | Polynomial Error | Notes |
|-------|-----------------|-------|
| DSC00202 (urban) | 2.6% | Large uniform areas, DRO minimal |
| DSC00144 (general) | 12.8% | Mixed scene, DRO active |
| DSC01531 (foliage) | 15%+ | High-frequency detail, DRO maximal |

### Recommendation

**Accept the DRO gap. Don't fight it.**

Options:
1. **Ignore DRO**: Accept 10-15% error on DRO-heavy images
2. **Detect DRO images**: Use EXIF DRO setting, apply different strategy
3. **Local tone mapping**: Implement Iridix-style operator (complex, research territory)

For style matching purposes, option 1 is pragmatic. The camera's DRO is scene-dependent magic that would require reverse-engineering Sony's firmware to replicate.

---

## 5. Recommended Pipeline Architecture

### Simplified Three-Stage Model

```
┌─────────────────────────────────────────────────────────┐
│ STAGE 1: DECODE (RAWS)                                  │
│ ─────────────────────────                               │
│ Input: RAW sensor data                                  │
│ Operations:                                             │
│   • Black level subtraction                             │
│   • White balance gains (Bayer domain)                  │
│   • Demosaic (AHD or AMaZE)                             │
│   • Color matrix (camera RGB → linear sRGB)             │
│ Output: Scene-linear RGB                                │
│ Dials: NONE (deterministic)                             │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ STAGE 2: TONE (VIEW)                                    │
│ ───────────────────────                                 │
│ Input: Scene-linear RGB                                 │
│ Operations:                                             │
│   • Exposure (2^EV multiplier)                          │
│   • Filmic/Reinhard tone mapping                        │
│   • Contrast (S-curve around midpoint)                  │
│   • Highlights/Shadows (regional curves)                │
│   • Black/White point adjustment                        │
│ Output: Display-linear RGB [0, 1]                       │
│ Dials: 6 (exposure, contrast, highlights, shadows,      │
│           black, white)                                 │
│ Loss: Tone features (percentiles, std_L, skew_L)        │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ STAGE 3: STYLE (POPS)                                   │
│ ────────────────────────                                │
│ Input: Display-linear RGB                               │
│ Operations:                                             │
│   • Vibrance (protect skin, boost muted)                │
│   • Saturation (uniform chroma multiplier)              │
│   • Split tone (shadow/highlight color cast)            │
│   • Selective color (8 hues × H/S/L)                    │
│   • Detail (sharpen L channel, denoise)                 │
│ Output: Styled RGB                                      │
│ Dials: 39 (everything else)                             │
│ Loss: Color features (mu_C, std_C, split tone)          │
│ Space: Lab for saturation/hue operations                │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ STAGE 4: OUTPUT                                         │
│ ──────────────────                                      │
│ • Apply sRGB gamma (linear → 2.2)                       │
│ • Clamp [0, 255]                                        │
│ • Encode JPEG/PNG                                       │
└─────────────────────────────────────────────────────────┘
```

### Key Insight: Separate Losses

The current HYBRID/STAGED optimizer is on the right track. The key is:

1. **VIEW optimization**: Use `viewLoss()` that heavily weights tone features
2. **POPS optimization**: Use `popsLoss()` that heavily weights color features
3. **Freeze dials**: When optimizing POPS, VIEW dials are frozen (and vice versa)

This prevents the interference where saturation changes affect perceived luminance, causing the optimizer to reduce contrast to compensate.

---

## 6. Validation Test Suite

### Create Synthetic Test Images

To verify dials work correctly, create controlled test inputs:

#### Test 1: Gradient Ramp
```
Input: Linear gradient 0.0 → 1.0 (horizontal)
Verify:
  - Black point: Crushes left side at correct threshold
  - White point: Clips right side at correct threshold
  - Contrast: S-curve pivots around 0.5
  - Toe/Shoulder: Affects correct regions
```

#### Test 2: Color Wheel
```
Input: HSL wheel at constant L=0.5, S=0.8
Verify:
  - Selective color hue shift: Red dial shifts reds only
  - Selective color sat: Affects target hue saturation
  - Vibrance: Boosts low-sat more than high-sat
  - Saturation: Uniform multiplier
```

#### Test 3: Split Tone Patches
```
Input: Left half L=0.2 (shadow), right half L=0.8 (highlight)
Verify:
  - Shadow temp: Warms left half only
  - Highlight temp: Cools right half only
  - Tint: Magenta/green shift in correct regions
```

#### Test 4: Round-Trip
```
Input: Camera JPEG (known good output)
Process: Decode as if RAW, apply neutral dials
Verify: Output matches input (identity transform)
```

### Automated Test Script

```bash
#!/bin/bash
# test_dials.sh - Verify dial behavior

# Generate test images
./bin/labs --generate-test-gradient tmp/gradient.png
./bin/labs --generate-test-wheel tmp/wheel.png
./bin/labs --generate-test-split tmp/split.png

# Test each dial in isolation
for dial in exposure contrast black white toe shoulder; do
  ./bin/labs tmp/gradient.png --dial $dial 0.0 --output tmp/${dial}_min.png
  ./bin/labs tmp/gradient.png --dial $dial 1.0 --output tmp/${dial}_max.png
  # Visual inspection or automated diff
done

# Compare against expected outputs
./bin/compare tmp/exposure_max.png expected/exposure_max.png
```

---

## 7. Darktable Processing Order Reference

For reference, here is darktable's module order (v3.6+ scene-referred):

### Fixed Early Modules (cannot reorder)
1. rawprepare - Initial data prep
2. temperature - White balance
3. highlights - Highlight recovery
4. demosaic - Bayer → RGB
5. colorin - Input color profile (→ linear Rec.2020)

### Scene-Referred Processing
6. exposure - Brightness adjustment
7. colorbalancergb - RGB color balance
8. channelmixerrgb - Channel mixing

### Tone Mapping Boundary
9. **filmic rgb** - Scene-linear → display-referred (THE key module)
   - Alternative: sigmoid (simpler) or agx (newest)

### Display-Referred Creative
10. colorzones - Per-hue adjustments
11. vibrance - Smart saturation
12. splittoning - Shadow/highlight tint
13. sharpen - USM sharpening

### Output
14. colorout - Output color profile
15. gamma - Final encoding

**Key Insight**: darktable's `filmic rgb` is equivalent to your VIEW stage tone mapping. Everything before it is scene-referred (linear), everything after is display-referred (creative).

---

## 8. Concrete Action Items

### Immediate (validation)

1. **Create test swatches** - Gradient, color wheel, split patches
2. **Verify dial isolation** - Each dial affects only its intended region
3. **Document expected behavior** - What should exposure=0.0, 0.5, 1.0 produce?

### Short-term (architecture)

4. **Implement camera calibration mode**
   ```bash
   ./bin/tune --calibrate --camera "Sony A7IV" --images var/calibration/*.ARW
   # Outputs: etc/vibes/Sony_A7IV.vibe
   ```

5. **Apply camera vibe as fixed baseline**
   ```bash
   ./bin/labs photo.ARW --camera-vibe etc/vibes/Sony_A7IV.vibe --output photo.png
   ```

6. **User vibe on top of camera vibe**
   ```bash
   ./bin/labs photo.ARW --camera-vibe Sony_A7IV.vibe --user-vibe my_style.vibe
   ```

### Medium-term (optimization)

7. **Strict stage separation in optimizer**
   - VIEW phase: Only tone dials, only tone loss
   - POPS phase: Only color dials, only color loss
   - No joint phase (or minimal iterations)

8. **Remove polynomial estimation from per-image path**
   - Polynomial is interesting research but adds complexity
   - Camera vibe (fixed dials) is simpler and more portable

### Research (optional)

9. **Local tone mapping investigation**
   - Iridix-style bilateral grid
   - Would close the DRO gap but significant complexity
   - Only pursue if <5% error is required

---

## 9. What To Stop Doing

1. **Stop per-image polynomial estimation** - Move to per-camera calibration
2. **Stop optimizing against embedded JPEGs** - They encode DRO/scene-classification that can't be replicated
3. **Stop joint optimization of all 45 dials** - Causes tone/color interference
4. **Stop expecting <5% error on DRO-heavy images** - It's a spatial problem you can't solve with global transforms

---

## 10. What Your "Lightroom" Should Look Like

A user-facing tool built on LABS:

```
┌─────────────────────────────────────────────────────────┐
│ IMPORT                                                  │
│ ───────                                                 │
│ • Decode RAW (RAWS - automatic, deterministic)          │
│ • Auto-apply camera vibe (Sony_A7IV.vibe)               │
│ • Show preview (matches camera LCD intent)              │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ DEVELOP (user-facing dials)                             │
│ ───────────────────────────                             │
│ BASIC:                                                  │
│   • Exposure [-3, +3] EV                                │
│   • Temperature [2000K, 12000K]                         │
│   • Tint [-100, +100]                                   │
│                                                         │
│ TONE:                                                   │
│   • Contrast [-100, +100]                               │
│   • Highlights [-100, +100]                             │
│   • Shadows [-100, +100]                                │
│   • Whites [-100, +100]                                 │
│   • Blacks [-100, +100]                                 │
│                                                         │
│ PRESENCE:                                               │
│   • Vibrance [-100, +100]                               │
│   • Saturation [-100, +100]                             │
│                                                         │
│ HSL: [8 hue sliders × H/S/L]                            │
│                                                         │
│ SPLIT TONE: [shadow/highlight × temp/tint]              │
│                                                         │
│ DETAIL: [sharpen, denoise]                              │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ EXPORT                                                  │
│ ──────                                                  │
│ • Apply gamma encoding                                  │
│ • Save as JPEG/PNG/TIFF                                 │
│ • Save dial settings as .vibe for reuse                 │
└─────────────────────────────────────────────────────────┘
```

This is complete. The 45 dials you have cover this UI. The pipeline is correct. The only change needed is shifting from per-image optimization to per-camera calibration + user adjustment.

---

## Summary

| Aspect | Status | Recommendation |
|--------|--------|----------------|
| Pipeline order | ✓ Correct | No change |
| Dial coverage | ✓ Complete | No change |
| Scene-referred workflow | ✓ Implemented | No change |
| Stage separation | ~80% | Strict VIEW/POPS loss separation |
| Per-image optimization | ✗ Wrong target | Shift to per-camera calibration |
| DRO matching | ✗ Impossible | Accept gap or implement local TM |
| Validation tests | Missing | Create synthetic test suite |

**Bottom line**: Your dials and pipeline are ready. The strategic shift is from "match this JPEG" to "apply camera style, then user style." This aligns with how darktable, Lightroom, and camera manufacturers actually work.
