# Sony ARW Decoder

[back](../README.md)

The Sony ARW decoder produces scene-referred linear RGB from Sony .ARW files. Output feeds directly into the labs styling pipeline.

---

## Pipeline

The decoder processes Bayer data through five stages in canonical scene-referred order:

| Stage | Operation | Input | Output |
|-------|-----------|-------|--------|
| 1 | BLC on Bayer | CV_16UC1 | CV_32FC1 |
| 2 | WB on Bayer | CV_32FC1 | CV_32FC1 |
| 3 | Demosaic | CV_32FC1 | CV_32FC3 RGB |
| 4 | Color Matrix | CV_32FC3 | CV_32FC3 |
| 5 | Crop | CV_32FC3 | CV_32FC3 |

**Output:** Scene-linear sRGB, [0,1+] range with HDR headroom preserved.

### Stage Details

**BLC (Black Level Correction):** Subtracts black level (512) and normalizes using white level (15360). Output range [0,1+].

**WB (White Balance):** Applies per-channel gains directly to Bayer pattern before color interpolation. Uses RGGB multipliers from camera metadata.

**Demosaic:** Converts single-channel Bayer to 3-channel RGB using OpenCV bilinear interpolation. Outputs RGB order (not BGR).

**Color Matrix:** Transforms camera-native RGB to linear sRGB using the 3x3 matrix from Sony metadata tag 0x7310.

**Crop:** Removes optical black border pixels using DNG DefaultCropOrigin/DefaultCropSize tags.

---

## Metadata

Values extracted from Sony ARW file metadata:

| Parameter | Tag | Typical Value | Notes |
|-----------|-----|---------------|-------|
| Black Level | SubIFD | 512 | Per-channel, uniform |
| White Level | SR2SubIFD | 15360 | Practical clipping point |
| WB RGGB | 0x7313 | 2572 1024 1024 1492 | Normalized: R=2.51, G=1.0, B=1.46 |
| Color Matrix | 0x7310 | See below | Fixed-point /1024 |
| CFA Pattern | SubIFD | RGGB | Standard Sony pattern |
| Crop Origin | 0xc61f | 12, 12 | DNG tag |
| Crop Size | 0xc620 | 6000 x 4000 | DNG tag |

### Color Matrix (0x7310)

```
| 1344/1024  -211/1024   -76/1024 |     | 1.312  -0.206  -0.074 |
|   -9/1024  1224/1024  -159/1024 |  =  |-0.009   1.195  -0.155 |
|    7/1024   -41/1024  1090/1024 |     | 0.007  -0.040   1.064 |
```

This matrix transforms camera RGB to linear sRGB. WB is not baked in - applied separately in stage 2.

---

## ARW2 Decompression

Sony ARW2 uses proprietary lossy compression:

- 7-bit delta encoding per 32-pixel chunks
- 11-bit non-linear output after decompression
- Linearization curve expands to ~17204 max (>14 bits)
- Row interleaving: odd rows R/G, even rows G/B

The linearization curve provides highlight recovery. Values above ~2000 expand 4x for dynamic range preservation.

### Heuristics

**Highlight preservation:** Decompression allows values > 2047 for extreme highlights. No clamping during decode.

**Demosaic scaling:** Float Bayer [0,1+] scales to 16-bit for OpenCV demosaic, then back. Values > 1.0 preserved as HDR headroom.

**Color matrix output:** No clamping after matrix - scene-referred data may exceed [0,1] range. Clamping happens in TAIL output stage.

---

## API

```cpp
namespace sony {
    class Decoder {
    public:
        // Decode ARW file to Bayer data + metadata
        static bool prepare(pqtr::Sink &sink, cv::UMat &data,
                           Info &info, RawMetadata &metadata);

        // Process Bayer to scene-linear RGB
        static bool process_linear(const cv::UMat &bayer,
                                   const RawMetadata &metadata,
                                   cv::UMat &rgb);

        // Full pipeline with gamma (display-referred)
        static bool process(const cv::UMat &bayer,
                           const RawMetadata &metadata,
                           cv::UMat &rgb);
    };
}
```

---

## Files

```
opt/raws/
├── doc/
│   └── sony.md              ← This file
├── src/
│   ├── main/part/
│   │   ├── sony.h           ← Public API
│   │   ├── sony.cpp         ← TIFF parsing, ARW2 decompression
│   │   └── sony/
│   │       ├── prepare.cpp  ← File loading, metadata extraction
│   │       ├── process.cpp  ← Pipeline orchestration
│   │       ├── blc_bayer.cpp
│   │       ├── wb_bayer.cpp
│   │       ├── demosaic.cpp
│   │       ├── color_matrix.cpp
│   │       └── crop.cpp
│   └── test/
│       └── sony.cpp         ← Test program
├── var/
│   └── sony.ARW             ← Test file (ILCE-7M3)
└── tmp/
    └── sony.png             ← Output
```

---

## References

- [LibRaw: RAW decoding pipeline](https://www.libraw.org/node/2309)
- [RawSpeed: ArwDecoder.cpp](https://github.com/darktable-org/rawspeed)
- [darktable: color calibration](https://docs.darktable.org/usermanual/4.0/en/module-reference/processing-modules/color-calibration/)
