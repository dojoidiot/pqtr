# Darktable Modules Reference (IOP v4 Order)

Pipeline order from `dark/lib/desk/src/common/iop_order.c` (v3.0 RAW / iop_order_version=4).

## Pipeline Overview

```
RAW → rawprepare → temperature → highlights → demosaic → exposure → colorin →
    → [scene-referred RGB modules] → colorout → gamma → PNG
```

## Pipeline Stages

| Stage | Modules | Colorspace |
|-------|---------|------------|
| Sensor | rawprepare, temperature, highlights | Bayer mosaic (u16→f32) |
| Camera | demosaic, exposure | Camera RGB (f32) |
| Scene | colorin, channelmixerrgb, colorbalancergb, filmicrgb, bilat | Rec2020 / Lab |
| Display | colorout | sRGB (gamma-encoded) |

---

## Implemented Modules

### 1. rawprepare (order: 1.0, stage: Sensor)
**Description:** Normalizes raw sensor data. Subtracts black level, divides by white point.

**Colorspace:** Bayer u16 → Bayer f32 [0,1]

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| left | int32 | no | 0 | Crop left pixels |
| top | int32 | no | 0 | Crop top pixels |
| right | int32 | no | 0 | Crop right pixels |
| bottom | int32 | no | 0 | Crop bottom pixels |
| raw_black_level_separate | u16[4] | **yes** | sensor | Per-channel black levels (R,G1,B,G2) |
| raw_white_point | u16 | no | sensor | White point |
| flat_field | int | no | 0 | 0=off, 1=embedded gainmap |

---

### 2. temperature (order: 3.0, stage: Sensor)
**Description:** White balance correction. Multiplies each Bayer channel by coefficients.

**Colorspace:** Bayer f32 → Bayer f32

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| red | float | no | 1.0 | Red multiplier |
| green | float | no | 1.0 | Green multiplier |
| blue | float | no | 1.0 | Blue multiplier |
| various | float | no | 1.0 | 4th channel (G2) multiplier |
| preset | int | no | 4 | WB preset (4=camera) |

**Runtime data:** coeffs[4] - per-channel multipliers applied to Bayer pattern.

---

### 3. highlights (order: 4.0, stage: Sensor)
**Description:** Highlight recovery. Reconstructs clipped channels from unclipped ones.

**Colorspace:** Bayer f32 → Bayer f32

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| mode | enum | no | 5 | 0=clip, 1=LCH, 2=inpaint, 5=opposed |
| blendL | float | no | 1.0 | Luminance blend |
| blendC | float | no | 0.0 | Chroma blend |
| strength | float | no | 1.0 | Recovery strength |
| clip | float | no | 1.0 | Clip threshold |
| noise_level | float | no | 0.0 | Noise estimation |
| iterations | int | no | 30 | Inpaint iterations |
| scales | int | no | 6 | Wavelet scales |
| candidating | float | no | 0.4 | Candidate threshold |
| combine | float | no | 2.0 | Combine factor |
| recovery | int | no | 0 | Recovery method |
| solid_color | float | no | 0.0 | Solid color fill |

**Modes:**
- 0 = DT_IOP_HIGHLIGHTS_CLIP (simple clamp)
- 1 = DT_IOP_HIGHLIGHTS_LCH (LCH reconstruction)
- 5 = DT_IOP_HIGHLIGHTS_OPPOSED (guided laplacian)

---

### 4. demosaic (order: 8.0, stage: Sensor→Camera)
**Description:** Bayer demosaicing. Interpolates missing color channels.

**Colorspace:** Bayer f32 → RGB f32 (4 channels)

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| green_eq | int | no | 0 | Green equilibration |
| median_thrs | float | no | 0.0 | Median threshold |
| color_smoothing | int | no | 0 | Color smoothing passes |
| demosaicing_method | int | no | 5 | Algorithm (5=RCD) |
| lmmse_refine | int | no | 1 | LMMSE refinement |
| dual_thrs | float | no | 0.2 | Dual demosaic threshold |
| cs_radius | float | no | 0.0 | Color smoothing radius |
| cs_thrs | float | no | 0.40 | Color smoothing threshold |
| cs_boost | float | no | 0.0 | Color smoothing boost |
| cs_iter | int | no | 8 | Color smoothing iterations |
| cs_center | float | no | 0.0 | Color smoothing center |
| cs_enabled | int | no | 0 | Color smoothing enabled |

