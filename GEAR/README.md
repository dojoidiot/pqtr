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

- **Produces**: `GEAR.a` static library
- **Exposes**: `gear::load(Sink&)` → `gear::Result`
- **Used by**: PIPE (links into `pipe.a`)

## Project Structure

```
GEAR/
├── inc/
│   └── gear.hpp              # Public API
├── lib/
│   └── GEAR.a                # Built library
├── src/
│   ├── main/
│   │   ├── gear.cpp          # Format detection, dispatch
│   │   └── part/
│   │       ├── sony.cpp      # Sony decoder entry
│   │       ├── sony.h        # Sony internal header
│   │       └── sony/         # Sony pipeline stages
│   └── test/
│       └── sony/             # Sony decoder tests
│           ├── sony.cpp      # Main decoder test
│           └── distortion.cpp
├── tmp/
│   ├── obj/                  # Build objects
│   ├── bin/                  # Test binaries
│   └── var/                  # Test output
├── Makefile                  # Top-level (delegates)
├── Makefile.gear             # Builds lib/GEAR.a
└── Makefile.sony             # Sony decoder tests
```

## Building

```bash
make              # Build lib/GEAR.a (default)
make test         # Run sony decoder test
make test-all     # Run full test suite (+ distortion)
make all          # Build everything
make clean        # Clean all artifacts
```

## Supported Formats

| Format | Camera | Status |
|--------|--------|--------|
| Sony ARW | ILCE-7M3, etc. | Implemented |
| Canon CR2/CR3 | — | Planned |
| Nikon NEF | — | Planned |
| DNG | Phone cameras | Planned |

## Public API

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

PIPE calls `gear::load()` and receives scene-linear RGB plus the embedded preview. It knows nothing about Sony, Canon, or Nikon internals.

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
