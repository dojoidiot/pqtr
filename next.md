# Next Steps

## Current State (2025-12-10)

### VIBE Module - PHASE 3 IN PROGRESS

**Done:**
- 17 image transform modules ported to VIBE with type aliases (`View`, `Dial`, `Grid`)
- Theory-based test harness: `theory.h` (pure math) vs CV implementation
- **15/17 gold tests pass** - theory matches CV where floating-point allows
- **17/17 CV tests pass** - VIBE = LABS (IDENTICAL output)
- Algorithm documentation in `VIBE/doc/hunt.md`

**2 Gold Failures (expected):**
- `global_color` (14.7%) - CV uses 8-bit Lab conversion
- `selective_color` (16.6%) - CV uses 8-bit HLS conversion
- These are inherent CV quantization artifacts, not algorithm errors

**Next:** Wire VIBE into LABS pipeline, implement Vibe orchestrator class.

## Architecture

**LABS** = orchestrator with modular pipe:

```
RAWS.view → LUTE.view → VIBE.view → PNG
```

**Modules contribute Tasks to both pipes:**

| Module | View | Tune | State |
|--------|------|------|-------|
| **RAWS** | decode flat | decode flat | - |
| **LUTE** | apply profile LUT | accumulate profile LUT | `~/.pqtr/var/profiles/*.json` |
| **VIBE** | apply 17 mods | optimize dials | `.vibe.json` |

### VIBE Module Architecture

| Module | Dials | Purpose |
|--------|-------|---------|
| exposure | 1 | EV adjustment |
| white_balance | 2 | Temperature + tint |
| tone_map | 7 | Contrast, shadows, highlights |
| global_color | 3 | Vibrance, saturation, density |
| geometric | 6 | Crop, zoom, rotation |
| selective_color | 24 | 8-band HSL |
| split_tone | 4 | Shadow/highlight grading |
| detail | 4 | Sharpen, denoise |
| + 9 meta mods | - | LUTs, curves, matrices |

---

## Implementation Plan

### Phase 1: Module Structures ✅
- [x] Create `LUTE/` project structure
- [x] Create `VIBE/` project structure
- [x] Define `lute.hpp` API
- [x] Define `vibe.hpp` API
- [x] Build DAWN WebGPU library

### Phase 2: LUTE Implementation
- [ ] Move CameraLut from RAWS to LUTE
- [ ] Implement Profile class (accumulator)
- [ ] Implement Lute class (manager + Task)
- [ ] LUTE.view(): apply profile LUT
- [ ] LUTE.tune(): accumulate profile LUT
- [ ] Wire LUTE into LABS

### Phase 3: VIBE Implementation ⬅️ CURRENT
- [x] Port 17 mods from LABS to VIBE
- [x] Create theory-based test harness
- [x] Validate 15/17 mods against theory gold
- [x] Document algorithms in hunt.md
- [ ] Implement Vibe orchestrator class
- [ ] VIBE.view(): apply all mods in order
- [ ] VIBE.tune(): optimize dials (GeoS from LABS)
- [ ] Wire VIBE into LABS

### Phase 4: Clean up LABS
- [ ] LABS becomes thin orchestrator
- [ ] view mode: RAWS → LUTE → VIBE → PNG
- [ ] tune mode: same + loss + diff output

### Phase 5: GPU Acceleration (DAWN)
- [ ] Port VIBE mods to WGSL shaders
- [ ] PIMPL backend switching (CV vs DAWN)
- [ ] Benchmark vs OpenCV UMat

---

## What's Working

- ✅ 17 VIBE modules (CV backend)
- ✅ Theory-based gold testing (15/17 pass)
- ✅ CameraLut accumulator with convergence tracking
- ✅ Profile save/load to JSON
- ✅ Key generation from EXIF (camera + style)
- ✅ GeoS optimizer (in LABS)

## What's Blocked

- ⏳ VIBE not wired to LABS pipeline yet
- ⏳ HSV LUT disabled (73° hue shifts)
- ⏳ DROP module deferred (DRO handling)

---

## Build & Test

```bash
./wire.sh && make

# Test VIBE
cd VIBE && make test

# Test LABS
cd LABS
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune var/pics/DSC00144.ARW preview
```
