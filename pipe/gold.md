# Gold Pipeline Architecture

## Overview

The gold pipeline processes Sony ARW files to match DT output. It consists of three phases:

1. **RAW Load** - Extract camera metadata and decode sensor data
2. **Profile Apply** - Lookup camera profile and merge with defaults
3. **Pipe Execute** - Run module chain with colorspace management

---

## Phase 1: RAW Load

### Camera Database (cameras.c)

The camera database mirrors DT's `adobe_coeff.c` and `rawspeed/cameras.xml`:

```
┌─────────────────────────────────────────────────────────┐
│  cameras.c - Camera Knowledge Base                      │
│                                                         │
│  Source: DT adobe_coeff.c + rawspeed cameras.xml        │
│                                                         │
│  CameraData struct:                                     │
│    - make         ("Sony", "Canon", etc.)               │
│    - model        ("ILCE-7M3", "EOS R5", etc.)          │
│    - xyz_to_cam[9]  (XYZ→CAM color matrix)              │
│    - black_level    (default black level)               │
│    - white_level    (default white level)               │
│    - filters        (Bayer pattern)                     │
│                                                         │
│  API:                                                   │
│    cameras_lookup(make, model) → CameraData*            │
│    cameras_compute_d65(xyz_to_cam, d65_coeffs)          │
│                                                         │
│  Supported cameras:                                     │
│    - Sony ILCE-7M3, ILCE-7RM3, ILCE-9, ILCE-7M4, etc.  │
│    - Canon EOS R5, EOS R6                               │
│    - (add more as needed from adobe_coeff.c)            │
└─────────────────────────────────────────────────────────┘
```

### RAW Decode Flow

```
sony.ARW
    │
    ▼
┌─────────────────────────────────────────────────────────┐
│  sony.c - Head Decoder                                  │
│                                                         │
│  1. Lookup camera in database:                          │
│     cam = cameras_lookup("Sony", "ILCE-7M3")            │
│       → xyz_to_cam, black, white, filters               │
│                                                         │
│  2. Compute D65 coefficients:                           │
│     cameras_compute_d65(xyz_to_cam, d65_coeffs)         │
│                                                         │
│  3. Parse RAW file (overrides database defaults):       │
│     - width, height                                     │
│     - strip_offset (compressed data location)           │
│     - sony_curve[4] (linearization curve)               │
│     - black_level, white_level (from embedded)          │
│     - wb_rggb[4] (as-shot WB from EXIF)                 │
│     - filters (Bayer pattern, adjusted for row order)   │
│     - color_matrix[9] (embedded CAM→XYZ if present)     │
│                                                         │
│  4. Decode:                                             │
│     ARW2 compressed data → uint16 Bayer mosaic          │
└─────────────────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────────────────┐
│  PipeState (camera metadata)                            │
│                                                         │
│    width, height, filters                               │
│    black_level, white_level                             │
│    chroma.as_shot[4]    ← wb_rggb                       │
│    chroma.D65coeffs[4]  ← from cameras_compute_d65()    │
│    color_matrix[9]      ← from RAW or cameras.c         │
│    camera_model         ← from EXIF                     │
└─────────────────────────────────────────────────────────┘
```

---

## Phase 2: Profile Apply

```
┌─────────────────────────────────────────────────────────┐
│  Camera Profile Database                                │
│                                                         │
│  profiles/sony_ilce7m3.json:                            │
│  {                                                      │
│    "exposure": { "ev": 1.1 },                           │
│    "channelmixer": {                                    │
│      "adaptation": 1,                                   │
│      "illuminant": [1.004, 0.994, 0.741]                │
│    },                                                   │
│    "filmic": {                                          │
│      "contrast": 1.5,                                   │
│      "black_source": -5.0,                              │
│      "dynamic_range": 8.2                               │
│    },                                                   │
│    "bilat": { "detail": 0.1, "midtone": 0.5 }           │
│  }                                                      │
└─────────────────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────────────────┐
│  Vibe Construction                                      │
│                                                         │
│  Layer 1: DT Module Defaults                            │
│    └─ From pipe/mods/*_defaults()                       │
│                                                         │
│  Layer 2: Camera Profile Override                       │
│    └─ From profiles/{camera_model}.json                 │
│                                                         │
│  Layer 3: XMP/Style Override (optional)                 │
│    └─ From user edits or style files                    │
│                                                         │
│  Result: Final module parameters                        │
└─────────────────────────────────────────────────────────┘
```

---

## Phase 3: Pipe Execute

### Module Chain with Colorspace Tracking

