# Global Color

[back](../pipe.md)

**Purpose**: Controls the overall color intensity and saturation in a perceptually uniform space.

## Sub-Modules

See [subm/global_color.md](./subm/global_color.md) for complete specifications.

The Global Color module contains 3 sub-modules:

### 1. Vibrance
**Purpose**: Adjusts saturation with skin tone protection (smart saturation).
**Dials**: 1 (vibrance)

### 2. Saturation
**Purpose**: Adjusts global saturation uniformly across all colors.
**Dials**: 1 (saturation)

### 3. Color Density
**Purpose**: Adjusts the overall color volume/intensity.
**Dials**: 1 (color_density)

## Total Dials

**3 dials** across all global color sub-modules

## Notes

- All operations are performed in LCh (CIELAB) color space for perceptual uniformity
- Vibrance protects skin tones (orange hue range 30°-60°)
