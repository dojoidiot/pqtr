# PQTR:VIBE

[back](../../README.md)

Creative style module for PQTR. VIBE handles the photographer's creative adjustments - the dials they twist in Lightroom/Darktable to express their style.

## Status

**Implementation pending WGPU port.**

The VIBE module API is defined in `inc/vibe.hpp`. Implementation will use WGPU (WebGPU) for GPU-accelerated processing.

## Role in PQTR

VIBE is where the creative magic happens. It provides 51 adjustable dials organized into modules:

- **Geometric** (6 dials): crop, zoom, rotation
- **ColorCorrection** (3 dials): exposure, white balance
- **ToneMapping** (7 dials): contrast, highlights, shadows, pivots, clips
- **GlobalColor** (3 dials): vibrance, saturation, density
- **SplitTone** (4 dials): shadow/highlight color grading
- **SelectiveColour** (24 dials): per-hue HSL adjustments
- **Detail** (4 dials): sharpen, denoise

## Project Structure

```
VIBE/
+-- inc/
|   +-- vibe.hpp              # Public API (stub)
+-- src/
|   +-- main/
|   |   +-- dawn/             # DAWN GPU shaders
|   +-- test/
|       +-- dawn/             # DAWN GPU tests
+-- README.md
```

## API

```cpp
namespace vibe {
    class Vibe {
    public:
        virtual Name name() const = 0;
        virtual pipe::Data view(pipe::Data in) = 0;
        virtual pipe::Data tune(pipe::Data in, pipe::Data reference) = 0;
        virtual bool save(const Name& path) = 0;
        virtual bool load(const Name& path) = 0;
    };

    std::unique_ptr<Vibe> create();
    std::unique_ptr<Vibe> create(const Name& path);
}
```

## Pipe Link Contributions

VIBE contributes two links:

```cpp
pipe::Hold<pipe::Link> vibe::tune();  // Learn style
pipe::Hold<pipe::Link> vibe::view();  // Apply style
```

| Link | Input Page | Output Page | Purpose |
|------|------------|-------------|---------|
| `vibe::tune()` | Context* | Context* | Optimize dials to match reference |
| `vibe::view()` | Context* | Context* | Apply learned style transforms |

### Pipe Configurations

**tune pipe** (learning):
```cpp
pipe->link(gear::read());    // raw -> Bayer
pipe->link(wgpu::open());    // Bayer -> GPU
pipe->link(lute::tune());    // learn profile
pipe->link(vibe::tune());    // <- learn style
pipe->link(wgpu::shut());    // GPU -> output
```

**view pipe** (production):
```cpp
pipe->link(gear::read());    // raw -> Bayer
pipe->link(wgpu::open());    // Bayer -> GPU
pipe->link(lute::view());    // apply profile
pipe->link(vibe::view());    // <- apply style
pipe->link(wgpu::shut());    // GPU -> output
```

See [PIPE](pipe.md) for the full pipeline model.

## GPU Compute

Processing implemented via WGPU (WebGPU) shaders in WGSL. See [WGPU](wgpu.md) for the GPU compute model.

## Dependencies

- **PIPE headers**: `../PIPE/inc` (pipe.hpp)
- **C++17** compiler
- **GPU compute**: WGPU (WebGPU)
