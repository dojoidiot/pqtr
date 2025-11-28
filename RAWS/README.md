# PQTR:RAWS

[back](../README.md)

RAW decoder library for PQTR. Provides `raws::decode()` API that auto-detects camera format and returns **camera-native RGB** plus metadata.

## Architecture: Separation of Concerns

RAWS and LABS have distinct responsibilities:

| Component | Responsibility | Output |
|-----------|----------------|--------|
| **RAWS** | Sensor data extraction | Camera-native RGB + metadata |
| **LABS** | Color science | Scene-linear sRGB |

```
Camera RAW ──► [RAWS] ──► camera-native RGB ──► [LABS HEAD] ──► scene-linear sRGB
                │              + metadata           │
                │                                   ├── WB (from metadata)
                └── raws::decode() API              └── ColorMatrix (from metadata)
                    (format auto-detection)
```

**Why this separation?**

1. **RAWS = sensor-specific**: Only knows how to extract data from camera formats
2. **LABS = camera-agnostic**: Applies color science using metadata; same code for all cameras
3. **Optimizer benefits**: When LABS applies WB/matrix, the tune optimizer learns the *actual* camera transform, not a correction to the decoder's interpretation

## Role in PQTR

RAWS is where camera support R&D happens. Adding new camera support requires only:
1. New decoder in RAWS that outputs camera-native RGB
2. Populate `ColorMeta` with camera's WB/matrix/distortion values
3. LABS code unchanged—it applies metadata automatically

- **Produces**: `raws.a` static library
- **Exposes**: `raws::decode(Sink&)` → `raws::Result`
- **Used by**: LABS (links into `labs.a`)

## Supported Formats

| Format | Camera | Status |
|--------|--------|--------|
| Sony ARW | ILCE-7M3, etc. | Implemented |
| Canon CR2/CR3 | — | Planned |
| Nikon NEF | — | Planned |
| DNG | Phone cameras | Planned |

## Public API

```cpp
// RAWS/inc/raws.hpp
namespace raws {

    // Color science metadata (for LABS to apply)
    struct ColorMeta {
        float wb_r, wb_g, wb_b;           // White balance multipliers
        cv::Matx33f color_matrix;         // Camera RGB → sRGB matrix
        int16_t distortion_params[16];    // Lens distortion coefficients
        int distortion_knot_count;
        bool has_distortion;
    };

    struct Result {
        bool success;
        pipe::View data;          // Camera-native RGB (CV_32FC3) - no WB, no matrix
        pipe::Info dataInfo;      // Metadata (includes color_space="camera_native")
        ColorMeta colorMeta;      // WB, matrix, distortion for LABS to apply
        pipe::View preview;       // Embedded camera JPEG (BGR, 8-bit)
        pipe::Info previewInfo;   // Preview metadata
    };

    Result decode(pqtr::Sink& sink);
}
```

LABS calls `raws::decode()` and receives:
- **data**: Camera-native RGB (no color science applied)
- **colorMeta**: WB/matrix/distortion values for LABS HEAD to apply

LABS applies `colorMeta.wb_*` and `colorMeta.color_matrix` in HEAD to produce scene-linear sRGB. This keeps RAWS focused on sensor extraction while LABS owns all color science.

## Project Structure

```
RAWS/
├── inc/
│   └── raws.hpp          # Public API
├── lib/
│   └── raws.a            # Built library
├── src/
│   ├── main/
│   │   └── raws.cpp      # Format detection, dispatch
│   └── main/part/
│       ├── sony.h        # Sony decoder (internal)
│       ├── sony.cpp
│       └── sony/         # Sony pipeline stages
├── doc/
│   └── sony.md           # Sony technical docs
├── Makefile.raws         # Builds raws.a
└── Makefile.sony         # Builds standalone test binary
```

## Building

```bash
# Library (used by LABS)
make -f Makefile.raws     # Produces lib/raws.a

# Standalone test binary (for decoder development)
make -f Makefile.sony     # Produces tmp/sony/sony
./tmp/sony/sony var/sony.ARW
```

## Adding a New Format

1. Create `src/main/part/<format>.h` and `src/main/part/<format>/` directory
2. Implement decoder following Sony pattern:
   - `prepare()`: Extract Bayer data + metadata
   - `process_linear()`: BLC → Demosaic → Crop (minimal pipeline)
3. Add format detection in `src/main/raws.cpp`
4. Populate `raws::ColorMeta` with camera's:
   - White balance multipliers (`wb_r`, `wb_g`, `wb_b`)
   - Color matrix (camera RGB → sRGB)
   - Lens distortion coefficients (if available)
5. Add source files to `Makefile.raws`
6. Create `doc/<format>.md` for technical documentation

**Key contract:**
- Return `pipe::View` (CV_32FC3) containing **camera-native RGB** in [0,1+] range
- Populate `ColorMeta` with WB/matrix/distortion for LABS to apply
- LABS handles all color science—decoder just extracts sensor data

---

## Sony ARW Implementation

### Pipeline (Minimal)

```
RAW → BLC (Bayer) → Demosaic → Crop → Camera-native RGB
                                        + WB metadata
                                        + ColorMatrix metadata
                                        + Distortion metadata
```

| Stage | Operation | Description |
|-------|-----------|-------------|
| 1 | BLC on Bayer | Subtract black level (512), normalize by white level (15360) |
| 2 | Demosaic | Bayer → RGB via OpenCV bilinear |
| 3 | Crop | Remove optical black borders |

**Deferred to LABS:**
- WB (passed as `colorMeta.wb_r/g/b`)
- Color Matrix (passed as `colorMeta.color_matrix`)
- Lens Distortion (passed as `colorMeta.distortion_params`)

**Output:** Camera-native RGB, [0,1+] range with HDR headroom preserved. Strong green cast expected (no WB applied).

### Embedded Preview

Extracts camera-rendered JPEG and style metadata (creative_style, contrast, saturation, sharpness). Provides reference target for LABS tune module.

See [doc/sony.md](doc/sony.md) for full technical documentation.

---

## Dependencies

- **OpenCV** (from LABS): `../LABS/lib/opencv`
- **LABS headers**: `../LABS/inc` (pipe.hpp, sink.hpp)
- **C++17** compiler
- No GPL dependencies (clean-room implementation)
