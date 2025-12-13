# LUTE

Camera Profile Module - learns and applies gear manufacturer's color science.

## What It Does

When photographers compose shots, they see the camera's interpretation of the scene on their LCD. LUTE captures this "out of camera" look by learning from RAW + embedded preview pairs.

## Camera Transforms

LUTE manages four transform types, all learned from camera behavior:

| Transform | Size | Purpose |
|-----------|------|---------|
| **BaseCurve** | 768 floats | Camera tone response curve |
| **PolyColor** | 30 floats | Polynomial RGB→RGB transform |
| **LutCurve** | 14,739 floats | 17³ 3D LUT for full color mapping |
| **HsvLut** | 1,296 floats | 36×12 HSV delta corrections |

## Learning Process

```
1. GEAR decodes RAW → scene-linear RGB
2. Extract embedded preview JPEG
3. LUTE compares flat vs preview
4. Accumulate RGB→RGB mappings into transforms
5. Repeat with more images until converged
```

Profiles are keyed by camera model + creative style:
- `Sony_ILCE-7M4_Standard.json`
- `Canon_EOS-R5_Faithful.json`

## Pipe Link Contributions

LUTE contributes two links:

```cpp
pipe::Hold<pipe::Link> lute::tune();  // Learn profile
pipe::Hold<pipe::Link> lute::view();  // Apply profile
```

| Link | Input Page | Output Page | Purpose |
|------|------------|-------------|---------|
| `lute::tune()` | Context* | Context* | Learn camera profile from RAW+preview |
| `lute::view()` | Context* | Context* | Apply learned profile |

### Pipe Configurations

**tune pipe** (learning):
```cpp
pipe->link(gear::read());    // raw → Bayer
pipe->link(wgpu::open());    // Bayer → GPU
pipe->link(lute::tune());    // ← learn profile
pipe->link(vibe::tune());    // learn style
pipe->link(wgpu::shut());    // GPU → output
```

**view pipe** (production):
```cpp
pipe->link(gear::read());    // raw → Bayer
pipe->link(wgpu::open());    // Bayer → GPU
pipe->link(lute::view());    // ← apply profile
pipe->link(vibe::view());    // apply style
pipe->link(wgpu::shut());    // GPU → output
```

See [PIPE](../PIPE/README.md) for the full pipeline model.

---

## Direct API

```cpp
#include <lute.hpp>

auto lute = lute::create();

// Set camera key (loads existing profile if found)
lute->setKey("Sony", "ILCE-7M4", "Standard");

// Learn from RAW+preview pair
lute->tune(flat_image, preview_image);

// Apply to new images
auto output = lute->view(input);

// Save when converged
if (lute->profile()->converged()) {
    lute->save();
}
```

## Profile Storage

```
~/.pqtr/var/profiles/
├── Sony_ILCE-7M4_Standard.json
├── Sony_ILCE-7M4_Vivid.json
├── Canon_EOS-R5_Faithful.json
└── ...
```

## Build

```bash
make        # Build lib/lute.a
make test   # Run tests
make tidy   # Clean
```
