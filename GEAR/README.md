# PQTR:GEAR

[back](../README.md)

Camera gear library for PQTR. Handles RAW decoding, metadata extraction, and scene-linear normalization. Provides the `gear::load()` API that auto-detects camera format and returns scene-linear RGB plus embedded preview for LUTE learning.

## Role in PQTR

GEAR is where camera support R&D happens. It's isolated from the rest of the pipeline — adding new camera support doesn't change downstream code.

```
Camera RAW ──► [GEAR] ──► scene-linear RGB ──► [LUTE] ──► [VIBE]
                │
                ├── gear::load() API
                │   (format auto-detection)
                │
                └── Outputs:
                    - Scene-linear RGB (data)
                    - Metadata (camera, lens, exposure)
                    - Embedded preview (for LUTE learning)
```

### Separation of Concerns

**GEAR extracts canonical data. LUTE/VIBE apply style.**

| GEAR does | GEAR does NOT |
|-----------|---------------|
| Decompress sensor data | Apply tone curves |
| Black level subtraction | Add contrast/saturation |
| White balance (camera-reported) | Match camera JPEG appearance |
| Demosaic | Make "pleasing" output |
| ColorMatrix → standard colorspace | Any stylistic decisions |
| Extract embedded preview | Apply creative styles |
| Extract camera metadata | |

**GEAR output will look flat and desaturated.** This is correct — scene-linear data has no tone curve or color grading. The camera JPEG look is achieved by LUTE (learned from the embedded preview), not GEAR.

- **Produces**: `GEAR.a` or `GEAR_pure.a` static library
- **Exposes**: `gear::sony::decode()` → `pipe::Data`
- **Used by**: PIPE (links into `pipe.a`)

## Builds

| Build | Makefile | Output | Dependencies |
|-------|----------|--------|--------------|
| Pure (WASM) | `Makefile.pure` | `GEAR_pure.a` | stb_image only |
| Full (native) | `Makefile.gear` | `GEAR.a` | OpenCV |

**Use Pure build** for WebGPU/WASM pipeline. Processing happens in PIPE links (GPU).

**Use Full build** for legacy native apps that need OpenCV processing.

## Project Structure

```
GEAR/
├── inc/
│   └── gear.hpp              # Public API
├── lib/
│   ├── stb_image.h           # JPEG decode (no OpenCV)
│   ├── GEAR.a                # Full build (OpenCV)
│   └── GEAR_pure.a           # Pure build (no OpenCV)
├── src/
│   ├── main/
│   │   ├── gear.cpp          # Format detection, dispatch
│   │   ├── sony_link.cpp     # pipe::Link adapter (uses pure decoder)
│   │   └── part/
│   │       ├── sony.cpp      # Sony TIFF helpers
│   │       ├── sony.h        # Sony internal header (OpenCV)
│   │       ├── sony_pure.h   # Sony pure header (no OpenCV)
│   │       └── sony/         # Sony pipeline stages
│   │           ├── prepare.cpp       # OpenCV version
│   │           └── prepare_pure.cpp  # Pure version (stb_image)
│   └── test/
│       └── sony/             # Sony decoder tests
├── tmp/
│   ├── obj/                  # Build objects
│   └── pure/                 # Pure build objects
├── Makefile                  # Top-level (delegates)
├── Makefile.gear             # Builds lib/GEAR.a (OpenCV)
├── Makefile.pure             # Builds lib/GEAR_pure.a (no OpenCV)
└── Makefile.sony             # Sony decoder tests
```

## Building

```bash
# Pure build (recommended for WASM/WebGPU)
make -f Makefile.pure         # Build lib/GEAR_pure.a (no OpenCV)

# Full build (legacy, requires OpenCV)
make -f Makefile.gear         # Build lib/GEAR.a (OpenCV)

# Tests
make -f Makefile.sony test    # Run sony decoder test
make clean                    # Clean all artifacts
```

## Supported Formats

| Format | Camera | Status |
|--------|--------|--------|
| Sony ARW | ILCE-7M3, etc. | Implemented |
| Canon CR2/CR3 | — | Planned |
| Nikon NEF | — | Planned |
| DNG | Phone cameras | Planned |

## Pipe Link Contribution

GEAR contributes the first link in any pipe:

```cpp
pipe::Hold<pipe::Link> gear::read();
```

| Input Page | Output Page | Info Added |
|------------|-------------|------------|
| raw file buffer | BayerBuffer* | camera metadata, WB, color matrix |

### Usage in Pipe

```cpp
auto pipe = pipe::make();
pipe->link(gear::read());    // raw → Bayer (CPU)
pipe->link(wgpu::open());    // Bayer → GPU
pipe->link(pipe::blc());     // Black level correction
pipe->link(pipe::wb());      // White balance
pipe->link(pipe::demosaic()); // Bayer → RGB
pipe->link(pipe::cst());     // Color matrix
pipe->link(pipe::crop());    // Active area
pipe->link(wgpu::shut());    // GPU → CPU
```

The link auto-detects format from magic bytes and dispatches to the appropriate manufacturer decoder (Sony, Canon, Nikon, etc.).

See [PIPE](../PIPE/README.md) for the full pipeline model.

## Info Contract

GEAR implementations MUST/MAY populate Info fields for downstream PIPE links.