**Methods:**
- 0 = PPG
- 1 = AMAZE
- 4 = VNG4
- 5 = RCD (default, recommended)

---

### 5. exposure (order: 21.0, stage: Camera)
**Description:** Exposure adjustment. Linear scaling with black point.

**Colorspace:** RGB f32 → RGB f32

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| mode | int | no | 0 | 0=manual, 1=deflicker |
| black | float | no | 0.0 | Black point |
| exposure | float | no | 0.0 | Exposure (EV) |
| deflicker_percentile | float | no | 50.0 | Deflicker percentile |
| deflicker_target_level | float | no | -4.0 | Deflicker target |
| compensate_exposure_bias | int | no | 0 | Compensate camera bias |

**Formula:** `out = (in - black) / (2^(-exposure) - black)`

---

### 6. colorin (order: 28.0, stage: Camera→Scene)
**Description:** Input color profile. Converts camera RGB to pipeline colorspace (Lab or Rec2020).

**Colorspace:** Camera RGB → Lab (or Pipeline RGB)

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| type | int | no | 6 | Input profile type (6=enhanced matrix) |
| filename | char[512] | no | "" | ICC profile path |
| intent | int | no | 0 | Rendering intent (0=perceptual) |
| normalize | int | no | 0 | Normalize (0=off) |
| blue_mapping | int | no | 0 | Blue channel mapping |
| type_work | int | no | 10 | Working profile (10=Rec2020) |
| filename_work | char[512] | no | "" | Working ICC profile |

**Runtime:** Uses 4x4 color matrix from camera calibration.

---

### 7. colorbalancergb (order: 41.5, stage: Scene)
**Description:** Color grading in Yrg perceptual colorspace. 4-way color wheels.

**Colorspace:** Pipeline RGB → Pipeline RGB

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| global[4] | float | **yes** | 0,0,0,0 | Global color shift (Yrg) |
| shadows[4] | float | **yes** | 0,0,0,0 | Shadows color shift |
| highlights[4] | float | **yes** | 0,0,0,0 | Highlights color shift |
| midtones[4] | float | **yes** | 0,0,0,0 | Midtones color shift |
| midtones_Y | float | no | 0.0 | Midtones luminance |
| chroma_global | float | no | 0.0 | Global chroma |
| chroma[4] | float | **yes** | 0,0,0,0 | Per-range chroma |
| vibrance | float | no | 0.0 | Vibrance |
| contrast | float | no | 0.0 | Contrast |
| saturation_global | float | no | 0.0 | Global saturation |
| saturation[4] | float | **yes** | 0,0,0,0 | Per-range saturation |
| brilliance_global | float | no | 0.0 | Global brilliance |
| brilliance[4] | float | **yes** | 0,0,0,0 | Per-range brilliance |
| hue_angle | float | no | 0.0 | Hue rotation |
| shadows_weight | float | no | 1.0 | Shadows mask weight |
| highlights_weight | float | no | 1.0 | Highlights mask weight |
| midtones_weight | float | no | 1.0 | Midtones mask weight |
| mask_grey_fulcrum | float | no | 0.1845 | Grey fulcrum for masks |
| white_fulcrum | float | no | 1.0 | White point |
| grey_fulcrum | float | no | 0.1845 | Grey point |
| saturation_formula | int | no | 0 | Saturation algorithm |

---

### 8. filmicrgb (order: 46.0, stage: Scene)
**Description:** Scene-referred tone mapping with filmic S-curve.

