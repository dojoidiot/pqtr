# LUTE

Camera profile LUT module for PQTR. Learns and applies camera-specific color transforms.

## What It Does

LUTE bridges the gap between scene-linear RAW data and display-referred camera JPEGs by learning 3D LUTs from flat/preview pairs.

**Key insight:** Camera manufacturers apply sophisticated color processing (highlight roll-off, skin tone protection, gamut mapping) that varies by creative style. LUTE captures this per-camera.

## How It Works

### Learning Phase (tune)

1. RAWS decodes RAW file to scene-linear + embedded preview
2. LUTE accumulates RGB→RGB mappings into 17³ grid
3. Multiple images fill gaps (dark shadows, bright highlights, saturated colors)
4. Profile converges when average cell delta < 0.1%

### Application Phase (view)

1. Image arrives in scene-linear
2. LUTE looks up profile by EXIF (camera + creative style + DRO)
3. Applies 3D LUT with trilinear interpolation
4. Output matches camera's color science

## Profile Storage

Profiles stored at `~/.pqtr/var/profiles/`:

```
~/.pqtr/var/profiles/
├── Sony_ILCE-7M4_Standard_DRO-Off.json
├── Sony_ILCE-7M4_Vivid_DRO-Auto.json
├── Canon_EOS-R5_Faithful_DRO-Off.json
└── ...
```

## Usage

```cpp
#include <lute.hpp>

// Create profile manager
auto lute = lute::create();

// Set camera key (loads existing profile if found)
lute->setKey("Sony_ILCE-7M4", "Standard", "Off");

// Apply to image
auto out = lute->view(input);

// Or accumulate from RAW
bool updated = lute->tune(flat, preview);
if (lute->profile()->converged()) {
    lute->save();
}
```

## Build

```bash
make        # Build lib/lute.a
make tidy   # Clean
```

## Integration

LUTE is used by LABS as part of the processing pipeline:

```
RAWS → LUTE → DROP → VIBE → output
```

Wire into LABS:
```bash
# In wire.sh
WIRE LUTE inc LABS
WIRE LUTE lib LABS
```

## Profile Format

JSON with 3D LUT data:

```json
{
  "key": "Sony_ILCE-7M4_Standard_DRO-Off",
  "camera_model": "ILCE-7M4",
  "creative_style": "Standard",
  "dro": "Off",
  "sample_count": 47,
  "coverage": 0.923,
  "converged": true,
  "lut": [...]
}
```
