# WGPU

WebGPU compute library for PQTR (using Dawn backend).

## What

WGPU provides GPU compute via WebGPU API:
- Native GPU access (Vulkan, Metal, D3D12)
- WASM target for browser deployment
- WGSL shader language

Currently uses Google's Dawn as the backend implementation.

## The Fidelity Rule

**WGPU enforces: No fidelity loss until POST.**

All GPU buffers maintain float32 precision throughout the pipeline. Only `wgpu::post()` quantizes to 8-bit for final output.

| Link | Precision In | Precision Out | Purpose |
|------|--------------|---------------|---------|
| `wgpu::open()` | uint16 (Bayer) | float32 | Normalize to [0,1] float |
| `wgpu::view()` | float32 | float32 + uint8 display | Preview (display only, not saved) |
| `wgpu::post()` | float32 | uint8 PNG | Final output (fidelity loss OK) |
| `wgpu::shut()` | float32 | float32 | Download from GPU |

## Prerequisites

```bash
# Linux
sudo apt install cmake ninja-build python3 libvulkan-dev

# depot_tools (for gclient)
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:/path/to/depot_tools
```

## Build

```bash
make deps   # Fetch Dawn dependencies (once)
make        # Build lib/dawn.a
make tidy   # Clean
```

## Pipe Link Contributions

WGPU contributes four links for GPU lifecycle and output:

```cpp
pipe::Hold<pipe::Link> wgpu::open();  // CPU → GPU (start)
pipe::Hold<pipe::Link> wgpu::view();  // GPU → display (preview, no save)
pipe::Hold<pipe::Link> wgpu::post();  // GPU → PNG (final output)
pipe::Hold<pipe::Link> wgpu::shut();  // GPU → CPU (end)
```

| Link | Input Page | Output Page | Fidelity | Purpose |
|------|------------|-------------|----------|---------|
| `wgpu::open()` | BayerBuffer* | Context* | Lossless | Upload to GPU, normalize to float32 |
| `wgpu::view()` | Context* | Context* | Lossless | Generate display texture (8-bit temp, not saved) |
| `wgpu::post()` | Context* | PngBuffer* | **Lossy** | Encode to PNG (8-bit, final output) |
| `wgpu::shut()` | Context* | OutputBuffer* | Lossless | Download float32 from GPU |

### wgpu::view() - Display Preview

For interactive tuning. Quantizes float32 → uint8 **for display only**:
- Input: float32 GPU buffer (full precision)
- Output: Same float32 buffer (unchanged) + OpenGL texture for display
- **Does not save** - display texture is temporary
- Maintains pipeline fidelity

### wgpu::post() - Final Output

For saving results. Encodes to PNG via WGSL compute shader:
- Input: float32 GPU buffer
- Output: PNG byte buffer (uint8, lossless compression)
- **This is the only place fidelity loss occurs**
- PNG encoding runs entirely on GPU

### Usage in Pipe

```cpp
// Tuning pipeline (interactive)
auto tune = pipe::make();
tune->link(gear::read());    // raw → Bayer (CPU)
tune->link(wgpu::open());    // Bayer → GPU float32
tune->link(lute::tune());    // learn profile (GPU)
tune->link(wgpu::view());    // display preview (no save)
tune->link(wgpu::shut());    // GPU → CPU

// Production pipeline (final output)
auto prod = pipe::make();
prod->link(gear::read());    // raw → Bayer (CPU)
prod->link(wgpu::open());    // Bayer → GPU float32
prod->link(lute::view());    // apply profile (GPU)
prod->link(vibe::view());    // apply style (GPU)
prod->link(wgpu::post());    // GPU → PNG (8-bit output)
prod->link(wgpu::shut());    // cleanup
```

### Context Structure

The `Context` struct carries GPU state through the pipeline:

```cpp
struct Context {
    wgpu::Device device;
    wgpu::Buffer buffer;      // float32 RGBA
    wgpu::Texture display;    // uint8 for view (optional)
    int width, height;
};
```

See [PIPE](pipe.md) for the full pipeline model and fidelity rule.

## PNG Encoding (wgpu::post)

The `post.wgsl` compute shader encodes float32 RGBA to PNG entirely on GPU.

