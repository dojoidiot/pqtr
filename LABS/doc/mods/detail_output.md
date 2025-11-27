# Detail + Output Transform

[back](../pipe.md)

**Purpose**: Finalizes the image for delivery with L-channel sharpening and the correct color space conversion for sRGB output.

## Sub-Modules

See [subm/detail_output.md](./subm/detail_output.md) for complete specifications.

The Detail + Output Transform module contains 2 sub-modules:

### 1. Sharpen (L-channel only)
**Purpose**: Applies unsharp mask sharpening to the L channel in Lab color space, preserving color accuracy.
**Dials**: 2 (sharpen_amount, sharpen_radius)

### 2. Output Transform
**Purpose**: Converts to sRGB color space with gamma encoding (automatic, no dials).
**Dials**: 0

## Total Dials

**2 dials** across all detail + output transform sub-modules

## Notes

- Sharpening operates on L-channel only to preserve color accuracy achieved by GEOS optimization
- Output color space is automatic: sRGB only (camera-to-web scope)
- Gamma encoding is automatic: sRGB gamma (2.2) is applied
- Output format is PNG (lossless) to preserve quality during tuning and development
