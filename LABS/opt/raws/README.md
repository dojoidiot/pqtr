# Sony ARW Decoder - Reference Implementation

[back](../../README.md)

## Overview

A GPL-free Sony .ARW decoder producing **scene-referred linear RGB** for the labs styling pipeline. Serves as reference implementation for clean-room RAW decoder development.

## Quick Start

```bash
cd /home/z/base/code/pqtr/LABS/opt/raws
make -f Makefile.sony
./tmp/sony/sony var/sony.ARW
```

**Output**:
- `./tmp/sony.png` - Scene-linear RGB (processed)
- `./tmp/sony_preview.png` - Embedded camera JPEG (reference)

---

## Pipeline

Scene-referred canonical order (accuracy-first):

```
RAW → BLC (Bayer) → WB (Bayer) → Demosaic → Color Matrix → Crop → Linear RGB
```

| Stage | Operation | Description |
|-------|-----------|-------------|
| 1 | BLC on Bayer | Subtract black level (512), normalize by white level (15360) |
| 2 | WB on Bayer | Apply per-channel gains to Bayer pattern before interpolation |
| 3 | Demosaic | Bayer → RGB (not BGR) via OpenCV bilinear |
| 4 | Color Matrix | Camera RGB → linear sRGB (Sony tag 0x7310) |
| 5 | Crop | Remove optical black borders (DNG crop tags) |

**Output:** Scene-linear sRGB, [0,1+] range with HDR headroom preserved.

### Embedded Preview Extraction

The decoder also extracts the camera's embedded JPEG preview and style metadata:

| Field | Description |
|-------|-------------|
| `preview` | Camera-rendered JPEG (1616x1080, sRGB) |
| `creative_style` | "Standard", "Vivid", "Portrait", etc. |
| `dro` | Dynamic Range Optimizer setting |
| `contrast` | -3 to +3 |
| `saturation` | -3 to +3 |
| `sharpness` | -3 to +3 |

This provides a reference target for tune - match the camera's look without reverse-engineering Sony's processing.

See [doc/sony.md](doc/sony.md) for full technical documentation.
See [doc/view.md](doc/view.md) for camera look extraction architecture.

---

## Key Decisions

### Accuracy Over Performance

This implementation applies WB on Bayer data before demosaic - the physically correct order. This requires scaling float→uint16→float for OpenCV demosaic, but produces accurate colors.

Previous "performance-first" approach (demosaic first) caused color errors (Yellow Sky Bug).

### Maximum Manufacturer Processing

The decoder implements all Sony-specified processing, including the linearization curve (tag 0x7010) that LibRaw ignores:

```
LibRaw:         Raw max = 16628 → No curve → Clipped highlights
Custom Decoder: Raw max = 2029 → Linearization curve → 4232 (expanded)
```

### Scene-Referred Output

Output is intentionally "flat" - no gamma, no tone mapping. This is correct for scene-referred data that feeds into the BODY styling pipeline.

---

## Project Structure

```
opt/raws/
├── README.md
├── Makefile.sony
├── doc/
│   └── sony.md           ← Technical documentation (source of truth)
├── src/
│   ├── main/part/
│   │   ├── sony.h        ← Public API
│   │   ├── sony.cpp      ← TIFF parsing, ARW2 decompression
│   │   └── sony/
│   │       ├── prepare.cpp
│   │       ├── process.cpp
│   │       ├── blc_bayer.cpp
│   │       ├── wb_bayer.cpp
│   │       ├── demosaic.cpp
│   │       ├── color_matrix.cpp
│   │       └── crop.cpp
│   └── test/
│       └── sony.cpp
├── var/
│   └── sony.ARW          ← Test file (ILCE-7M3)
└── tmp/
    └── sony.png          ← Output
```

---

## Template for Other Formats

To create a decoder for another manufacturer (e.g., Nikon NEF):

1. Copy `opt/raws/` to `opt/raws_nikon/`
2. Rename `sony.*` files to `nikon.*`
3. Replace ARW2 decompression with format-specific algorithm
4. Modify TIFF/MakerNote parsing for manufacturer tags
5. Adjust color matrix and WB extraction
6. Create `doc/nikon.md` following same structure

---

## Dependencies

- **OpenCV** (local build): `../../lib/opencv`
- **C++11** compiler
- No GPL dependencies

---

## Credits

Clean-room reverse engineering of Sony ARW2 format.
No code derived from GPL-licensed software.
Safe for commercial use.
