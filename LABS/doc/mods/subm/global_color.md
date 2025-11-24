# Global Color

[back](../global_color.md)

**Purpose**: Controls overall color intensity and saturation in a perceptually uniform space.

## Sub-Module: Vibrance

Adjusts saturation with skin tone protection (smart saturation).

### Dials

| Dial            | Range     | Default | Maps To                   | Transfer Function                     |
|-----------------|-----------|---------|---------------------------|---------------------------------------|
| `vibrance`        | 0.0 - 1.0 | 0.5     | -100 to +100              | Linear centered: `v = (value - 0.5) * 200` |

**Total**: 1 dial

**Notes**:
- Protects skin tones (orange hue range 30°-60°)

## Sub-Module: Saturation

Adjusts global saturation uniformly across all colors.

### Dials

| Dial            | Range     | Default | Maps To                   | Transfer Function                     |
|-----------------|-----------|---------|---------------------------|---------------------------------------|
| `saturation`      | 0.0 - 1.0 | 0.5     | -100 to +100              | Linear centered: `s = (value - 0.5) * 200` |

**Total**: 1 dial

## Sub-Module: Color Density

Adjusts the overall color volume/intensity.

### Dials

| Dial            | Range     | Default | Maps To                   | Transfer Function                     |
|-----------------|-----------|---------|---------------------------|---------------------------------------|
| `color_density`   | 0.0 - 1.0 | 0.5     | 0.5 - 1.5 (volume multiplier) | Linear: `d = 0.5 + (value * 1.0)`     |

**Total**: 1 dial

## Total Dials

**3 dials** across all global color sub-modules (1 + 1 + 1)

**Notes**:
- All operations are performed in LCh (CIELAB) color space for perceptual uniformity.
- Default 0.5 values = neutral (no adjustment).
