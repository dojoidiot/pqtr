# Detail + Output Transform

[back](../pipe.md)

**Purpose**: Finalizes the image for delivery with sharpening, noise reduction, and the correct color space conversion for sRGB output.

## Sub-Modules

See [subm/detail_output.md](./subm/detail_output.md) for complete specifications.

The Detail + Output Transform module contains 3 sub-modules:

### 1. Sharpen
**Purpose**: Applies unsharp mask sharpening to enhance detail.
**Dials**: 2 (sharpen_amount, sharpen_radius)

### 2. Denoise (parameterized)
**Purpose**: Reduces noise in specific channels. Instantiated 2× with constants: LUMINANCE, CHROMA.
**Dials**: 2 total (1 per channel × 2 channels)

### 3. Output Transform
**Purpose**: Converts to sRGB color space with gamma encoding (automatic, no dials).
**Dials**: 0

## Total Dials

**4 dials** across all detail + output transform sub-modules

## Notes

- Output color space is automatic: sRGB only (camera-to-web scope)
- Gamma encoding is automatic: sRGB gamma (2.2) is applied
- Output format is PNG (lossless) to preserve quality during tuning and development
