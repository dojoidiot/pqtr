# PQTR:RAWS

[back](../README.md)

RAW decoder library for PQTR. Provides `raws::decode()` API that auto-detects camera format and returns scene-linear RGB.

## Role in PQTR

RAWS is where camera support R&D happens. It's isolated from LABS—adding new camera support doesn't change downstream code.

```
Camera RAW ──► [RAWS] ──► scene-linear RGB ──► [LABS]
                │
                └── raws::decode() API
                    (format auto-detection)
```

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
    struct Result {
        bool success;
        pipe::View data;          // Scene-linear RGB (CV_32FC3)
        pipe::Info dataInfo;      // Metadata
        pipe::View preview;       // Embedded camera JPEG (BGR, 8-bit)
        pipe::Info previewInfo;   // Preview metadata
    };

    Result decode(pqtr::Sink& sink);
}
```

LABS calls `raws::decode()` and receives scene-linear RGB. It knows nothing about Sony, Canon, or Nikon internals.

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
2. Implement decoder following Sony pattern (prepare → process_linear)
3. Add format detection in `src/main/raws.cpp`
4. Add source files to `Makefile.raws`
5. Create `doc/<format>.md` for technical documentation

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

Extracts camera-rendered JPEG and style metadata (creative_style, contrast, saturation, sharpness). Provides reference target for LABS tune module.

See [doc/sony.md](doc/sony.md) for full technical documentation.

---

## Dependencies

- **OpenCV** (from LABS): `../LABS/lib/opencv`
- **LABS headers**: `../LABS/inc` (pipe.hpp, sink.hpp)
- **C++17** compiler
- No GPL dependencies (clean-room implementation)
