# Color Correction

[back](../pipe.md)

**Purpose**: Transforms camera-native RGB to a device-independent color space with proper white balance. This is the foundation for all subsequent edits.

## Sub-Modules

See [subm/color_correction.md](./subm/color_correction.md) for complete specifications.

The Color Correction module contains 2 sub-modules:

### 1. White Balance
**Purpose**: Adjusts color temperature and tint to correct for different lighting conditions.
**Dials**: 2 (temperature, tint)

### 2. Exposure
**Purpose**: Adjusts overall brightness by applying an exposure value (EV) shift.
**Dials**: 1 (exposure)

## Total Dials

**3 dials** across all color correction sub-modules

## Notes

- Camera color matrix is automatic (loaded from EXIF/DNG metadata)
- Chromatic adaptation is automatic (derived from temperature)