**Colorspace:** Pipeline RGB → Pipeline RGB

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| grey_source | float | no | -4.47 | Log2 middle grey |
| black_source | float | no | -8.0 | Log2 black point |
| white_source | float | no | 4.0 | Log2 white point |
| dynamic_range | float | no | 12.0 | Total dynamic range (EV) |
| normalize | float | no | 1.0 | Normalization factor |
| output_power | float | no | 2.4 | Output gamma |
| contrast | float | no | 1.0 | Contrast |
| saturation | float | no | 0.0 | Color saturation |
| sigma_toe | float | no | 0.5 | Toe smoothness |
| sigma_shoulder | float | no | 0.5 | Shoulder smoothness |
| spline | struct | no | - | Curve control points |

**Mutual exclusion:** Only one tone mapper should be enabled: filmicrgb OR sigmoid OR basecurve.

---

### 9. bilat (order: 54.0, stage: Scene)
**Description:** Local contrast enhancement using local Laplacian pyramids.

**Colorspace:** Lab → Lab (operates on L channel only)

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| mode | int | no | 1 | 0=bilateral, 1=local laplacian |
| sigma_r | float | no | 0.5 | Highlights (range sigma) |
| sigma_s | float | no | 0.5 | Shadows (spatial sigma) |
| detail | float | no | 0.25 | Clarity/detail boost |
| midtone | float | no | 0.5 | Midtone protection |

**Note:** Only modifies L channel; a,b channels pass through unchanged.

---

### 10. colorout (order: 70.0, stage: Scene→Display)
**Description:** Output color profile. Converts Lab to output colorspace (sRGB).

**Colorspace:** Lab → sRGB (gamma-encoded)

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| type | int | no | 1 | Output profile (1=sRGB) |
| filename | char[512] | no | "" | ICC profile path |
| intent | int | no | 0 | Rendering intent (0=perceptual) |

**Process:**
1. Lab → XYZ (D50 white point)
2. XYZ → linear RGB (3x3 matrix)
3. linear RGB → sRGB (gamma curve)

---

## Mutual Exclusions

| Group | Modules | Notes |
|-------|---------|-------|
| Tone mapping | filmicrgb, sigmoid, basecurve, filmic | Enable only one |
| Color adaptation | channelmixerrgb, colorchecker | Usually one primary |
| Denoising | nlmeans, denoiseprofile | Different algorithms |

---

### 11. channelmixerrgb (order: 28.5, stage: Scene)
**Description:** Chromatic adaptation and color calibration. Converts camera white balance to D50.

**Colorspace:** Pipeline RGB → Pipeline RGB

| Parameter | Type | Multi-ch | Default | Description |
|-----------|------|----------|---------|-------------|
| adaptation | enum | no | 1 | CAT16, LINEAR_BRADFORD, FULL_BRADFORD, XYZ, RGB |
| illuminant | float[4] | **yes** | D65 | Source illuminant in LMS space |
| MIX | float[4][4] | **yes** | identity | Channel mixing matrix |
| saturation | float[4] | **yes** | 0 | Per-channel saturation |
| lightness | float[4] | **yes** | 0 | Per-channel lightness |
| p | float | no | 1.0 | Power for full Bradford |
| gamut | float | no | 1.0 | Gamut compression |
| clip | int | no | 1 | Clip negative values |
| version | enum | no | V3 | Saturation algorithm version |

**Output:** `src/main/labs/mods/channelmixerrgb.c` - 0 mismatches (73,011,456 RGB values).

---

## Not Yet Implemented

| Module | Order | Stage | Purpose |
|--------|-------|-------|---------|
| sigmoid | 45.3 | Scene | Alternative tone mapper (simpler than filmic) |
| flip | 16.0 | Camera | Image orientation (geometric) |
| gamma | 78.0 | Display | Final display gamma (no-op for export) |

---

## Parameter Encoding in XMP

Parameters are stored as hex-encoded binary blobs in `darktable:params`. Example:
```
darktable:params="010000000000003f0000003fcccccc3d0000003f"
```
This is little-endian binary: int32 mode=1, float sigma_r=0.5, float sigma_s=0.5, float detail=0.1, float midtone=0.5.

Some modules use gzip compression (params starting with "gz").
