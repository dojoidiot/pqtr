# Color Correction

[back](../color_correction.md)

**Purpose**: Transforms camera-native RGB to a device-independent color space with proper white balance.

## Sub-Module: White Balance

Adjusts the color temperature and tint to correct for different lighting conditions.

### Dials

| Dial        | Range     | Default | Maps To          | Transfer Function                        |
|-------------|-----------|---------|------------------|------------------------------------------|
| `temperature` | 0.0 - 1.0 | 0.5     | 2000K - 10000K   | Linear: `T = 2000 + (value * 8000)`      |
| `tint`        | 0.0 - 1.0 | 0.5     | -100 to +100     | Linear centered: `tint = (value - 0.5) * 200` |

**Total**: 2 dials

## Sub-Module: Exposure

Adjusts the overall brightness by applying an exposure value (EV) shift.

### Dials

| Dial        | Range     | Default | Maps To          | Transfer Function                        |
|-------------|-----------|---------|------------------|------------------------------------------|
| `exposure`    | 0.0 - 1.0 | 0.5     | -4 EV to +4 EV   | Linear centered: `EV = (value - 0.5) * 8`  |

**Total**: 1 dial

## Total Dials

**3 dials** across all color correction sub-modules (2 + 1)

**Notes**:
- Camera color matrix is **automatic** (no dial) - loaded from EXIF/DNG metadata.
- Chromatic adaptation is **automatic** (no dial) - derived from temperature.
- Default 0.5 values = neutral (no adjustment).
