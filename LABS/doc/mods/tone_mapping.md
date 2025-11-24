# Tone Mapping

[back](../pipe.md)

**Purpose**: Compresses the high dynamic range (HDR) of the scene into the standard dynamic range (SDR) of the display, while preserving perceptual contrast and detail.

## Sub-Modules

See [subm/tone_mapping.md](./subm/tone_mapping.md) for complete specifications.

The Tone Mapping module contains 3 sub-modules:

### 1. Contrast
**Purpose**: Adjusts the global contrast via S-curve around midpoint.
**Dials**: 1 (contrast: 0.5 → 1.0 neutral)

### 2. Curve Adjustment
**Purpose**: Adjusts specific regions of the tone curve independently.
**Dials**: 2 (highlights, shadows: 0.5 → 0 neutral)

### 3. Clipping Point
**Purpose**: Defines scene black and white points for Reinhard compression.
**Dials**: 2 (white_point: 0.5 → 4.0, black_point: 0.5 → 0.05)

## Total Dials

**5 dials** across all tone mapping sub-modules (1 + 2 + 2)

## Notes

- Uses Extended Reinhard tone compression for soft highlight rolloff
- Processing order: Black point → Reinhard → Shadows → Highlights → Contrast
- White point default (0.5) maps to 4.0 scene luminance
- Black point default (0.5) maps to 0.05 for subtle shadow lift
