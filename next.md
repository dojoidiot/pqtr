# Next Steps

## Current State (2025-12-12)

### Recent Changes
- **WGPU**: Renamed from DAWN (WebGPU compute library)
- **BASE**: Renamed from JWTA, now serves static site + JWT auth
- **TUNE**: Separated from LABS as standalone optimizer
- **APEX/SITE**: Removed (unused)

### WGPU Shaders - COMPLETE
- **VIBE**: 16 WGSL compute shaders (all mods except geometric)
- **RAWS**: 5 WGSL compute shaders (BLC, WB, demosaic, color matrix)
- All tests passing: VIBE 16/16, RAWS 5/5

## Architecture

```
RAWS → VIBE → TUNE
  │      │      │
  │      │      └── Style optimizer (GeoS algorithm)
  │      └── 17 image processing modules
  └── RAW decoder (camera-specific)
```

### Projects

| Project | Purpose |
|---------|---------|
| BASE | Web server + JWT auth + static site |
| DESK | Desktop GUI (ImGui) |
| LABS | Core pipeline orchestrator |
| RAWS | RAW file decoding |
| TUNE | Style optimization |
| VIBE | Image processing modules |
| WGPU | WebGPU compute (Dawn backend) |

## What's Working

- ✅ 17 VIBE modules (CV backend)
- ✅ 16 VIBE WGSL shaders (GPU backend)
- ✅ 5 RAWS WGSL shaders (GPU backend)
- ✅ TUNE separated from LABS
- ✅ BASE web server with static site serving
- ✅ Theory-based testing (CPU reference vs GPU)

## Next

- [ ] Wire VIBE into LABS pipeline
- [ ] Implement Vibe orchestrator class
- [ ] GPU pipeline integration (WGPU)
- [ ] DESK UI for vibe creation

## Build & Test

```bash
./wire.sh && make

# WGPU shader tests
cd VIBE && make test-dawn
cd RAWS && make test-dawn

# TUNE optimizer
cd TUNE && ./bin/tune --help

# BASE server
cd BASE && make && ./bin/base --help
```
