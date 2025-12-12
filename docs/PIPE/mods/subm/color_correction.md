# Color Correction

[back](../color_correction.md)

**Purpose**: Transforms camera-native RGB to a device-independent color space with proper white balance.

## Sub-Module: Exposure

Adjusts the overall brightness by applying an exposure value (EV) shift.

### Dials

| Dial        | Range     | Default | Maps To          | Transfer Function                        |
|-------------|-----------|---------|------------------|------------------------------------------|
| `exposure`    | 0.0 - 1.0 | 0.5     | -4 EV to +4 EV   | `EV = (dial - 0.5) * 8`                  |

### Implementation

```
multiplier = 2^EV
output = input × multiplier
```

| Dial | EV | Multiplier | Effect |
|------|-----|------------|--------|
| 0.0 | -4 | 0.0625 | 16× darker |
| 0.25 | -2 | 0.25 | 4× darker |
| 0.5 | 0 | 1.0 | Neutral |
| 0.75 | +2 | 4.0 | 4× brighter |
| 1.0 | +4 | 16.0 | 16× brighter |

**Total**: 1 dial

## Sub-Module: White Balance

Adjusts the color temperature and tint to correct for different lighting conditions.

### Dials

| Dial        | Range     | Default | Maps To          | Transfer Function                        |
|-------------|-----------|---------|------------------|------------------------------------------|
| `temperature` | 0.0 - 1.0 | 0.5     | 2000K - 12000K   | Exponential: `K = 2000 × 6^dial`         |
| `tint`        | 0.0 - 1.0 | 0.5     | Green to Magenta | `shift = (dial - 0.5) × 0.4`             |

### Implementation

Temperature uses exponential mapping for perceptual linearity (equal dial movement = equal perceived change):

| Dial | Kelvin | Description |
|------|--------|-------------|
| 0.0 | 2000K | Candlelight (very warm) |
| 0.25 | ~3130K | Tungsten |
| 0.5 | ~4900K | Daylight (neutral) |
| 0.75 | ~7670K | Overcast |
| 1.0 | 12000K | Shade (very cool) |

Tint adjusts the green/magenta balance:
- dial < 0.5: Green shift (corrects magenta cast)
- dial = 0.5: Neutral
- dial > 0.5: Magenta shift (corrects green cast)

**Total**: 2 dials

## Total Dials

**3 dials** across all color correction sub-modules (1 + 2)

## Processing Order

1. **Exposure** - Applied first (brightness adjustment)
2. **White Balance** - Applied second (color correction)

## Notes

- Camera color matrix is **automatic** (no dial) - applied in HEAD decoder from EXIF/DNG metadata
- Chromatic adaptation is **automatic** (no dial) - derived from temperature
- Default 0.5 values = neutral (no adjustment)
- All processing operates on CV_32FC3 scene-linear RGB
- HDR headroom preserved (values > 1.0 allowed until tone mapping)