### PNG Format

```
┌─────────────────────────────────────────────────────────┐
│ Signature: 89 50 4E 47 0D 0A 1A 0A  (8 bytes)          │
├─────────────────────────────────────────────────────────┤
│ IHDR chunk: width, height, bit depth, color type       │
│   Length(4) + "IHDR"(4) + Data(13) + CRC(4) = 25 bytes │
├─────────────────────────────────────────────────────────┤
│ IDAT chunk(s): filtered + compressed pixel data        │
│   Length(4) + "IDAT"(4) + Data(N) + CRC(4)             │
├─────────────────────────────────────────────────────────┤
│ IEND chunk: empty end marker                           │
│   Length(4) + "IEND"(4) + CRC(4) = 12 bytes            │
└─────────────────────────────────────────────────────────┘
```

### GPU Encoding Strategy

PNG encoding has two phases:

**Phase 1: Row Filtering** (parallel, one workgroup per row)
- Each row chooses optimal filter (None, Sub, Up, Average, Paeth)
- Filter predicts pixel from neighbors, stores difference
- Reduces entropy for better compression

**Phase 2: DEFLATE** (simplified, GPU-friendly)
- Use "stored" blocks (no compression) for simplicity
- Or fixed Huffman tables (no adaptive coding)
- Full DEFLATE possible but complex for GPU

### Implementation Approach

Start simple, optimize later:

1. **v1: Uncompressed PNG** - Valid PNG with stored DEFLATE blocks
   - Filter: None (0x00 prefix per row)
   - Compression: Store only (no LZ77)
   - Larger files but fast and correct

2. **v2: Filtered PNG** - Add row filtering
   - Parallel filter selection per row
   - Still stored blocks
   - ~30% smaller

3. **v3: Compressed PNG** - Add fixed Huffman
   - Fixed tables (no per-image adaptation)
   - ~60% smaller

### WGSL Shader Structure

```wgsl
// post.wgsl - PNG encoder compute shader

@group(0) @binding(0) var<storage, read> pixels: array<vec4f>;
@group(0) @binding(1) var<storage, read_write> png: array<u32>;
@group(0) @binding(2) var<uniform> params: PngParams;

struct PngParams {
    width: u32,
    height: u32,
    // ...
}

// CRC32 lookup table (precomputed)
var<private> crc_table: array<u32, 256>;

@compute @workgroup_size(256)
fn encode_rows(@builtin(global_invocation_id) gid: vec3u) {
    let row = gid.x;
    if (row >= params.height) { return; }

    // Quantize float32 → uint8
    // Apply filter
    // Write to output buffer
}

@compute @workgroup_size(1)
fn write_header() {
    // PNG signature + IHDR chunk
}

@compute @workgroup_size(1)
fn write_footer() {
    // IEND chunk
}
```

### Output Buffer Layout

```
offset 0:     PNG signature (8 bytes)
offset 8:     IHDR chunk (25 bytes)
offset 33:    IDAT chunk header (8 bytes)
offset 41:    zlib header (2 bytes)
offset 43:    DEFLATE blocks (filtered rows)
offset N:     zlib adler32 (4 bytes)
offset N+4:   IDAT CRC (4 bytes)
offset N+8:   IEND chunk (12 bytes)
```

---

## Direct API

```cpp
#include <dawn/webgpu_cpp.h>

// Helper functions in wgpu.cpp
auto inst = dawn::instance();
auto adapt = dawn::adapter(inst);
auto dev = dawn::device(adapt);
auto pipe = dawn::pipeline(dev, wgslShaderCode);
```

## Structure

```
WGPU/
├── src/main/wgpu/wgpu.cpp # Dawn helper functions
├── src/
│   ├── main/
│   │   └── link.cpp       # Pipe link implementations
│   ├── shaders/
│   │   ├── view.wgsl      # Display preview (uint8 temp, no save)
│   │   └── post.wgsl      # PNG encoder (uint8 final output)
│   └── test/
├── lib/dawn/              # Dawn WebGPU backend (submodule)
└── README.md
```

## Include

```makefile
INCLUDES = -I./inc/WGPU -I./lib/WGPU/lib/dawn/include
LIBS = ./lib/WGPU/wgpu.a
```
