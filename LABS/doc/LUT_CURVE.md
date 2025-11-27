# LUT Curve Module

## Overview

Per-channel RGB LUT (Look-Up Table) curve estimation and application for matching camera JPEG "vibe" from RAW processing. Reduces optimization loss from 2.48% to 0.19%.

## Architecture

### Files
- `src/main/part/pipe/mods/lut_curve.cpp` - Core module (estimate + apply)
- `src/main/part/pipe/mods/mods.h` - Function declarations
- `src/main/part/pipe/link.cpp` - `LutCurveImpl` class in Link
- `inc/pipe.hpp` - `LutCurve` interface in `Body::Link`

### Data Structure
```cpp
// 3 channels x 32 bins = 96 floats
// Layout: [R0..R31, G0..G31, B0..B31]
// Values: normalized 0-1 (output value for each input bin)
float m_lut[LUT_SIZE * 3];  // LUT_SIZE = 32
```

### Pipeline Position
```
ColorCorrection -> LutCurve -> ToneMapping -> GlobalColor -> SelectiveColour -> Detail
```

## Algorithm

### Estimation (`estimate_lut`)
1. Convert base and target images to 8-bit gamma-encoded BGR
2. For each pixel, bin the base channel value (32 bins)
3. Accumulate target channel value weighted by saturation
4. Compute average target value per bin
5. Smooth LUT to handle sparse bins

**Saturation weighting**: Colorful pixels get 3x weight vs neutrals. This prevents dominant colors (e.g., green foliage) from overwhelming minority colors (e.g., red brick).

### Application (`lut_curve`)
1. Gamma-encode input (linear -> sRGB-like)
2. Build 256-entry interpolated LUTs from 32-point curves
3. Apply LUT per channel (lookup + replace)
4. Gamma-decode output (sRGB-like -> linear)

## Optimization Flow

**Best approach: LUT first, dials second**
```
1. body.view() -> raw linear image (no LUT yet)
2. estimate_lut(raw, target) -> per-channel curves stored in Link
3. LUT applied automatically in link.run() on subsequent body.view() calls
4. GEOS dial optimization (fine-tunes on top of LUT)
5. Edge optimization
```

## Results

| Approach | Final Loss | Notes |
|----------|------------|-------|
| Baseline (no optimization) | 2.48% | |
| Dials only (no LUT) | 0.89% | |
| Luminance-only LUT + dials | 0.83% | Single channel |
| **Per-channel RGB LUT + dials** | **0.19%** | Current best |

## Failed Approaches (Lessons Learned)

### 1. Residual LUT (Dials -> LUT residual)
**Idea**: Optimize dials first, then estimate residual (target - dial_result), apply as additive correction.

**Result**: 4.5-5.5% loss (worse than baseline!)

**Why it failed**:
- Residual estimation in gamma space, application also in gamma space, but small deltas interact badly
- The LUT residual "fights" with dial adjustments already applied
- Color space precision issues with small corrections

### 2. Pre-LUT Optimization (Dials -> LUT -> more dials)
**Idea**: Optimize dials to get close, estimate LUT from that closer state, then fine-tune.

**Result**: 0.31% (worse than LUT-first at 0.19%)

**Why it failed**:
- LUT estimated from dial-adjusted base captures a transform that conflicts with existing dial settings
- When LUT is applied on top of dial-adjusted image, it over-corrects
- Post-LUT loss jumped to 4.8% immediately after LUT application

### 3. Luminance-only LUT
**Idea**: Single LUT for luminance channel only.

**Result**: 0.83% (better than dials-only, worse than per-channel)

**Why it failed**:
- Camera tone curves are often channel-dependent (different R, G, B responses)
- Green channel typically needs different curve than red/blue

## Known Limitations

### Color Interaction
Per-channel LUTs are independent - they don't account for hue-dependent corrections. Example: if image is dominated by green foliage, the green channel curve is well-estimated but red brick wall may still be slightly off.

**Mitigation**: Saturation weighting gives colorful pixels more influence.

**Potential future fix**: Hue-segmented LUTs (separate curves per hue range).

## Usage

```cpp
// In tune Task::run()
if (!link.lutCurve().isEstimated())
{
    View baseView = body.view();
    link.lutCurve().estimate(baseView, targetImage);
}
// LUT automatically applied in subsequent body.view() calls via link.run()
```

## API

```cpp
namespace pipe::mods {
    // Apply per-channel LUT
    bool lut_curve(const cv::UMat& input, cv::UMat& output,
                   const float* lut, int lut_size);

    // Estimate LUT from base->target
    bool estimate_lut(const cv::UMat& base, const cv::UMat& target,
                      float* lut, int lut_size);
}

// In Link interface (pipe.hpp)
class LutCurve {
    virtual const float* lut() const = 0;
    virtual void setLut(const float* values) = 0;
    virtual bool estimate(View base, View target) = 0;
    virtual void reset() = 0;
    virtual bool isEstimated() const = 0;
};
```
