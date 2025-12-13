# WGPU

WebGPU compute library for PQTR (using Dawn backend).

## What

WGPU provides GPU compute via WebGPU API:
- Native GPU access (Vulkan, Metal, D3D12)
- WASM target for browser deployment
- WGSL shader language

Currently uses Google's Dawn as the backend implementation.

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

WGPU contributes two links that bookend GPU processing:

```cpp
pipe::Hold<pipe::Link> wgpu::open();  // CPU → GPU
pipe::Hold<pipe::Link> wgpu::shut();  // GPU → CPU
```

| Link | Input Page | Output Page | Purpose |
|------|------------|-------------|---------|
| `wgpu::open()` | BayerBuffer* | Context* | Upload to GPU, create device |
| `wgpu::shut()` | Context* | OutputBuffer* | Download from GPU, cleanup |

### Usage in Pipe

```cpp
auto pipe = pipe::make();
pipe->link(gear::link());    // raw → Bayer (CPU)
pipe->link(wgpu::open());    // Bayer → GPU ← opens GPU context
pipe->link(lute::view());    // apply camera profile (GPU)
pipe->link(vibe::view());    // apply style (GPU)
pipe->link(wgpu::shut());    // GPU → output (CPU) ← closes GPU context
```

The `Context` struct carries the GPU device and buffer through intermediate links:

```cpp
struct Context {
    wgpu::Device device;
    wgpu::Buffer buffer;
    int width, height, channels;
};
```

See [PIPE](../PIPE/README.md) for the full pipeline model.

---

## Direct API

```cpp
#include <wgpu.hpp>

auto inst = dawn::instance();
auto adapt = dawn::adapter(inst);
auto dev = dawn::device(adapt);
auto pipe = dawn::pipeline(dev, wgslShaderCode);
```

## Include

```makefile
INCLUDES = -I./inc/WGPU -I./lib/WGPU/lib/dawn/include
LIBS = ./lib/WGPU/wgpu.a
```