```
Data: uint16 Bayer mosaic (6048 x 4024 x 1)
Colorspace: RAW

    ┌─────────────────────────────────────────────────────┐
    │  rawprepare                                         │
    │    - Subtracts black level                          │
    │    - Scales to [0, 1] using white level             │
    │    - Input: uint16 → Output: float32                │
    │    - Params: black=512, white=16383 (from RAW)      │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 Bayer (6048 x 4024 x 1)
Colorspace: RAW
                              │
    ┌─────────────────────────────────────────────────────┐
    │  temperature                                        │
    │    - Applies WB coefficients per Bayer channel      │
    │    - Params: coeffs from chroma.as_shot (from RAW)  │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 Bayer (6048 x 4024 x 1)
Colorspace: RAW
                              │
    ┌─────────────────────────────────────────────────────┐
    │  highlights                                         │
    │    - Reconstructs clipped highlights                │
    │    - Mode: OPPOSED (2)                              │
    │    - Uses D65coeffs for color recovery              │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 Bayer (6048 x 4024 x 1)
Colorspace: RAW
                              │
    ┌─────────────────────────────────────────────────────┐
    │  demosaic                                           │
    │    - Interpolates Bayer → RGB                       │
    │    - Method: PPG (0)                                │
    │    - Output: 4-channel RGBA                         │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 RGBA (6048 x 4024 x 4)
Colorspace: Camera RGB (linear)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  exposure                                           │
    │    - Applies EV compensation                        │
    │    - Params: ev from camera profile (e.g., 1.1 EV)  │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 RGBA (6048 x 4024 x 4)
Colorspace: Camera RGB (linear)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  colorin                                            │
    │    - Camera RGB → Rec2020 (working space)           │
    │    - Uses color_matrix from cameras.xml             │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 RGBA (6048 x 4024 x 4)
Colorspace: Rec2020 RGB (linear, scene-referred)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  channelmixerrgb                                    │
    │    - Chromatic adaptation (CAT16)                   │
    │    - Params: illuminant from camera profile         │
    │    - DT auto-applies in scene-referred workflow     │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 RGBA (6048 x 4024 x 4)
Colorspace: Rec2020 RGB (linear, scene-referred)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  colorbalancergb                                    │
    │    - Color grading (shadows/mids/highlights)        │
    │    - Params: from camera profile or defaults        │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 RGBA (6048 x 4024 x 4)
Colorspace: Rec2020 RGB (linear, scene-referred)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  filmicrgb                                          │
    │    - Tone mapping (HDR → SDR)                       │
    │    - Params: contrast, black, white from profile    │
    │    - Converts scene-referred → display-referred     │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 RGBA (6048 x 4024 x 4)
Colorspace: Rec2020 RGB (display-referred)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  [RGB → Lab conversion]                             │
    │    - Auto-inserted for bilat                        │
    │    - Rec2020 → XYZ (D65) → Lab (D50)                │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 LabA (6048 x 4024 x 4)
Colorspace: CIE Lab (D50)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  bilat                                              │
    │    - Local contrast (local Laplacian)               │
    │    - Operates on L channel only                     │
    │    - Params: detail, midtone from profile           │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 LabA (6048 x 4024 x 4)
Colorspace: CIE Lab (D50)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  [Lab → RGB conversion]                             │
    │    - Auto-inserted after bilat                      │
    │    - Lab (D50) → XYZ (D65) → Rec2020                │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 RGBA (6048 x 4024 x 4)
Colorspace: Rec2020 RGB (display-referred)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  colorout                                           │
    │    - Rec2020 → sRGB (output space)                  │
    │    - Applies gamma for display                      │
    └─────────────────────────────────────────────────────┘
                              │
Data: float32 RGBA (6048 x 4024 x 4)
Colorspace: sRGB (gamma-encoded)
                              │
    ┌─────────────────────────────────────────────────────┐
    │  PNG Tail                                           │
    │    - Converts float [0,1] → uint8 [0,255]           │
    │    - Writes PNG file                                │
    └─────────────────────────────────────────────────────┘
                              │
                              ▼
                        gold.png
```

---

## Colorspace Summary

| Stage | Colorspace | Notes |
|-------|------------|-------|
| RAW decode | RAW (Bayer) | Single channel mosaic |
| rawprepare → demosaic | RAW (Bayer) | Linear, sensor values |
| demosaic output | Camera RGB | Linear, 4-channel |
| colorin output | Rec2020 RGB | Linear, scene-referred |
| After filmic | Rec2020 RGB | Display-referred |
| bilat | CIE Lab (D50) | Auto-converted |
| colorout output | sRGB | Gamma-encoded |

---

## Parameter Sources

| Module | Camera Metadata | Camera Profile | XMP/Style |
|--------|-----------------|----------------|-----------|
| rawprepare | black, white | - | - |
| temperature | as_shot WB | - | - |
| highlights | D65coeffs, filters | mode | - |
| demosaic | filters | method | - |
| exposure | - | ev | ev |
| colorin | color_matrix | - | - |
| channelmixer | - | illuminant, adaptation, p | illuminant |
| colorbalance | - | contrast, grading | all |
| filmic | - | contrast, black, white, DR | all |
| bilat | - | detail, midtone | detail |
| colorout | - | - | profile |

