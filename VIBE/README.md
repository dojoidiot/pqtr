# VIBE

Style processing module for PQTR. Handles 17 image transform modules with PIMPL pattern for future GPU backends.

## What It Does

VIBE transforms images to match a target style. Given a source image and reference, it:
1. Optimizes dials to minimize perceptual difference (tune mode - in LABS)
2. Applies the learned style to new images (view mode)

## Architecture

### Type Aliases

```cpp
namespace vibe {
    using View = cv::UMat;      // Image buffer
    using Name = std::string;   // File paths
    using Dial = float;         // 0.0-1.0 normalized parameter
    using Grid = const float*;  // LUT/matrix data pointer
}
```

### 17 Modules

| Module | Type | Parameters | Purpose |
|--------|------|------------|---------|
| exposure | Dial | 1 | EV adjustment |
| white_balance | Dial | 2 | Temperature + tint |
| tone_map | Dial | 7 | Contrast, highlights, shadows, pivots |
| global_color | Dial | 3 | Vibrance, saturation, density |
| geometric | Dial | 6 | Crop, zoom, rotation |
| selective_color | Dial | 24 | 8-band HSL adjustments |
| split_tone | Dial | 4 | Shadow/highlight grading |
| detail | Dial | 4 | Sharpen, denoise |
| baseline | Meta | 2 | Highlight recovery + exposure |
| sigmoid | Meta | 4 | darktable log-logistic curve |
| base_curve | Meta | 768 | Per-channel response curve |
| color_matrix | Meta | 9 | 3x3 RGB transform |
| lut_curve | Meta | N×3 | Per-channel 1D LUT |
| lut3d | Meta | N³×3 | 3D color cube |
| hsv_lut | Meta | 36×12×3 | HSV delta adjustments |
| poly_color | Meta | 30 | Quadratic polynomial RGB→RGB |
| local_tone | Meta | 3 | Bilateral-style local adaptation |

### PIMPL Pattern

```
VIBE/src/main/
├── mods/              # OpenCV backend (17 files)
│   ├── exposure.cpp
│   ├── sigmoid.cpp
│   └── ...
└── (future: dawn/)    # WebGPU backend via WGSL shaders
```

## Test Harness

Theory-based golden reference testing validates implementations against pure math:

```bash
make gold   # Generate reference images from theory.h
make test   # Compare CV output vs theory gold
```

**Current Results:** 15/17 pass (2 fail due to CV's 8-bit color space quantization)

See `doc/hunt.md` for algorithm documentation.

## Build

```bash
make        # Build lib/vibe.a (17 module objects)
make test   # Run comparison tests
make gold   # Regenerate golden reference images
make info   # Show source counts
make tidy   # Clean
```

## Integration

VIBE is used by LABS as part of the processing pipeline:

```
GEAR → LUTE → VIBE → output
```

Wire into LABS:
```bash
# In wire.sh
WIRE VIBE inc LABS
WIRE VIBE lib LABS
```

## Files

```
VIBE/
├── inc/vibe.hpp           # Public API + type aliases
├── src/main/mods/         # 17 OpenCV module implementations
│   └── mods.h             # Module interface header
├── src/test/
│   ├── test.cpp           # Main test runner
│   ├── diff.h             # Test infrastructure
│   ├── theory.h           # Pure math reference implementations
│   ├── mods/*.cpp         # 17 individual test files
│   └── gold/*.png         # Reference images from theory
├── doc/hunt.md            # Algorithm research + references
├── lib/vibe.a             # Built library
└── Makefile
```
