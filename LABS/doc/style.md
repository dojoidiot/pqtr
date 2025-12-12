# Style Architecture

[back](../README.md)

## Core Concept: Style + Tweaks

The LABS pipeline separates image processing into two layers:

1. **Style** - Deterministic transforms learned from a reference image
2. **Tweaks** - Per-image adjustments via 45 dials

This separation enables:
- Automatic matching of camera picture styles
- Learning photographer editing styles from examples
- Applying consistent looks across image sets

## The Insight

When optimizing dials to match camera JPEGs (same camera, same style setting), **all dials converge to neutral (0.5)**:

```
Dial Analysis (9 images, Sony A7III Standard):
- Mean deviation from 0.5: < 0.02 for all dials
- Standard deviation: 0.01-0.03
- No significant pattern
```

This means the **style transforms alone** (color matrix + base curve) capture the camera's look. Dials are unnecessary when the style is already extracted from the reference.

## Style Transforms

### 1. Color Matrix (3x3)

Captures cross-channel color transformations:

```
[R']   [a b c]   [R]
[G'] = [d e f] × [G]
[B']   [g h i]   [B]
```

What it captures:
- Hue rotation (shift greens toward yellow)
- Color-specific saturation
- Cross-channel color grading
- White balance (when diagonal)

**Why needed**: Per-channel curves can only scale channels independently. A matrix can mix channels, enabling hue shifts that curves cannot.

### 2. Base Curve (1D × 3 channels)

Captures per-channel tone response:

```
R_out = curve_R[R_in]
G_out = curve_G[G_in]
B_out = curve_B[B_in]
```

What it captures:
- Contrast expansion
- Highlight/shadow rolloff
- Per-channel brightness
- Color balance at different luminance levels

## Estimation Process

Both transforms are estimated in GEAR by comparing RAW decode to reference:

```cpp
// In GEAR
Result decode(Sink& sink) {
    // 1. Decode RAW
    cv::UMat data = decodeRaw(sink);

    // 2. Extract reference (embedded JPEG or provided)
    cv::UMat reference = extractPreview(sink);

    // 3. Estimate color matrix (cross-channel)
    //    Solve: minimize ||M × data - reference||²
    estimateColorMatrix(data, reference, result.colorMatrix);

    // 4. Apply matrix, then estimate residual curve
    cv::UMat afterMatrix = applyMatrix(data, result.colorMatrix);
    estimateBaseCurve(afterMatrix, reference, result.baseCurve);

    return result;
}
```

## Pipeline Order

```
RAW → Demosaic → WB → [Color Matrix] → [Base Curve] → Dials → Output
                            ↑               ↑
                      Style transforms (from reference)
```

The matrix applies first (in linear space), then the curve (in gamma space). This matches how camera pipelines typically work.

## Use Cases

### Camera JPEG Matching

When the reference is an embedded camera JPEG:

- **Style** = Camera's picture style (Standard, Vivid, Portrait, etc.)
- **Dials** ≈ 0.5 (neutral) - style does the work
- **Result** = Near-perfect match to camera output

The EXIF tells us which style was used (`Creative Style: Standard`), enabling automatic lookup or verification.

### Photographer Style Matching

When the reference is a photographer's edited image:

- **Style** = Base transforms learned from their edit
- **Dials** = Creative choices they made (exposure, color grading)
- **Result** = Can apply their style to new images

Example workflow:
1. Photographer edits DSC00001.ARW → DSC00001_edit.jpg
2. System learns style from RAW→edit comparison
3. Apply same style to DSC00002.ARW, DSC00003.ARW, etc.
4. Dials capture the consistent adjustments

### Style Presets

Because style transforms are deterministic for a camera+setting combination, they can be saved as presets:

```json
{
  "name": "Sony A7III Standard",
  "camera": "ILCE-7M3",
  "style": "Standard",
  "colorMatrix": [1.02, -0.01, -0.01, ...],
  "baseCurve": [0.0, 0.004, 0.008, ...]
}
```

New images from the same camera+style can use the preset instead of re-estimating.

## Why This Matters

Traditional RAW processors require manual adjustment for every image. This architecture enables:

1. **Zero-click matching** - Style transforms automatically match camera output
2. **Style transfer** - Learn and apply photographer styles
3. **Batch consistency** - Same style across hundreds of images
4. **Predictable tweaks** - Dials only handle deviations from the style

The key insight: **Separate what's deterministic (style) from what's creative (tweaks)**.
