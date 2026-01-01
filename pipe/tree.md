# Gold Pipeline Parameter Tree

Pipeline: `ARW -> rawprepare -> temperature -> highlights -> demosaic -> exposure -> colorin -> [channelmixerrgb] -> [filmicrgb] -> [colorbalancergb] -> colorout -> PNG`

Modules in `[brackets]` are not yet used in gold.cpp.

---

## PipeState (shared)

```
PipeState
├── width              int
├── height             int
├── filters            uint32_t     0x94949494 (RGGB)
└── chroma
    ├── late_correction  int
    ├── D65coeffs[4]     double
    └── as_shot[4]       double
```

---

## [1] rawprepare

```
RawprepareParams
├── left               int32_t      crop pixels
├── top                int32_t      crop pixels
├── right              int32_t      crop pixels
├── bottom             int32_t      crop pixels
├── raw_black_level_separate[4]
│   ├── [0]            uint16_t     R black (512)
│   ├── [1]            uint16_t     G1 black
│   ├── [2]            uint16_t     G2 black
│   └── [3]            uint16_t     B black
├── raw_white_point    uint16_t     (16383)
└── flat_field         int          0=off, 1=gainmap

RawprepareData (computed)
├── left/top/right/bottom  int32_t
├── sub[4]             float        black / 65535.0
│   ├── [0]            R
│   ├── [1]            G1
│   ├── [2]            G2
│   └── [3]            B
└── div[4]             float        white - black
    ├── [0]            R
    ├── [1]            G1
    ├── [2]            G2
    └── [3]            B
```

---

## [2] temperature

```
TemperatureParams
├── red                float        WB multiplier
├── green              float        WB multiplier (reference)
├── blue               float        WB multiplier
├── various            float        G2 multiplier
└── preset             int          4=camera

TemperatureData
├── coeffs[4]          float
│   ├── [0]            R coefficient (~2.51)
│   ├── [1]            G1 coefficient (1.0)
│   ├── [2]            B coefficient (~1.46)
│   └── [3]            G2 coefficient (1.0)
└── preset             int
```

---

## [3] highlights

```
HighlightsData
├── mode               HighlightsMode
│   ├── DT_IOP_HIGHLIGHTS_CLIP     = 0
│   ├── DT_IOP_HIGHLIGHTS_LCH      = 1
│   ├── DT_IOP_HIGHLIGHTS_INPAINT  = 2
│   └── DT_IOP_HIGHLIGHTS_OPPOSED  = 3  (default)
├── blendL             float        1.0  luminance blend
├── blendC             float        0.0  chroma blend
├── strength           float        1.0
├── clip               float        1.0
├── noise_level        float        0.0
├── iterations         int          30
├── scales             int          6
├── candidating        float        0.4
├── combine            float        2.0
├── recovery           int          0
└── solid_color        float        0.0
```

---

## [4] demosaic

```
DemosaicParams
├── green_eq           int          0=disabled
├── median_thrs        float        0.0
├── color_smoothing    int          0=disabled
├── demosaicing_method int
│   ├── 0 = PPG
│   ├── 1 = AMAZE
│   ├── 2 = VNG4
│   ├── 3 = passthrough monochrome
│   ├── 4 = passthrough color
│   ├── 5 = RCD (default)
│   ├── 6 = LMMSE
│   ├── 7 = RCD+VNG4
│   └── 8 = AMAZE+VNG4
├── lmmse_refine       int          1
├── dual_thrs          float        0.2
├── cs_radius          float        0.0  (not used)
├── cs_thrs            float        0.40
├── cs_boost           float        0.0  (not used)
├── cs_iter            int          8
├── cs_center          float        0.0  (not used)
└── cs_enabled         int          FALSE (not used)
```

---

## [5] exposure

```
ExposureParams
├── mode               int          0=manual
├── black              float        0.0
├── exposure           float        0.7 EV (scene-dependent)
├── deflicker_percentile     float  50.0
├── deflicker_target_level   float  -4.0
└── compensate_exposure_bias int    0
```

---

## [6] colorin