**Legend:**
- Camera Metadata: From RAW file / cameras.xml (mandatory)
- Camera Profile: From profile database (camera-specific defaults)
- XMP/Style: From user edits or style files (optional override)

---

## Files

```
pipe/
├── src/main/labs/
│   ├── cameras.h           # Camera database API
│   ├── cameras.c           # Camera database (from DT adobe_coeff.c)
│   ├── sony.c              # RAW decode (uses cameras.c for lookup)
│   ├── pipe_state.h        # Camera metadata struct
│   ├── pipe_prepare.c      # D65 coefficient computation
│   └── mods/
│       ├── rawprepare.c    # + rawprepare_defaults()
│       ├── temperature.c   # (no defaults - uses camera WB)
│       ├── highlights.c    # + highlights_defaults()
│       ├── demosaic.c      # + demosaic_defaults()
│       ├── exposure.c      # + exposure_defaults()
│       ├── colorin.c       # + colorin_defaults()
│       ├── channelmixerrgb.c # + channelmixerrgb_reset()
│       ├── colorbalancergb.c # + colorbalancergb_defaults()
│       ├── filmicrgb.c     # + filmicrgb_reset()
│       ├── bilat.c         # + bilat_defaults()
│       ├── colorout.c      # + colorout_defaults()
│       └── colorspace.c    # RGB↔Lab conversion
│
├── copy.md                 # Module copy status
└── gold.md                 # This file

labs/
├── src/main/
│   ├── part/
│   │   ├── vibe.cpp        # Parameter model
│   │   └── pipe.cpp        # Pipe execution
│   ├── plug/
│   │   └── SonyHead.cpp    # Head using sony.c
│   └── step/
│       └── *.cpp           # Step wrappers for each module
│
├── profiles/               # Camera profile database (NEW)
│   └── sony_ilce7m3.json   # Sony ILCE-7M3 profile
│
└── src/test/labs/
    ├── gold.cpp            # Gold pipeline test
    └── phase1.cpp          # Baseline test (no profile)
```

---

## Execution Flow

```cpp
// 1. Create pipe and vibe
auto pipe = pqtr::pipe();
auto vibe = pqtr::vibe(*pipe);

// 2. Load RAW (extracts camera metadata + model)
pipe->head(sonyHead());

// 3. Apply camera profile to vibe (NEW)
//    Looks up profile by camera_model, merges with defaults
applyProfile(*vibe, camera_model);

// 4. Optionally apply XMP overrides
//    vibe->exposure().ev(0.8f);  // from XMP

// 5. Build pipe with vibe parameters
pipe->body("rawprepare", rawprepareStep())
    .body("temperature", temperatureStep())
    .body("highlights", highlightsStep(vibe->highlights()))
    // ... remaining modules
    .tail(pngTail("gold.png"));

// 6. Execute
pipe->pump(arw_filename);
```

---

## Adding New Cameras

To add support for a new camera, add an entry to `camera_db[]` in `cameras.c`:

```c
// In cameras.c - camera_db[] array

{
    .make = "Nikon",
    .model = "Z 8",
    .xyz_to_cam = {
        // Get values from DT's adobe_coeff.c or rawspeed cameras.xml
        // Values are floats (already divided by 10000 if from adobe_coeff)
         0.8142f, -0.2593f, -0.0882f,
        -0.4509f,  1.2573f,  0.2160f,
        -0.0710f,  0.1354f,  0.6292f
    },
    .black_level = 0,      // From cameras.xml or typical for sensor
    .white_level = 16383,  // 14-bit sensor max
    .filters = 0x94949494  // RGGB Bayer pattern
},
```

**Finding color matrix values:**

1. Check DT source `src/external/adobe_coeff.c` for the camera model
2. Or check `rawspeed/data/cameras.xml` for the `<ColorMatrix1>` entry
3. Matrix values in adobe_coeff.c are scaled by 10000 - divide to get floats

**Finding black/white levels:**

1. Check `rawspeed/data/cameras.xml` for `<BlackAreas>` and typical sensor bit depth
2. Common values: 14-bit sensor = white 16383, black varies by camera

---

## DT Expert System

Darktable is an **expert system** with multiple knowledge sources:

