# Global Color

[back](../global_color.md)

**Purpose**: Controls overall color intensity and saturation in a perceptually uniform space.

## Sub-Module: Vibrance

Adjusts saturation with skin tone protection (smart saturation).

### Dials

| Dial       | Range     | Default | Maps To      | Transfer Function              |
|------------|-----------|---------|--------------|--------------------------------|
| `vibrance` | 0.0 - 1.0 | 0.5     | -1.0 to +1.0 | Linear: `v = (dial - 0.5) × 2` |

**Total**: 1 dial

**Behavior**:
- Boosts less-saturated colors more than already-saturated ones
- Protects skin tones (orange hue range 15°-75° with Gaussian falloff)
- Positive values boost saturation, negative values reduce it

## Sub-Module: Saturation

Adjusts global saturation uniformly across all colors.

### Dials

| Dial         | Range     | Default | Maps To   | Transfer Function          |
|--------------|-----------|---------|-----------|----------------------------|
| `saturation` | 0.0 - 1.0 | 0.5     | 0.0 - 2.0 | Linear: `s = dial × 2`     |

**Total**: 1 dial

**Behavior**:
- Uniform chroma multiplier applied to all colors
- 0.5 = neutral (1.0× multiplier)
- 0.0 = complete desaturation, 1.0 = double saturation

## Sub-Module: Color Density

Adjusts the overall color volume/intensity.

### Dials

| Dial            | Range     | Default | Maps To     | Transfer Function          |
|-----------------|-----------|---------|-------------|----------------------------|
| `color_density` | 0.0 - 1.0 | 0.5     | 0.5 - 1.5   | Linear: `d = 0.5 + dial`   |

**Total**: 1 dial

**Behavior**:
- Boosts both chroma and luminance contrast
- Creates richer, more "dense" colors at high values
- Subtle L contrast boost around midpoint (50)

## Total Dials

**3 dials** across all global color sub-modules (1 + 1 + 1)

---

## Implementation

**Algorithm** (processing order):
1. Convert linear RGB to Lab color space
2. Compute chroma C = sqrt(a² + b²) and hue h = atan2(b, a)
3. Apply vibrance (weighted by inverse chroma, skin protection)
4. Apply saturation (uniform chroma multiplier)
5. Apply color density (chroma boost + L contrast)
6. Convert Lab back to linear RGB

**Input**: CV_32FC3 scene-linear sRGB
**Output**: CV_32FC3 adjusted linear RGB

### Dial-to-Value Mappings

| Dial | 0.0 | 0.25 | 0.5 (neutral) | 0.75 | 1.0 |
|------|-----|------|---------------|------|-----|
| vibrance | -1.0 | -0.5 | 0 | +0.5 | +1.0 |
| saturation | 0.0 | 0.5 | 1.0 | 1.5 | 2.0 |
| color_density | 0.5 | 0.75 | 1.0 | 1.25 | 1.5 |

### Notes

- All operations performed in Lab (CIELAB) color space for perceptual uniformity
- Vibrance uses inverse-chroma weighting: low-saturation pixels get larger boost
- Skin tone protection uses Gaussian falloff centered at 45° (orange)
- Color density also applies subtle L contrast for richer appearance