### MUST Provide (Required)

| Field | Type | Used By | Description |
|-------|------|---------|-------------|
| `width` | dial | all | Image width in pixels |
| `height` | dial | all | Image height in pixels |
| `black_level` | dial | blc | Sensor black level (raw units) |
| `white_level` | dial | blc | Sensor white level (raw units) |
| `bayer_pattern` | dial | wb, demosaic | CFA pattern: 46=RGGB, 47=GRBG, 48=BGGR, 49=GBRG |
| `wb_r` | dial | wb | Red channel gain (normalized, G=1.0) |
| `wb_g` | dial | wb | Green channel gain (1.0) |
| `wb_b` | dial | wb | Blue channel gain (normalized, G=1.0) |

### MAY Provide (Optional)

| Field | Type | Used By | Description |
|-------|------|---------|-------------|
| `color_matrix` | data[9] | cst | 3x3 camera RGB → sRGB matrix (row-major). Default: identity |
| `crop_left` | dial | crop | Active area left offset. Default: 0 |
| `crop_top` | dial | crop | Active area top offset. Default: 0 |
| `crop_width` | dial | crop | Active area width. Default: full width |
| `crop_height` | dial | crop | Active area height. Default: full height |

### MAY Provide (Metadata)

| Field | Type | Description |
|-------|------|-------------|
| `camera_make` | text | Manufacturer (Sony, Canon, Nikon) |
| `camera_model` | text | Model name (ILCE-7M4, EOS R5) |
| `iso` | dial | ISO sensitivity |
| `shutter_speed` | dial | Exposure time in seconds |
| `aperture` | dial | F-number |
| `focal_length` | dial | Focal length in mm |
| `lens_model` | text | Lens name |
| `creative_style` | text | Camera style preset (Standard, Vivid, etc.) |
| `dro` | text | Dynamic range optimizer setting |
| `orientation` | dial | EXIF orientation |

### Preview (for LUTE)

GEAR implementations SHOULD also extract the embedded camera preview JPEG for LUTE learning. This is returned separately from Info as part of the Result struct.

### Example: Sony Implementation

```cpp
// Populate MUST fields
info.dial("width", metadata.width);
info.dial("height", metadata.height);
info.dial("black_level", metadata.black_level);
info.dial("white_level", metadata.white_level);
info.dial("bayer_pattern", metadata.bayer_pattern);

// Normalize WB gains (G=1.0)
float g_ref = metadata.wb_rggb[1];
info.dial("wb_r", metadata.wb_rggb[0] / g_ref);
info.dial("wb_g", 1.0f);
info.dial("wb_b", metadata.wb_rggb[2] / g_ref);

// MAY fields
float matrix[9] = { ... };
info.data("color_matrix", matrix, 9);
info.dial("crop_left", metadata.crop_left);
info.dial("crop_top", metadata.crop_top);
info.dial("crop_width", metadata.crop_width);
info.dial("crop_height", metadata.crop_height);

// Metadata
info.text("camera_make", metadata.camera_make);
info.text("camera_model", metadata.camera_model);
info.dial("iso", metadata.iso);
```

---

## Legacy API

```cpp
// GEAR/inc/gear.hpp
namespace gear {
    struct Result {
        bool success;
        pipe::View data;          // Scene-linear RGB (CV_32FC3)
        pipe::InfoMap dataInfo;   // Metadata
        pipe::View preview;       // Embedded camera JPEG (BGR, 8-bit)
        pipe::InfoMap previewInfo; // Preview metadata (style settings)
    };

    Result load(pqtr::Sink& sink);
}
```

Direct `gear::load()` API remains for non-pipe usage.

## Adding a New Format

1. Create `src/main/part/<format>.cpp` and `src/main/part/<format>/` directory
2. Implement decoder following Sony pattern (prepare → process_linear)
3. Add format detection in `src/main/gear.cpp`
4. Add source files to `Makefile.gear`
5. Create test in `src/test/<format>/`
6. Create `doc/<format>.md` for technical documentation

The key contract: return `pipe::View` (CV_32FC3) containing scene-linear sRGB in [0,1+] range.

---

## Sony ARW Implementation

### Pipeline

```
RAW → BLC (Bayer) → WB (Bayer) → Demosaic → Color Matrix → Crop → Linear RGB
```

| Stage | Operation | Description |
|-------|-----------|-------------|
| 1 | BLC on Bayer | Subtract black level, normalize by white level |
| 2 | WB on Bayer | Apply per-channel gains before interpolation |
| 3 | Demosaic | Bayer → RGB via OpenCV bilinear |
| 4 | Color Matrix | Camera RGB → linear sRGB (Sony tag 0x7310) |
| 5 | Crop | Remove optical black borders |

**Output:** Scene-linear sRGB, [0,1+] range with HDR headroom preserved.

### Embedded Preview

Extracts camera-rendered JPEG and style metadata (creative_style, contrast, saturation, sharpness). Provides reference target for LUTE learning.

See [doc/sony.md](doc/sony.md) for full technical documentation.

---

## Dependencies

- **OpenCV** (from PIPE): `../PIPE/lib/opencv`
- **PIPE headers**: `../PIPE/inc` (pipe.hpp, sink.hpp)
- **C++17** compiler
- No GPL dependencies (clean-room implementation)
