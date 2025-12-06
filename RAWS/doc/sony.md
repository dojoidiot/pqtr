# Sony ARW Decoder

[back](../README.md)

The Sony ARW decoder is the reference "lab rat" implementation for RAW processing. It produces scene-referred linear RGB from Sony .ARW files, serving as the template for future camera support.

---

## Pipeline

The decoder processes Bayer data through six stages in canonical scene-referred order:

| Stage | Operation | Input | Output |
|-------|-----------|-------|--------|
| 1 | BLC on Bayer | CV_16UC1 | CV_32FC1 |
| 2 | WB on Bayer | CV_32FC1 | CV_32FC1 |
| 3 | Demosaic | CV_32FC1 | CV_32FC3 RGB |
| 4 | Color Matrix | CV_32FC3 | CV_32FC3 |
| 5 | Undistort | CV_32FC3 | CV_32FC3 |
| 6 | Crop | CV_32FC3 | CV_32FC3 |

**Output:** Scene-linear sRGB, [0,1+] range with HDR headroom preserved.

### Stage Details

**BLC (Black Level Correction):** Subtracts black level (380) and normalizes using white level (17220). Output range [0,1+]. Note: these are post-linearization values.

**WB (White Balance):** Applies per-channel gains directly to Bayer pattern before color interpolation. Uses RGGB multipliers from camera metadata.

**Demosaic:** Converts single-channel Bayer to 3-channel RGB using OpenCV bilinear interpolation. Outputs RGB order (not BGR).

**Color Matrix:** Transforms camera-native RGB to linear sRGB using the 3x3 matrix from Sony metadata tag 0x7310.

**Undistort:** Corrects lens barrel/pincushion distortion using Sony's embedded radial spline coefficients (tag 0x7037). See [Lens Distortion Correction](#lens-distortion-correction) below.

**Crop:** Removes optical black border pixels using DNG DefaultCropOrigin/DefaultCropSize tags.

---

## Lens Distortion Correction

Sony embeds lens-specific distortion correction data in each RAW file. The decoder extracts and applies this automatically.

### Metadata Tags

| Tag | Name | Format | Description |
|-----|------|--------|-------------|
| 0x7037 | DistortionCorrParams | int16s[N+1] | Radial spline knots |
| 0x7036 | DistortionCorrection | int16u | 0=Off, 1=Auto |

### Algorithm

Sony uses a spline-based radial correction. The tag contains:
- First value: knot count N (typically 11 or 16)
- Following N values: correction coefficients at equi-spaced radii from center to corner

The correction factor at radius r is:

```
g(r) = 1 + param[i] * 2^-14
```

Where `param[i]` is linearly interpolated from the N knots based on normalized radius (0=center, 1=corner).

The remap formula for undistortion:

```
g_normalized = g(r) / max(g)
source = center + (dest - center) * g_normalized
```

The `g_max` normalization prevents black borders by scaling the output to fit within the source image bounds.

### Example Values

**E PZ 18-105mm F4 G OSS @ 25mm:**
- 11 knots: `29 14 14 43 101 173 288 446 733 1251 2070`
- g_max = 1.126 (12.6% correction at corners)

**FE 16-35mm F2.8 GM II @ 30mm:**
- 16 knots: `-2 0 2 5 9 13 20 28 39 52 70 93 122 162 212 275`
- g_max = 1.017 (1.7% correction at corners)

### References

