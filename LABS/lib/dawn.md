# Building Dawn

Dawn is Google's WebGPU implementation. This document describes how to build it for PQTR.

## Prerequisites

Ubuntu/Debian:
```bash
sudo apt-get install libxrandr-dev libxinerama-dev libxcursor-dev \
                     mesa-common-dev libx11-xcb-dev pkg-config python3
```

Also required: CMake 3.16+, Git, C++20 compiler.

## Setup

Dawn is a git submodule. Initialize it:
```bash
git submodule update --init LABS/lib/dawn
```

## Build

Run from LABS/lib directory:
```bash
cd LABS/lib
./dawn.sh
```

The script:
1. Configures CMake with `DAWN_FETCH_DEPENDENCIES=ON` (downloads deps via Python, no depot_tools needed)
2. Builds with all available cores
3. Installs to `dawn/install/`

## Output

After building:
- Headers: `LABS/lib/dawn/install/include/`
- Libraries: `LABS/lib/dawn/install/lib/`

## Linking

In CMake projects:
```cmake
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/LABS/lib/dawn/install")
find_package(Dawn REQUIRED)
target_link_libraries(your_target dawn::webgpu_dawn)
```

## References

- [Dawn quickstart-cmake.md](https://github.com/google/dawn/blob/main/docs/quickstart-cmake.md)
- [Dawn building.md](https://dawn.googlesource.com/dawn/+/HEAD/docs/building.md)
