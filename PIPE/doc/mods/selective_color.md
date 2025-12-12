# Selective Color

[back](../pipe.md)

**Purpose**: Allows for targeted adjustments to specific color ranges, enabling creative looks and brand consistency.

## Sub-Modules

See [subm/selective_color.md](./subm/selective_color.md) for complete specifications.

The Selective Color module contains 1 sub-module:

### 1. HSL Adjustment (×8 color bands)
**Purpose**: Provides independent HSL adjustments for 8 color bands.
**Bands**: Red, Orange, Yellow, Green, Cyan, Blue, Purple, Magenta
**Dials**: 24 (8 colors × 3 dials: hue, saturation, luminance)

## Total Dials

**24 dials** across all selective color sub-modules

## Notes

- 45° effective range per band with cosine falloff for smooth blending
- Overlapping hue ranges ensure no hard transitions between bands
- Processing in HLS space for perceptual consistency
- All neutral dials (0.5) result in no change
