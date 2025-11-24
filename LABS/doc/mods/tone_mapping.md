# Tone Mapping

[back](../pipe.md)

**Purpose**: Compresses the high dynamic range (HDR) of the scene into the standard dynamic range (SDR) of the display, while preserving perceptual contrast and detail.

## Sub-Modules

See [subm/tone_mapping.md](./subm/tone_mapping.md) for complete specifications.

The Tone Mapping module contains 3 sub-modules:

### 1. Contrast
**Purpose**: Adjusts the global contrast of the filmic curve.
**Dials**: 1 (contrast)

### 2. Curve Adjustment (parameterized)
**Purpose**: Adjusts specific regions of the tone curve. Instantiated 2× with constants: HIGHLIGHTS, SHADOWS.
**Dials**: 2 total (1 per region × 2 regions)

### 3. Clipping Point (parameterized)
**Purpose**: Defines black and white clipping points. Instantiated 2× with constants: WHITE, BLACK.
**Dials**: 2 total (1 per endpoint × 2 endpoints)

## Total Dials

**5 dials** across all tone mapping sub-modules

## Notes

- The filmic curve is computed from the 5 parameters
- White point default (0.85) maps to 8.0 scene luminance for well-exposed images
- Black point default (0.15) maps to 0.015 to preserve shadow detail
