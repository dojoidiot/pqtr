# PQTR:LUTE

[back](../../README.md)

Camera profile module for PQTR. LUTE learns and applies camera-specific color transforms - the "gear manufacturer's style" that photographers see on their LCD when composing shots.

## Status

**Implementation pending WGPU port.**

The LUTE module API is defined in `inc/lute.hpp`. Implementation will use WGPU (WebGPU) for GPU-accelerated processing.

## Role in PQTR

LUTE learns camera profiles from RAW + embedded JPEG pairs:

1. GEAR decodes RAW to Bayer data
2. LUTE accumulates RGB->RGB mappings into LUTs
3. Profile converges across multiple images

### Transforms (learned from camera behavior)

| Transform | Size | Purpose |
|-----------|------|---------|
| BaseCurve | 768 floats | Per-channel tone response |
| PolyColor | 30 floats | Polynomial color mapping |
| LutCurve | 14,739 floats | 17^3 3D LUT (full color mapping) |
| HsvLut | 1,296 floats | HSV delta corrections |

Camera profiles are stored per camera model and creative style:

- `Sony_ILCE-7M4_Standard.lute`
- `Canon_EOS-R5_Faithful.lute`
- `Nikon_Z8_Vivid.lute`

## Project Structure

```
LUTE/
+-- inc/
|   +-- lute.hpp              # Public API (stub)
+-- src/
|   +-- main/
|   |   +-- dawn/             # DAWN GPU shaders
|   +-- test/
|       +-- dawn/             # DAWN GPU tests
+-- README.md
```

## API

```cpp
namespace lute {
    class Lute {
    public:
        virtual Name key() const = 0;
        virtual pipe::Data view(pipe::Data in) = 0;
        virtual bool tune(pipe::Data flat, pipe::Data preview) = 0;
        virtual bool save(const Name& path) const = 0;
        virtual bool load(const Name& path) = 0;
    };

    Hold create();
    Hold create(const Name& cameraModel,
                const Name& creativeStyle,
                const Name& dro);
}
```

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
pipe->link(gear::read());    // raw -> Bayer
pipe->link(wgpu::open());    // Bayer -> GPU
pipe->link(lute::tune());    // <- learn profile
pipe->link(vibe::tune());    // learn style
pipe->link(wgpu::shut());    // GPU -> output
```

**view pipe** (production):
```cpp
pipe->link(gear::read());    // raw -> Bayer
pipe->link(wgpu::open());    // Bayer -> GPU
pipe->link(lute::view());    // <- apply profile
pipe->link(vibe::view());    // apply style
pipe->link(wgpu::shut());    // GPU -> output
```

See [PIPE](pipe.md) for the full pipeline model.

## Profile Storage

```
~/.pqtr/var/profiles/
+-- Sony_ILCE-7M4_Standard.json
+-- Sony_ILCE-7M4_Vivid.json
+-- Canon_EOS-R5_Faithful.json
+-- ...
```

## GPU Compute

Processing implemented via WGPU (WebGPU) shaders in WGSL. See [WGPU](wgpu.md) for the GPU compute model.

## Dependencies

- **PIPE headers**: `../PIPE/inc` (pipe.hpp)
- **C++17** compiler
- **GPU compute**: WGPU (WebGPU)
