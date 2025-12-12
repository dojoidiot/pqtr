# Selective Color (Hue Mixer)

[back](../selective_color.md)

**Purpose**: Provides selective color manipulation for brand identity and creative look development.

## Sub-Module: HSL Adjustment

Adjusts Hue, Saturation, and Luminance for 8 color bands with smooth blending.

### Dials (per color band)

| Dial         | Range     | Default | Maps To        | Transfer Function                |
|--------------|-----------|---------|----------------|----------------------------------|
| `hue`        | 0.0 - 1.0 | 0.5     | -30° to +30°   | Linear: `h = (dial - 0.5) × 60`  |
| `saturation` | 0.0 - 1.0 | 0.5     | -1.0 to +1.0   | Linear: `s = (dial - 0.5) × 2`   |
| `luminance`  | 0.0 - 1.0 | 0.5     | -1.0 to +1.0   | Linear: `l = (dial - 0.5) × 2`   |

**Total**: 3 dials per color × 8 colors = **24 dials**

### Color Bands and Hue Centers

| Index | Band     | Center Hue | Effective Range |
|-------|----------|------------|-----------------|
| 0     | Red      | 0° (360°)  | 315° - 45°      |
| 1     | Orange   | 45°        | 0° - 90°        |
| 2     | Yellow   | 90°        | 45° - 135°      |
| 3     | Green    | 150°       | 105° - 195°     |
| 4     | Cyan     | 195°       | 150° - 240°     |
| 5     | Blue     | 240°       | 195° - 285°     |
| 6     | Purple   | 285°       | 240° - 330°     |
| 7     | Magenta  | 315°       | 270° - 360°     |

## Total Dials

**24 dials** (8 colors × 3 dials each)

---

## Implementation

**Algorithm** (processing order):
1. Convert linear RGB to HLS (via gamma-encoded 8-bit)
2. For each pixel, compute hue weight for each of 8 color bands
3. Accumulate weighted HSL adjustments from all matching bands
4. Apply hue shift, saturation adjust, luminance adjust
5. Convert HLS back to linear RGB

**Input**: CV_32FC3 scene-linear sRGB
**Output**: CV_32FC3 adjusted linear RGB

### Hue Weight Function

Uses cosine falloff for smooth blending between adjacent bands:

```
weight(pixel_hue, band_center) =
    if |pixel_hue - band_center| > 45°: 0
    else: 0.5 × (1 + cos(π × diff / 45°))
```

### Adjustment Application

- **Hue**: Direct shift (wrapped to 0-360°)
- **Saturation**: Multiplicative boost (toward 1), additive reduction (toward 0)
- **Luminance**: Similar asymmetric boost/reduce for natural response

### Dial-to-Value Mappings

| Dial | 0.0 | 0.25 | 0.5 (neutral) | 0.75 | 1.0 |
|------|-----|------|---------------|------|-----|
| hue | -30° | -15° | 0° | +15° | +30° |
| saturation | -1.0 | -0.5 | 0 | +0.5 | +1.0 |
| luminance | -1.0 | -0.5 | 0 | +0.5 | +1.0 |

### Notes

- Overlapping hue ranges ensure smooth transitions (no hard edges)
- Weights are normalized for overlapping regions
- Processing in HLS space ensures perceptual consistency
- All neutral dials (0.5) result in no change (early exit optimization)
