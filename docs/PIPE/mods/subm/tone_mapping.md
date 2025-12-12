# Tone Mapping

[back](../tone_mapping.md)

**Purpose**: Compresses high dynamic range scene data to a display-capable range while preserving perceptual contrast.

## Sub-Module: Contrast

Adjusts the global contrast via S-curve around midpoint (0.5).

### Dials

| Dial       | Range     | Default | Maps To   | Transfer Function                           |
|------------|-----------|---------|-----------|---------------------------------------------|
| `contrast` | 0.0 - 1.0 | 0.5     | 0.5 - 2.0 | Exponential: `c = 0.5 × exp(dial × 1.386)` |

**Total**: 1 dial

## Sub-Module: Curve Adjustment

Adjusts specific regions of the tone curve independently.

### Dials

| Dial         | Range     | Default | Maps To      | Transfer Function                       |
|--------------|-----------|---------|--------------|----------------------------------------|
| `highlights` | 0.0 - 1.0 | 0.5     | -1.0 to +1.0 | Linear: `h = (dial - 0.5) × 2`         |
| `shadows`    | 0.0 - 1.0 | 0.5     | -1.0 to +1.0 | Linear: `s = (dial - 0.5) × 2`         |

**Total**: 2 dials

**Behavior**:
- `highlights`: Adjusts shoulder curve. Positive expands highlights, negative compresses.
- `shadows`: Adjusts toe curve. Positive lifts shadows, negative crushes.

## Sub-Module: Clipping Point

Defines the scene black and white points for Reinhard tone compression.

### Dials

| Dial          | Range     | Default | Maps To    | Transfer Function                      |
|---------------|-----------|---------|------------|----------------------------------------|
| `white_point` | 0.0 - 1.0 | 0.5     | 1.0 - 16.0 | Exponential: `wp = exp(dial × 2.773)` |
| `black_point` | 0.0 - 1.0 | 0.5     | 0.0 - 0.1  | Linear: `bp = dial × 0.1`              |

**Total**: 2 dials

**Behavior**:
- `white_point`: Scene luminance mapped to display white. Default 0.5 → 4.0.
- `black_point`: Luminance below this is lifted. Default 0.5 → 0.05.

## Total Dials

**5 dials** across all tone mapping sub-modules (1 + 2 + 2)

---

## Implementation

**Algorithm** (processing order):
1. **Black point lift**: Subtract black_point, rescale to [0, 1]
2. **Extended Reinhard**: `L_out = L × (1 + L/W²) / (1 + L)` where W = white_point
3. **Shadows curve**: Power adjustment for values < 0.5
4. **Highlights curve**: Inverse power for values > 0.5
5. **Contrast**: S-curve around midpoint (0.5)

**Input**: CV_32FC3 scene-linear sRGB
**Output**: CV_32FC3 tone-mapped linear (before gamma)

### Dial-to-Value Mappings

| Dial | 0.0 | 0.25 | 0.5 (neutral) | 0.75 | 1.0 |
|------|-----|------|---------------|------|-----|
| contrast | 0.5 | 0.71 | 1.0 | 1.41 | 2.0 |
| highlights | -1.0 | -0.5 | 0 | +0.5 | +1.0 |
| shadows | -1.0 | -0.5 | 0 | +0.5 | +1.0 |
| white_point | 1.0 | 2.0 | 4.0 | 8.0 | 16.0 |
| black_point | 0.0 | 0.025 | 0.05 | 0.075 | 0.1 |

### Notes

- Extended Reinhard provides soft highlight rolloff without hard clipping
- Shadows/highlights use soft blending with threshold at 0.5
- Contrast uses signed power curve to preserve monotonicity
- All values clamped to [0, 1] after processing
