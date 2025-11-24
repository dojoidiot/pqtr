# Detail + Output Transform

[back](../detail_output.md)

**Purpose**: Finalizes the image for delivery with sharpening, noise reduction, and the correct color space conversion for the target display.

## Sub-Module: Sharpen

Applies unsharp mask sharpening to enhance detail.

### Dials

| Dial                | Range     | Default | Maps To                | Transfer Function                                                    |
|---------------------|-----------|---------|------------------------|----------------------------------------------------------------------|
| `sharpen_amount`    | 0.0 - 1.0 | 0.6     | 0.0 - 2.0 (strength)   | Linear: `a = value * 2.0`                                            |
| `sharpen_radius`    | 0.0 - 1.0 | 0.4     | 0.5 - 3.0 (pixels)     | Linear: `r = 0.5 + (value * 2.5)`                                    |

**Total**: 2 dials

**Notes**:
- Default amount 0.6 = moderate sharpening for web output

## Sub-Module: Denoise

This is a parameterized sub-module that reduces noise in specific channels. The same function is instantiated 2 times with different channel constants.

### Parameters

**Channel Constant** (compile-time parameter):
- `LUMINANCE` (brightness noise, default 0.3)
- `CHROMA` (color noise, default 0.5)

### Dials (per channel instance)

| Dial                | Range     | Default | Maps To                | Transfer Function                                                    |
|---------------------|-----------|---------|------------------------|----------------------------------------------------------------------|
| `strength`          | 0.0 - 1.0 | varies  | 0.0 - 100.0 (strength) | Exponential: `d = 100 * (value^2)`                                   |

**Total**: 1 dial per channel × 2 channels = **2 dials**

**Notes**:
- Defaults are conservative. Increase for high-ISO images.

## Sub-Module: Output Transform

Converts to sRGB color space with gamma encoding (automatic, no dials).

**Total**: 0 dials

**Notes**:
- Output color space is **automatic**: sRGB only (camera-to-web scope)
- Gamma encoding is **automatic**: sRGB gamma (2.2) is applied
- Output format is **PNG** (lossless) to preserve quality during tuning and development

## Total Dials

**4 dials** across all detail + output transform sub-modules (2 + 2 + 0)