```
┌─────────────────────────────────────────────────────────────────────┐
│                     DT Expert System                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  1. RAW METADATA (per-image)                                        │
│     └─ black/white levels, WB coeffs, EXIF                          │
│     └─ Source: Embedded in RAW file                                 │
│                                                                     │
│  2. CAMERA DATABASE (per-camera)                                    │
│     └─ xyz_to_cam matrix, default black/white, Bayer pattern        │
│     └─ Source: cameras.c (from adobe_coeff.c + cameras.xml)         │
│                                                                     │
│  3. MODULE DEFAULTS (universal)                                     │
│     └─ $DEFAULT annotations in each module                          │
│     └─ Source: mods/*_defaults() functions                          │
│                                                                     │
│  4. AUTO-TUNE (per-image, runtime)                         ← NEW    │
│     └─ Histogram analysis to set optimal parameters                 │
│     └─ Source: filmicrgb.c apply_autotune()                         │
│                                                                     │
│  5. CAMERA STYLES (per-camera-family)                               │
│     └─ Pre-tuned looks: exposure, contrast, color                   │
│     └─ Source: styles/darktable_Sony_ILCE-7M3.dtstyle               │
│                                                                     │
│  6. XMP/USER (per-image)                                            │
│     └─ User edits, sidecar files                                    │
│     └─ Source: image.ARW.xmp                                        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Parameter Precedence

```
XMP/User  →  overrides  →  Camera Style
    │                          │
    └──────────┬───────────────┘
               ▼
         Auto-Tune  →  adjusts  →  Module Defaults
               │                        │
               └──────────┬─────────────┘
                          ▼
                   Camera Database
                          │
                          ▼
                    RAW Metadata
```

---

## Parameter Optimization Order

### The Problem

Both DRO (colorbalancergb) and filmic autotune try to lift shadows:
- **DRO**: `shadows[] *= 1.08` in scene-referred space (before filmic)
- **Filmic autotune**: `output_power = f(DR)` lifts shadows via gamma curve

When both are active: **double shadow lift = shadows too bright**

### Solution: Scene-Adaptive Filmic + Metadata DRO

The pipeline uses both adaptive mechanisms:
1. **Filmic autotune**: Analyzes image histogram to set black/white EV points
2. **DRO from metadata**: Applies shadow lift based on camera DRO setting

### Optimization Order (by parameter source)

| Phase | Source | Parameters | Module |
|-------|--------|-----------|--------|
| **1. Fixed from metadata** | Camera calibration | black/white levels | rawprepare |
| | Shot metadata | WB coefficients | temperature |
| | Camera database | color matrix | colorin |
| | ISO from EXIF | noise profile | denoiseprofile |
| **2. Exposure calibration** | Per-camera | exposure EV | exposure |
| **3. Tone mapping** | Scene autotune | black/white EV from histogram | filmicrgb |
| **4. Color grading** | Picture Profile | saturation, vibrance | colorbalancergb |
| | DRO metadata | shadow_lift | colorbalancergb |

### Key Insight

```
Filmic autotune adapts to scene dynamic range
DRO applies camera-specified shadow lift
Both work together for per-image optimization
```

### Implementation

```cpp
// In gold.cpp

// Scene-adaptive filmic: analyze histogram for black/white points
FilmicRGBData filmic_data;
filmicrgb_reset(&filmic_data);
filmicrgb_autotune(&filmic_data, rec2020_cb, width, height);

// DRO handles shadow adaptation (from camera metadata)
if (meta.dro_shadow_lift != 1.0f) {
    cb_data.shadows[0] = meta.dro_shadow_lift;  // e.g., 1.08 for DRO Auto
    cb_data.shadows[1] = meta.dro_shadow_lift;
    cb_data.shadows[2] = meta.dro_shadow_lift;
}
```

---

## Filmic Auto-Tune (Reference)

### Status: ENABLED

Auto-tune is implemented in `filmicrgb_autotune()` and **enabled by default**. It analyzes the scene histogram to adapt the tone curve to each image's dynamic range.

### What It Does

```c
// 1. Compute min/max per channel from input image
// 2. Use max RGB method to find scene black/white
// 3. Convert to EV relative to grey point (0.1845)
// 4. Update black_source, white_source, dynamic_range
// 5. Recompute output_power from DR
// 6. Recompute spline coefficients
```

### To Disable (if needed)

```cpp
// Comment out in gold.cpp:
// filmicrgb_autotune(&filmic_data, rec2020_cb, width, height);

// filmicrgb_reset() provides fixed DT defaults:
// black=-8, white=4, output_power=4.0
```

---

## Verification Tools

### diff.cpp

Compares two images with delta-E tolerance:

```bash
./tmp/build/diff <reference> <output> [--bits|--bayer|--rgb|--lab|--display]

# Modes:
--bits     Exact binary match (for raw data)
--bayer    Float match (mean < 1e-6)
--rgb      Linear RGB delta-E < 0.01
--lab      Lab delta-E < 0.01
--display  sRGB delta-E < 1.0 (perceptual match)
```

### dark.cpp

Parses DT XMP sidecar files:

```bash
./tmp/build/dark src/test/raws/sony.xmp --dump

# Shows all modules and their parameters
```
