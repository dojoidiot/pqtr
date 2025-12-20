# LABS Processing Layers

Camera-matching pipeline with 4 layers: DRUM, TUNE, TINT, VIBE.

## Pipeline

```
HEAD (scene-linear RGB from demosaic + WB + color matrix)
  |
  v
DRUM - Local Tone Mapping (CLAHE)
  |     - Per-pixel adjustment based on local luminance
  |     - Driven by DRO/ALO/ADL level from EXIF maker notes
  |     - Params: clip_limit, tile_size
  v
TUNE - Global Tone Curve (1D)
  |     - 256-bin luminance mapping
  |     - From Sony Tone Curve EXIF or learned
  |     - Params: 4 control points or full curve
  v
TINT - Color Grading (3D LUT)
  |     - 17^3 RGB lookup table
  |     - Learned from residual after DRUM+TUNE
  |     - Handles Creative Style (Standard, Vivid, Portrait...)
  v
VIBE - Optimization + User Dials
        - Finds optimal params for DRUM/TUNE/TINT
        - Exposes Lightroom-style sliders
        - Presets and styles
```

## Layer Details

### DRUM (Local)

Based on CLAHE (Contrast Limited Adaptive Histogram Equalization).
Sony's DRO, Canon's ALO, and Nikon's ADL all use variants of this.

```cpp
struct Params {
    int tile_size = 8;        // 8x8 tiles typical
    float clip_limit = 2.0f;  // DRO Lv1~2, Lv5~10
};
```

DRO level mapping (approximate):
- Off: skip CLAHE
- Lv1: clip_limit = 2.0
- Lv2: clip_limit = 3.5
- Lv3: clip_limit = 5.0
- Lv4: clip_limit = 7.0
- Lv5: clip_limit = 10.0
- Auto: analyze histogram, pick level

### TUNE (Global)

1D luminance curve. Sony stores 4 control points in EXIF:
```
Sony Tone Curve: 8000 10400 12900 14100
```

These are output values for inputs at 25%, 50%, 75%, 100% of 14-bit range.
Spline interpolation between points.

### TINT (Color)

17^3 3D LUT for hue/saturation shifts per color region.
Learned from residual after DRUM+TUNE applied.
Requires good coverage (~70%+ cells) to work well.

### VIBE (Control)

Orchestrates all layers. Two modes:

1. **Camera Match**: Parse EXIF, apply DRUM/TUNE/TINT to match embedded JPEG
2. **User Creative**: Map dials to layer params for manual adjustment

Dials:
- Shadows, Highlights, Clarity -> DRUM
- Exposure, Contrast, Whites, Blacks -> TUNE
- Temperature, Tint, Vibrance, Saturation, HSL -> TINT

## Universal Color Science

The algorithms are standard across cameras:
- CLAHE: universal local tone mapping
- Tone curves: spline interpolation
- 3D LUTs: trilinear interpolation
- Color matrices: 3x3 transform

Camera-specific parts:
- DRO level -> clip_limit mapping
- Tone curve control points
- Color matrix values
- Creative style LUT content

## EXIF Fields (Sony)

```
Sony Tone Curve         : 8000 10400 12900 14100
Dynamic Range Optimizer : Auto / Off / Lv1-5
Creative Style          : Standard / Vivid / Portrait / ...
```

## References

- Apical Iridix: Sony's DRO is licensed Iridix technology
- CLAHE: Zuiderveld, Graphics Gems IV (1994)
- OpenCV: cv2.createCLAHE(clipLimit, tileGridSize)