- [stannum.io: Sony ARW distortion correction](https://stannum.io/blog/0PwljB)
- [darktable lens.cc](https://github.com/darktable-org/darktable/blob/master/src/iop/lens.cc)

---

## Metadata

Values extracted from Sony ARW file metadata:

| Parameter | Tag | Typical Value | Notes |
|-----------|-----|---------------|-------|
| Black Level | (computed) | 380 | Post-linearization curve |
| White Level | (computed) | 17220 | Linearization curve max |
| WB RGGB | 0x7313 | 2572 1024 1024 1492 | Normalized: R=2.51, G=1.0, B=1.46 |
| Color Matrix | 0x7310 | See below | Fixed-point /1024 |
| CFA Pattern | SubIFD | RGGB | Standard Sony pattern |
| Crop Origin | 0xc61f | 12, 12 | DNG tag |
| Crop Size | 0xc620 | 6000 x 4000 | DNG tag |
| Distortion | 0x7037 | N + N knots | Radial spline coefficients |

### Color Matrix (0x7310)

```
| 1344/1024  -211/1024   -76/1024 |     | 1.312  -0.206  -0.074 |
|   -9/1024  1224/1024  -159/1024 |  =  |-0.009   1.195  -0.155 |
|    7/1024   -41/1024  1090/1024 |     | 0.007  -0.040   1.064 |
```

This matrix transforms camera RGB to linear sRGB. WB is not baked in - applied separately in stage 2.

---

## Preview & Style Metadata

The decoder extracts the embedded camera JPEG and rendering settings during `prepare()`:

| Field | Tag | Description |
|-------|-----|-------------|
| preview | 0x0201/0x0202 | Embedded JPEG (1616x1080 sRGB) |
| creative_style | 0xb020 | "Standard", "Vivid", "Portrait", etc. |
| dro | 0xb04f | "Off", "Auto", "Lv1"-"Lv5" |
| contrast | 0x2004 | -3 to +3 (0 = Normal) |
| saturation | 0x2005 | -3 to +3 (0 = Normal) |
| sharpness | 0x2006 | -3 to +3 (0 = Normal) |

These are stored in `RawMetadata` and passed through to `pipe::Head::view()`.

**Purpose:** The preview is the camera's display-referred rendering - the target for tune to match. Style metadata describes what camera settings produced it.

---

## Camera Look Matching

Match the camera's in-body rendering using the embedded preview as a tune reference.

### Architecture

```
pipe::Pipe::open(sink)
    │
    ├──► head.data()  →  scene-linear RGB + camera metadata
    │
    └──► head.view()  →  embedded preview + style metadata
                              │
                              ▼
                         tune reference
```

The decoder extracts both the scene-referred image and the camera's display-referred preview in a single `prepare()` call. No separate extraction step needed.

### Strategy

The embedded JPEG is just another reference image. Use tune.

```
head.data() ──► pipe (body) ──► output
                                   │
head.view() ─────────────────► diff ◄──┘
                                   │
                               tune (SPSA)
```

No special "camera look extraction" logic. Tune finds dials that minimize spectral loss against `head.view()`.

### View Metadata

Available via `head.view().info()`:

| Key | Example | Description |
|-----|---------|-------------|
| width | 1616 | Preview width |
| height | 1080 | Preview height |
| format | srgb_8bit | Color space |
| creative_style | Standard | Camera look preset |
| dro | Auto | Dynamic Range Optimizer |
| contrast | 0 | -3 to +3 |
| saturation | 0 | -3 to +3 |
| sharpness | 0 | -3 to +3 |

Style metadata describes what camera settings produced the preview. Useful for caching calibrated dials by style.

### Analysis Notes

**Sony Tone Curve** (tag 0x7010): Fixed per Creative Style, not per-image. Values `8000 10400 12900 14100` are identical across all Standard images.

**DRO Auto**: Scene-dependent processing. Camera doesn't record what it did - only that Auto was selected. Calibration captures the "average" behavior.

**Resolution**: Preview is 1616x1080. Spectral loss is content-invariant, works across resolutions.

---

## ARW2 Decompression

Sony ARW2 uses proprietary lossy compression (TIFF compression code 32767).

### Block Structure

ARW2 encodes 16-pixel blocks with:
- 11-bit min/max values (packed in first 4 bytes)
- 4-bit indices for min/max pixel positions
- 7-bit delta values for remaining 14 pixels
- Variable shift factor for delta scaling

```
Block structure (16 bytes → 16 pixels):
  Bytes 0-3: header (min, max, imax, imin packed)
  Bytes 4-15: 7-bit deltas (14 values × 7 bits = 98 bits)
```

Decompressed output range: 0-2047 typical, up to ~4095 with delta expansion.

### Linearization Curve

The decompressed 11-bit values require expansion to 14-bit linear values. LibRaw uses **<<1 indexing**: `output = curve[raw_value << 1]`.

**Curve structure (8192 entries):**
| Index Range | Mapping | Description |
|-------------|---------|-------------|
| 0-1999 | identity | curve[i] = i |
| 2000-4095 | expansion | 2000→2000, 4095→17220 |
| 4096+ | identity | overflow protection |

**Key kneepoints (index, output):**
```
(2000, 2000), (2500, 3000), (3000, 4800), (3500, 7900),
(4000, 15700), (4050, 16500), (4090, 17140), (4095, 17220)
```

**Example with <<1 indexing:**
```
raw_value  190 → curve[380]  →   380  (identity region)
raw_value 1000 → curve[2000] →  2000  (start of expansion)
raw_value 2029 → curve[4058] → 16628  (full expansion)
```

### Black/White Levels (Post-Linearization)

After linearization curve application:
- **Black level**: 380 (sensor floor after curve)
- **White level**: 17220 (curve maximum)

These differ from the sensor's native 14-bit levels (512/16383) because the linearization curve transforms the value space.

### Heuristics

**Highlight preservation:** Decompression allows values > 2047 for extreme highlights. No clamping during decode.

**Demosaic scaling:** Float Bayer [0,1+] scales to 16-bit for OpenCV demosaic, then back. Values > 1.0 preserved as HDR headroom.

**Color matrix output:** No clamping after matrix - scene-referred data may exceed [0,1] range. Clamping happens in TAIL output stage.

---

## Testing

### Build & Run

```bash
make -f Makefile.sony        # Build decoder and tools
make -f Makefile.sony test   # Run full test suite
make -f Makefile.sony clean  # Clean build
```

### Test Suite

The `test` target runs:

1. **sony** - Process test ARW through full pipeline, output with numbered grid overlay
2. **distortion_check** - Cross-correlation analysis comparing RAW output to camera preview

### Distortion Check

The distortion check tool measures pixel shift between processed RAW and embedded preview at grid intersections using normalized cross-correlation:

```
Grid intersection shifts (RAW vs Preview):
  X    Y   |  dx    dy  | score | dist
-----------|------------|-------|------
  500  500 |    5    2 | 0.852 |   5.4
 1000  500 |    3    2 | 0.691 |   3.6
 ...
Average shift: 6.0 pixels
```

Output: `tmp/distortion_check.png` - overlay visualization with shift vectors (green dots, red arrows 5x magnified).

A ~6px average shift on a 3936px image is ~0.15% error, acceptable given different processing pipelines.

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
│   ├── sony.md              ← This file
│   └── todo.md              ← Calibration tasks
├── src/
│   ├── main/part/
│   │   ├── sony.h           ← Public API + RawMetadata struct
│   │   ├── sony.cpp         ← TIFF parsing, ARW2 decompression
│   │   └── sony/
│   │       ├── prepare.cpp  ← File loading, metadata + preview extraction
│   │       ├── process.cpp  ← Pipeline orchestration
│   │       ├── blc_bayer.cpp
│   │       ├── wb_bayer.cpp
│   │       ├── demosaic.cpp
│   │       ├── color_matrix.cpp
│   │       ├── undistort.cpp  ← Lens distortion correction
│   │       └── crop.cpp
│   └── test/
│       ├── sony.cpp           ← Main test program
│       └── distortion_check.cpp ← Cross-correlation validation
├── var/
│   └── sony.ARW             ← Test file (DSC00144, E PZ 18-105mm)
├── tmp/
│   ├── sony.png             ← Scene-linear output with grid
│   ├── sony_preview.png     ← Embedded camera preview with grid
│   └── distortion_check.png ← Shift vector visualization
└── Makefile.sony
```

---

## References

- [LibRaw: RAW decoding pipeline](https://www.libraw.org/node/2309)
- [RawSpeed: ArwDecoder.cpp](https://github.com/darktable-org/rawspeed)
- [darktable: color calibration](https://docs.darktable.org/usermanual/4.0/en/module-reference/processing-modules/color-calibration/)
- [stannum.io: Sony ARW distortion correction](https://stannum.io/blog/0PwljB)
- [darktable PR #7092: Built-in lens correction](https://github.com/darktable-org/darktable/pull/7092)
