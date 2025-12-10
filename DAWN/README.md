# DAWN

WebGPU implementation for GPU compute in PQTR.

## What

Dawn is Google's WebGPU implementation. It provides:
- Native GPU access (Vulkan, Metal, D3D12)
- WASM target for browser deployment
- WGSL shader language

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
#include <dawn.hpp>

auto inst = dawn::instance();
auto adapt = dawn::adapter(inst);
auto dev = dawn::device(adapt);
auto pipe = dawn::pipeline(dev, wgslShaderCode);
```

## Include

```makefile
INCLUDES = -I./inc/DAWN -I./lib/DAWN/lib/dawn/include
LIBS = ./lib/DAWN/dawn.a
```
