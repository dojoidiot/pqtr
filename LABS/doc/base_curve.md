# Base Curve

[back](../README.md)

## Purpose

The base curve bridges the gap between flat RAW decode and camera JPEG appearance. RAWS estimates it automatically per-image from the RAW→preview comparison.

## Problem Solved

Camera JPEGs apply a tone curve (per picture style) BEFORE any adjustments. Without this, our output looks "washed out" compared to the camera preview.

**Before base curve:** Loss = 17.3%
**After base curve:** Loss = 13.1%

## Key Insight: Photographer Intent

The base curve represents **what the photographer saw and intended**.

```
Photographer's Workflow:
1. Sets camera to "Vivid" / "Standard" / etc.
2. Composes shot while looking at LCD preview (WITH base curve)
3. Judges exposure and color based on what they SEE
4. Takes the shot
5. Expects editing software to show similar baseline
```

The camera JPEG preview IS the photographer's reference point. By matching it, we restore their creative intent.

## Architecture

```
RAWS (camera-specific):
  1. Decode RAW → scene-linear data
  2. Extract embedded JPEG → preview
  3. Estimate per-channel curves: gamma-space RGB mapping data→preview
  4. Return {data, preview, baseCurve[768]} (BGR × 256)

pipe::Head:
  - Stores baseCurve from raws::Result
  - Exposes via head->baseCurve(), head->hasBaseCurve()

Link (in BODY):
  - link.baseCurve().setCurve(head->baseCurve())
  - Applied after colorCorrection, before toneMapping
  - Operates in gamma space (linear→gamma, apply LUT, gamma→linear)
```

## Estimation Algorithm

```cpp
// In RAWS
static void estimateBaseCurve(const cv::UMat& data, const cv::UMat& preview, float* curve)
{
    // 1. Resize data to preview size
    // 2. Convert data to gamma-encoded 8-bit
    // 3. For each channel (B, G, R):
    //    - For each input bin [0-255]:
    //      - Accumulate corresponding output from preview
    //    - curve[c*256+i] = average(output for input=i) / 255.0
    // 4. Enforce monotonicity per channel
    // 5. Apply smoothing per channel
}
```

The curve is 768 floats (3 channels × 256) mapping gamma-space input to gamma-space output [0-1].

## Resolution Independence

Testing confirms the embedded preview (1616×1080) is sufficient for accurate curve estimation:

| Source | L2 vs Preview | Note |
|--------|--------------|------|
| Sidecar@preview | < 0.002 | Near-perfect match |
| Sidecar@full | 0.003-0.019 | Often worse |

**Why resolution doesn't matter:**
- Curve estimation averages millions of pixels into 256 bins
- 1.7M pixels (preview) provides ample statistical samples
- Higher resolution adds alignment noise, not signal
- Geometric differences (crop, lens correction) amplify at full-res

The embedded preview and full-res sidecar JPG use **identical camera processing**. The "reduced" preview loses no information for style extraction.

## Application

```cpp
// In base_curve.cpp
bool base_curve(const cv::UMat& input, cv::UMat& output, const float* curve)
{
    // For each pixel, each channel:
    // 1. Clamp to [0, 1]
    // 2. Linear → gamma: v^(1/2.2)
    // 3. Apply LUT with interpolation
    // 4. Gamma → linear: v^2.2
}
```

The gamma space conversion is critical - the curve was estimated in gamma space (8-bit images), so it must be applied in gamma space.

## Files

| File | Purpose |
|------|---------|
| `RAWS/inc/raws.hpp` | Result struct with baseCurve[768] |
| `RAWS/src/main/raws.cpp` | estimateBaseCurve() function |
| `LABS/inc/pipe.hpp` | Head::baseCurve(), Link::BaseCurve |
| `LABS/src/main/part/pipe/pipe.cpp` | HeadImpl stores curve |
| `LABS/src/main/part/pipe/link.cpp` | BaseCurveImpl |
| `LABS/src/main/part/pipe/mods/base_curve.cpp` | Apply function |
| `LABS/src/main/part/pipe/mods/mods.h` | Declarations |
| `LABS/src/main/part/geos/task.cpp` | Baseline guard (line 179) |

## Usage

```cpp
// In tune.cpp or any application
auto head = pipe->open(sink);

if (head->hasBaseCurve())
{
    link.baseCurve().setCurve(head->baseCurve());
}
```

## Advantages of Per-Image Approach

| Per-Camera Files | Per-Image (Implemented) |
|------------------|-------------------------|
| Requires training data | Works immediately |
| Average across images | Exact for this image |
| Needs curve library | No files to distribute |
| Style lookup needed | Derived automatically |

The per-image approach is simpler and more accurate because it uses the actual preview from this specific shot.

## Baseline Guard

When the base curve achieves very low baseline loss (< 5%), the optimizer may make things worse by trading spectral quality for frequency matching (combined loss). A guard in `task.cpp` prevents this:

```cpp
if (result.loss.spectral > baselineLoss.spectral)
{
    writeDials(link, neutralDials);  // Restore neutral
    result.loss = baselineLoss;      // Return baseline
}
```

This ensures the optimizer never degrades quality compared to base curve alone.
