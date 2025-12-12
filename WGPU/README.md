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

## Usage

```cpp
#include <wgpu.hpp>

auto inst = wgpu::instance();
auto adapt = wgpu::adapter(inst);
auto dev = wgpu::device(adapt);
auto pipe = wgpu::pipeline(dev, wgslShaderCode);
```

## Include

```makefile
INCLUDES = -I./inc/WGPU -I./lib/WGPU/lib/dawn/include
LIBS = ./lib/WGPU/wgpu.a
```
