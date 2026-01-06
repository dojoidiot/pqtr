# Pipeline Porting State - Jan 6, 2026

## Overview
The C99 pipeline from `args-project` is being ported to idiomatic C++17 in `copy`. High-precision binary dumping has been implemented in both to verify bit-exactness at every stage.

## Build Configuration
- Both projects are using `-O2` optimization.
- OpenMP has been temporarily disabled in `copy/src/modules/demosaic.cpp` to ensure deterministic tile processing for comparison.
- C-style math functions (`powf`, `exp2f`, etc.) are being used in `copy` to match C99 `math.h` behavior.

## Comparison Progress

| Step | Module | Status | Notes |
| :--- | :--- | :--- | :--- |
| 00 | **Decode** | **MATCH** | Exact binary copy. |
| 01 | **Rawprepare** | **MATCH** | Exact binary copy. |
| 02 | **Temperature** | **MATCH** | Exact binary copy. |
| 03 | **Highlights** | **MATCH** | Exact binary copy. |
| 04 | **Demosaic** | **MATCH** | Match achieved single-threaded. |
| 05 | **Exposure** | **MATCH** | Exact binary copy. |
| 06 | **Colorin** | **MATCH** | Exact binary copy. |
| 07 | **ChannelMixer** | **MATCH** | Exact binary copy. |
| 08 | **Denoise** | **MATCH** | Exact binary copy. |
| 09 | **ColorBalance** | **MATCH** | Exact binary copy. |
| 10 | **FilmicRGB** | **MATCH** | Bit-exactness achieved. |
| 11 | **Bilat** | **MATCH** | Bit-exactness achieved multi-threaded. |
| 12 | **Colorout** | **MATCH** | Bit-exactness achieved multi-threaded. |

## Multi-threading and Determinism
- **Resolved**: Re-enabled OpenMP in `demosaic.cpp`, `bilat.cpp`, `denoiseprofile.cpp`, `colorbalancergb.cpp`, `filmicrgb.cpp` (process), `exposure.cpp`, `colorin.cpp`, and `temperature.cpp`.
- **Bug Fixed (Demosaic)**: Discovered that intermediate buffers (`rgb_buf`, `PQ_Dir`, etc.) were being reused across tiles without being fully zeroed, leading to thread-count-dependent results. Fixed by explicitly zeroing all tile buffers at the start of each tile in both C++ and C99 implementations.
- **Precision Note**: `highlights` Pass 3 (chrominance reduction) remains single-threaded to maintain bit-exactness with the reference implementation, as parallel floating-point reduction is non-deterministic due to associativity.
- **Optimization Note**: Stuck with `-O2` optimization and avoided `-march=native` to ensure bit-exact results, as higher levels introduced subtle FMA differences.

## Next Steps
1. Performance profiling to identify further bottlenecks.
2. Consider implementing bit-exact manual SIMD if further speedup is needed.
3. Clean up temporary debug dump files.