```
ColorinParams
├── type               int          6=DT_COLORSPACE_ENHANCED_MATRIX
├── filename[512]      char         ICC profile path
├── intent             int          0=DT_INTENT_PERCEPTUAL
├── normalize          int          0=DT_NORMALIZE_OFF
├── blue_mapping       int          FALSE
├── type_work          int          10=DT_COLORSPACE_LIN_REC2020
└── filename_work[512] char         work profile ICC

cmatrix[4][4]          float        Camera RGB -> XYZ (from ICC/DNG)
│   cam_to_xyz[3][3] = {           Sony A7 III example
│       { 0.6389, 0.1092, 0.1820 },  // X row
│       { 0.2454, 0.7867, -0.0321 }, // Y row
│       { 0.0132, -0.1291, 0.9523 }  // Z row
│   }

XYZ_to_REC2020[4][4]   float        (module constant)
│   ├── [0] = { 1.6473, -0.3936, -0.2360, 0 }
│   ├── [1] = { -0.6826, 1.6476, 0.0128, 0 }
│   ├── [2] = { 0.0297, -0.0629, 1.2539, 0 }
│   └── [3] = { 0, 0, 0, 0 }

REC2020_to_XYZ[4][4]   float        (module constant)
│   ├── [0] = { 0.6735, 0.1657, 0.1250, 0 }
│   ├── [1] = { 0.2790, 0.6753, 0.0456, 0 }
│   ├── [2] = { -0.0019, 0.0300, 0.7969, 0 }
│   └── [3] = { 0, 0, 0, 0 }
```

---

## [7] channelmixerrgb

```
ChannelMixerRGBData
├── adaptation         dt_adaptation_t
│   ├── DT_ADAPTATION_LINEAR_BRADFORD = 0
│   ├── DT_ADAPTATION_CAT16           = 1
│   ├── DT_ADAPTATION_FULL_BRADFORD   = 2
│   ├── DT_ADAPTATION_XYZ             = 3
│   └── DT_ADAPTATION_RGB             = 4
├── illuminant[4]      float        chromatic adaptation source
│   ├── [0]            X
│   ├── [1]            Y
│   ├── [2]            Z
│   └── [3]            unused
├── MIX[4][4]          float        channel mixing matrix
│   ├── [0][0-2]       R output from R,G,B
│   ├── [1][0-2]       G output from R,G,B
│   └── [2][0-2]       B output from R,G,B
├── saturation[4]      float        per-channel saturation
│   ├── [0]            R
│   ├── [1]            G
│   └── [2]            B
├── lightness[4]       float        per-channel lightness
│   ├── [0]            R
│   ├── [1]            G
│   └── [2]            B
├── grey[4]            float        grey point coefficients
│   ├── [0]            R
│   ├── [1]            G
│   └── [2]            B
├── p                  float        norm power
├── gamut              float        gamut compression
├── clip               int          clip negatives
├── apply_grey         int          apply grey normalization
└── version            int          algorithm version (3)
```

---

## [8] filmicrgb

```
FilmicRGBData
├── grey_source        float        0.1845 (18.45% grey)
├── black_source       float        -5.0 EV (relative to grey)
├── white_source       float        0.0 EV (relative to grey)
├── dynamic_range      float        8.2 EV (white - black)
├── normalize          float        9.4369 (for output power)
├── output_power       float        3.4165 (display gamma)
├── contrast           float        1.5
├── saturation         float        0.0 (v5 uses 0)
├── sigma_toe          float        0.0413
├── sigma_shoulder     float        0.0169
└── spline             dt_iop_filmic_rgb_spline_t
    ├── M1[4]          float        spline coefficients (toe)
    ├── M2[4]          float        spline coefficients
    ├── M3[4]          float        spline coefficients
    ├── M4[4]          float        spline coefficients
    ├── M5[4]          float        spline coefficients (shoulder)
    ├── latitude_min   float        0.6097 (toe/linear boundary)
    ├── latitude_max   float        0.6098 (linear/shoulder boundary)
    ├── x[5]           float        spline x coordinates
    ├── y[5]           float        spline y coordinates
    └── type[2]        dt_iop_filmicrgb_curve_type_t
        ├── [0]        toe type (POLY_4)
        └── [1]        shoulder type (POLY_4)

FILMIC_INPUT_MATRIX_TRANS[4][4]     Rec2020 -> LMS
FILMIC_OUTPUT_MATRIX[4][4]          LMS -> Rec2020
FILMIC_OUTPUT_MATRIX_TRANS[4][4]    (transposed)
FILMIC_EXPORT_INPUT_MATRIX_TRANS    sRGB -> LMS
FILMIC_EXPORT_OUTPUT_MATRIX         LMS -> sRGB
FILMIC_EXPORT_OUTPUT_MATRIX_TRANS   (transposed)
```

