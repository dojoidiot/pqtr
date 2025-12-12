# Next Steps

## Current State (2025-12-12)

### Architecture Clarified

Two-phase processing that mirrors professional workflow:

1. **LUTE** - Camera profile (gear manufacturer's style)
   - BaseCurve, PolyColor, LutCurve, HsvLut
   - Learned from RAW + embedded preview pairs
   - Fixed per camera model + creative style

2. **VIBE** - Creative style (photographer's adjustments)
   - 51 dials across 7 modules
   - Optimized by TUNE to match reference
   - Variable per photographer/vibe

### Recent Changes
- Moved BaseCurve, PolyColor, LutCurve, HsvLut from VIBE to LUTE
- Moved corresponding WGSL shaders to LUTE
- Rewrote README.md with complete architecture
- WGPU: Renamed from DAWN
- BASE: Renamed from JWTA

## Projects

| Project | Status | Purpose |
|---------|--------|---------|
| GEAR | Active | RAW decoder |
| LUTE | Active | Camera profiles |
| VIBE | Active | Creative styles (51 dials) |
| TUNE | Active | Style optimizer |
| PIPE | Active | Pipeline orchestrator |
| WGPU | Active | GPU compute |
| DESK | In Dev | Desktop GUI |
| BASE | Active | Web server |

## What's Working

- VIBE: 13 WGSL shaders (creative dials)
- LUTE: 4 WGSL shaders (camera transforms)
- GEAR: 5 WGSL shaders (RAW decode)
- TUNE: Style optimizer (GeoS algorithm)

## Next

- [ ] Wire LUTE into PIPE pipeline
- [ ] Wire VIBE into PIPE pipeline
- [ ] DESK UI for vibe creation
- [ ] Profile convergence testing

## Build & Test

```bash
./wire.sh && make

# GPU shader tests
cd VIBE && make test-dawn
cd LUTE && make test-dawn
cd GEAR && make test-dawn
```
