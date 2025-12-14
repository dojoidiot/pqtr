# PQTR:GEAR

[back](../README.md)

Camera gear library for PQTR. Handles RAW decoding, metadata extraction, and scene-linear normalization. Provides the `gear::read()` API that auto-detects camera format and returns Bayer data plus metadata for pipeline processing.

## Role in PQTR

GEAR is where camera support R&D happens. It's isolated from the rest of the pipeline - adding new camera support doesn't change downstream code.

```
Camera RAW --> [GEAR] --> Bayer + metadata --> [WGPU Pipeline]
                |
                +-- gear::read() API
                |   (format auto-detection)
                |
                +-- Outputs:
                    - Bayer sensor data
                    - Camera metadata
                    - Embedded preview (for LUTE learning)
```

### Separation of Concerns

**GEAR extracts canonical data. Pipeline applies processing.**

| GEAR does | GEAR does NOT |
|-----------|---------------|
| Extract sensor data | Apply tone curves |
| Extract metadata | Add contrast/saturation |
| Extract embedded preview | Make "pleasing" output |
| Format detection | Any stylistic decisions |

Processing (BLC, WB, demosaic, color matrix) happens in the WGPU pipeline.

- **Produces**: `GEAR_pure.a` static library
- **Exposes**: `gear::sony::decode()` -> `pipe::Data`
- **Used by**: PIPE (links into pipeline)

## Project Structure

```
GEAR/
+-- inc/
|   +-- gear.hpp              # Public API
+-- lib/
|   +-- stb_image.h           # JPEG decode
|   +-- GEAR_pure.a           # Built library
+-- src/
|   +-- main/
|   |   +-- sony_link.cpp     # pipe::Link adapter
|   |   +-- part/
|   |       +-- sony_pure.h   # Sony internal header
|   |       +-- sony/
|   |           +-- prepare_pure.cpp  # Sony decoder
|   +-- test/
|       +-- dawn/             # DAWN GPU tests
+-- Makefile                  # Top-level
+-- Makefile.pure             # Builds lib/GEAR_pure.a
+-- Makefile.dawn             # DAWN shader tests
```

## Building

```bash
make                          # Build lib/GEAR_pure.a
make test-dawn                # Run DAWN GPU shader tests
make clean                    # Clean all artifacts
```

## Supported Formats

| Format | Camera | Status |
|--------|--------|--------|
| Sony ARW | ILCE-7M3, etc. | Implemented |
| Canon CR2/CR3 | - | Planned |
| Nikon NEF | - | Planned |
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
pipe->link(gear::read());    // raw -> Bayer (CPU)
pipe->link(wgpu::open());    // Bayer -> GPU
pipe->link(pipe::blc());     // Black level correction
pipe->link(pipe::wb());      // White balance
pipe->link(pipe::demosaic()); // Bayer -> RGB
pipe->link(pipe::cst());     // Color matrix
pipe->link(pipe::crop());    // Active area
pipe->link(wgpu::shut());    // GPU -> CPU
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
| `color_matrix` | data[9] | cst | 3x3 camera RGB -> sRGB matrix (row-major). Default: identity |
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

GEAR implementations SHOULD also extract the embedded camera preview JPEG for LUTE learning. This is returned separately as part of the BayerBuffer struct.

## Adding a New Format

1. Create `src/main/part/<format>/` directory
2. Implement decoder following Sony pattern
3. Add format detection in sony_link.cpp
4. Add source files to `Makefile.pure`
5. Create test in `src/test/<format>/`

## Dependencies

- **PIPE headers**: `../PIPE/inc` (pipe.hpp)
- **stb_image**: Bundled (JPEG decode)
- **C++17** compiler
- **GPU compute**: WGPU (WebGPU)