---

## [9] colorbalancergb

```
ColorBalanceRGBData
├── global[4]          float        GLOBAL color wheel (offset/lift)
│   ├── [0]            R shift
│   ├── [1]            G shift
│   ├── [2]            B shift
│   └── [3]            unused
├── shadows[4]         float        SHADOWS color wheel
│   ├── [0]            R shift
│   ├── [1]            G shift
│   ├── [2]            B shift
│   └── [3]            unused
├── highlights[4]      float        HIGHLIGHTS color wheel
│   ├── [0]            R shift
│   ├── [1]            G shift
│   ├── [2]            B shift
│   └── [3]            unused
├── midtones[4]        float        MIDTONES color wheel (power/gamma)
│   ├── [0]            R power
│   ├── [1]            G power
│   ├── [2]            B power
│   └── [3]            unused
├── midtones_Y         float        luminance gamma
├── chroma_global      float        global chroma boost
├── chroma[4]          float        per-zone chroma
│   ├── [0]            shadows chroma
│   ├── [1]            midtones chroma
│   ├── [2]            highlights chroma
│   └── [3]            unused
├── vibrance           float        vibrance (saturation of unsaturated)
├── contrast           float        contrast
├── saturation_global  float        global saturation
├── saturation[4]      float        per-zone saturation
│   ├── [0]            shadows
│   ├── [1]            midtones
│   ├── [2]            highlights
│   └── [3]            unused
├── brilliance_global  float        global brilliance
├── brilliance[4]      float        per-zone brilliance
│   ├── [0]            shadows
│   ├── [1]            midtones
│   ├── [2]            highlights
│   └── [3]            unused
├── hue_angle          float        global hue rotation (degrees)
├── shadows_weight     float        mask transition steepness
├── highlights_weight  float        mask transition steepness
├── midtones_weight    float        mask width
├── mask_grey_fulcrum  float        mask center point
├── white_fulcrum      float        scene white reference
├── grey_fulcrum       float        scene grey reference
├── gamut_LUT[4096]    float        gamut boundary LUT
├── max_chroma         float        max chroma for gamut mapping
└── saturation_formula int          formula version
```

---

## [10] colorout

```
ColoroutParams
├── type               int          1=DT_COLORSPACE_SRGB
├── filename[512]      char         output ICC profile
└── intent             int          0=DT_INTENT_PERCEPTUAL

cmatrix[4][4]          float        XYZ D50 -> output RGB
│   XYZ_D50_to_sRGB[4][4] = {
│       { 3.1339, -1.6169, -0.4906, 0 },
│       { -0.9788, 1.9161, 0.0335, 0 },
│       { 0.0719, -0.2290, 1.4052, 0 },
│       { 0, 0, 0, 0 }
│   }

XYZ_D65_to_sRGB[3][3]  float        (for direct D65 path)
│   ├── [0] = { 3.2405, -1.5371, -0.4985 }
│   ├── [1] = { -0.9693, 1.8760, 0.0416 }
│   └── [2] = { 0.0556, -0.2040, 1.0572 }
```

---

## Camera-Specific (gold.cpp)

```
cam_to_xyz[3][3]       float        Camera RGB -> XYZ D65
│   Sony A7 III (ILCE-7M3):
│   ├── [0] = { 0.6389, 0.1092, 0.1820 }
│   ├── [1] = { 0.2454, 0.7867, -0.0321 }
│   └── [2] = { 0.0132, -0.1291, 0.9523 }
│
│   Source: Adobe DNG ColorMatrix1 / ICC profile
│   TODO: Read from RAW EXIF or camera database
```

---

## Channel Index Convention

| Index | Bayer | RGB | RGBA | Lab |
|-------|-------|-----|------|-----|
| 0 | R | R | R | L |
| 1 | G1 | G | G | a |
| 2 | G2/B | B | B | b |
| 3 | B | - | A | A |

Bayer pattern `0x94949494` (RGGB):
```
  col 0   col 1
  ┌───┬───┐
  │ R │ G │  row 0
  ├───┼───┤
  │ G │ B │  row 1
  └───┴───┘
```
