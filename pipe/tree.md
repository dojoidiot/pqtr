# Gold Pipeline Parameter Tree

Pipeline: `ARW -> rawprepare -> temperature -> highlights -> demosaic -> exposure -> colorin -> colorout -> PNG`

```
gold.cpp
├── PipeState (shared)
│   ├── width           = meta.width
│   ├── height          = meta.height
│   └── filters         = 0x94949494 (RGGB)
│
├── [1] rawprepare
│   ├── RawprepareParams
│   │   ├── crop_x/y/w/h = 0
│   │   ├── black[4]     = meta.black_level (512)
│   │   └── white        = meta.white_level (16383)
│   └── RawprepareData (computed)
│       ├── scale        = 1.0 / (white - black)
│       └── sub[4]       = black / 65535.0
│
├── [2] temperature
│   └── TemperatureData
│       ├── coeffs[0]    = meta.wb_rggb[0] / wb_rggb[1]  (~2.51)
│       ├── coeffs[1]    = 1.0
│       ├── coeffs[2]    = meta.wb_rggb[2] / wb_rggb[1]  (~1.46)
│       ├── coeffs[3]    = 1.0
│       └── preset       = 4 (camera)
│
├── [3] highlights
│   └── HighlightsData
│       ├── mode         = DT_IOP_HIGHLIGHTS_OPPOSED
│       ├── blendL       = 1.0
│       ├── blendC       = 0.0
│       ├── strength     = 1.0
│       ├── clip         = 1.0
│       ├── noise_level  = 0.0
│       ├── iterations   = 30
│       ├── scales       = 6
│       ├── candidating  = 0.4
│       ├── combine      = 2.0
│       ├── recovery     = 0
│       └── solid_color  = 0.0
│
├── [4] demosaic
│   └── DemosaicParams
│       ├── demosaicing_method = 5 (RCD)
│       ├── dual_thrs    = 0.2
│       ├── cs_thrs      = 0.40
│       └── cs_iter      = 8
│
├── [5] exposure
│   └── ExposureParams
│       ├── mode         = 0 (manual)
│       ├── black        = 0.0
│       ├── exposure     = 0.7 EV
│       ├── deflicker_percentile    = 50.0
│       ├── deflicker_target_level  = -4.0
│       └── compensate_exposure_bias = 0
│
├── [6] colorin (simplified)
│   ├── cam_to_xyz[3][3]     = Sony A7 III matrix (hardcoded)
│   │   ├── [0] = { 0.6389, 0.1092, 0.1820 }
│   │   ├── [1] = { 0.2454, 0.7867, -0.0321 }
│   │   └── [2] = { 0.0132, -0.1291, 0.9523 }
│   └── XYZ_to_REC2020[4][4] = (from colorin.c)
│
├── [7-9] colorout (simplified, no filmic)
│   ├── REC2020_to_XYZ[4][4] = (from colorin.c)
│   ├── XYZ_D65_to_sRGB[3][3] = (from colorout.c)
│   └── sRGB gamma transfer function
│
└── [10] PNG output
    └── stbi_write_png()
```

## Matrices (from modules)

### colorin.c
```c
REC2020_to_XYZ[4][4] = {
    { 0.673474789, 0.165675461, 0.125049725, 0 },
    { 0.279040545, 0.675347328, 0.045612101, 0 },
    { -0.001932710, 0.029981442, 0.796851277, 0 },
    { 0, 0, 0, 0 }
};

XYZ_to_REC2020[4][4] = {
    { 1.647250295, -0.393625855, -0.235971376, 0 },
    { -0.682616651, 1.647609591, 0.012813044, 0 },
    { 0.029678674, -0.062945843, 1.253884912, 0 },
    { 0, 0, 0, 0 }
};
```

### colorout.c
```c
XYZ_D65_to_sRGB[3][3] = {
    { 3.2404542, -1.5371385, -0.4985314 },
    { -0.9692660, 1.8760108, 0.0415560 },
    { 0.0556434, -0.2040259, 1.0572252 }
};
```

### gold.cpp (camera-specific)
```c
cam_to_xyz[3][3] = {  /* Sony A7 III - should come from DNG/ICC */
    { 0.6389, 0.1092, 0.1820 },
    { 0.2454, 0.7867, -0.0321 },
    { 0.0132, -0.1291, 0.9523 }
};
```

## TODO
- [ ] Read cam_to_xyz from RAW metadata or ICC profile
- [ ] Add channelmixerrgb module
- [ ] Add filmicrgb tone mapping
- [ ] Add colorbalancergb grading
