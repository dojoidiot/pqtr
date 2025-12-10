# VIBE

Style processing module for PQTR. Handles the 45 style dials plus non-dial transforms (LUTs, curves).

## What It Does

VIBE transforms images to match a target style. Given a source image and reference, it:
1. Optimizes 45 dials to minimize perceptual difference (tune mode)
2. Applies the learned style to new images (view mode)

## Architecture

### 45 Style Dials

| Module | Dials | Color Space | Purpose |
|--------|-------|-------------|---------|
| Geometric | 6 | SPATIAL | Crop, zoom, rotation |
| ColorCorrection | 3 | LINEAR_RGB | Exposure, white balance |
| ToneMapping | 7 | LINEAR_RGB | Contrast, highlights, shadows, pivots, clips |
| GlobalColor | 3 | LCH | Vibrance, saturation, density |
| SplitTone | 4 | LINEAR_RGB | Shadow/highlight color grading |
| SelectiveColour | 24 | LCH | Per-hue H/S/L adjustments |
| Detail | 4 | LINEAR_RGB/LCH | Sharpen, denoise |

### Non-Dial Transforms

| Transform | Size | Purpose |
|-----------|------|---------|
| BaseCurve | 768 floats | Camera response curve from RAWS |
| PolyColor | 30 floats | Quadratic polynomial RGB→RGB |
| LutCurve | 14,739 floats | 17³ 3D LUT |
| HsvLut | 1,296 floats | 36×12 HSV delta LUT |

## Usage

```cpp
#include <vibe.hpp>

// Create and configure
auto style = vibe::create();
style->toneMapping().contrast().set(1.2f);
style->globalColor().vibrance().set(0.8f);

// Apply to image
auto out = style->view(input);

// Or tune from reference
auto out = style->tune(input, reference);

// Save/load
style->save("mystyle.vibe.json");
auto loaded = vibe::create("mystyle.vibe.json");
```

## Build

```bash
make        # Build lib/vibe.a
make tidy   # Clean
```

## Integration

VIBE is used by LABS as part of the processing pipeline:

```
RAWS → LUTE → DROP → VIBE → output
```

Wire into LABS:
```bash
# In wire.sh
WIRE VIBE inc LABS
WIRE VIBE lib LABS
```
