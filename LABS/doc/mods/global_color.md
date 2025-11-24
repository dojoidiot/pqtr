# Global Color

[back](../pipe.md)

**Purpose**: Controls the overall color intensity and saturation in a perceptually uniform space.

## Sub-Modules

See [subm/global_color.md](./subm/global_color.md) for complete specifications.

The Global Color module contains 3 sub-modules:

### 1. Vibrance
**Purpose**: Smart saturation boost weighted by inverse chroma, with skin tone protection.
**Dials**: 1 (vibrance: 0.5 → 0 neutral)

### 2. Saturation
**Purpose**: Uniform saturation multiplier across all colors.
**Dials**: 1 (saturation: 0.5 → 1.0× neutral)

### 3. Color Density
**Purpose**: Overall color volume/intensity boost with L contrast.
**Dials**: 1 (color_density: 0.5 → 1.0× neutral)

## Total Dials

**3 dials** across all global color sub-modules (1 + 1 + 1)

## Notes

- All operations performed in Lab (CIELAB) color space for perceptual uniformity
- Vibrance protects skin tones (orange hue range 15°-75° with Gaussian falloff)
- Processing order: Vibrance → Saturation → Color Density
