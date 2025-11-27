# Detail + Output Transform

[back](../detail_output.md)

**Purpose**: Finalizes the image for delivery with L-channel sharpening and the correct color space conversion for the target display.

## Sub-Module: Sharpen (L-channel only)

Applies unsharp mask sharpening to the L channel in Lab color space, preserving color accuracy achieved by GEOS optimization.

### Dials

| Dial                | Range     | Default | Maps To                | Transfer Function                                                    |
|---------------------|-----------|---------|------------------------|----------------------------------------------------------------------|
| `sharpen_amount`    | 0.0 - 1.0 | 0.4     | 0.0 - 2.0 (strength)   | Linear: `a = value * 2.0`                                            |
| `sharpen_radius`    | 0.0 - 1.0 | 0.5     | 0.5 - 3.0 (pixels)     | Linear: `r = 0.5 + (value * 2.5)`                                    |

**Total**: 2 dials

**Notes**:
- Operates on L-channel only in Lab color space
- Preserves a/b (color) channels unchanged
- Prevents sharpening from affecting spectral loss

## Sub-Module: Output Transform

Converts to sRGB color space with gamma encoding (automatic, no dials).

**Total**: 0 dials

**Notes**:
- Output color space is **automatic**: sRGB only (camera-to-web scope)
- Gamma encoding is **automatic**: sRGB gamma (2.2) is applied
- Output format is **PNG** (lossless) to preserve quality during tuning and development

## Total Dials

**2 dials** across all detail + output transform sub-modules (2 + 0)
