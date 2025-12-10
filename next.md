# Next Steps

## Current State (2025-12-10)

### Modular Pipe Refactor - PHASE 1 COMPLETE

Project structures created for modular pipeline:

**Done:**
- `pipe::Info` - Tree-structured metadata (nodes + leaves)
- `pipe::Data` - Concrete `View` + `Info` bundle
- `pipe::Task` - Universal interface with `view()` and `tune()`
- `pipe::Pipe` - Chain runner with `add()`, `view()`, `tune()`
- **DAWN** - WebGPU via Google Dawn (lib/dawn.a built)
- **LUTE** - Camera profile LUT module (structure created)
- **VIBE** - Style processing module (45 dials interface)

**Next:** Implement LUTE and VIBE internals, wire into LABS.

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
| **VIBE** | apply 45 dials | optimize 45 dials | `.vibe.json` |

### Module Responsibilities

| Module | Purpose | Input | Output |
|--------|---------|-------|--------|
| **RAWS** | RAW decode | RAW file | flat scene-linear |
| **LUTE** | Camera profile | flat + preview (same file) | profile LUT |
| **VIBE** | Style matching | any two images | 45 dials |

### Key Principles

1. **RAWS** extracts flat + embedded preview from same RAW file
2. **LUTE** only learns from RAWS output (paired flat/preview)
3. **VIBE** is camera-agnostic (any source/reference pair)
4. Modules are independent - LABS just runs the pipe

---

## Implementation Plan

### Phase 1: Module Structures ✅
- [x] Create `LUTE/` project structure
- [x] Create `VIBE/` project structure
- [x] Define `lute.hpp` API
- [x] Define `vibe.hpp` API (45 dials)
- [x] Build DAWN WebGPU library

### Phase 2: LUTE Implementation
- [ ] Move CameraLut from RAWS to LUTE
- [ ] Implement Profile class (accumulator)
- [ ] Implement Lute class (manager + Task)
- [ ] LUTE.view(): apply profile LUT
- [ ] LUTE.tune(): accumulate profile LUT
- [ ] Wire LUTE into LABS

### Phase 3: VIBE Implementation
- [ ] Move 45 dials + optimizer from LABS/Body
- [ ] Implement Vibe class with all modules
- [ ] VIBE.view(): apply dials
- [ ] VIBE.tune(): optimize dials (GeoS)
- [ ] Wire VIBE into LABS

### Phase 4: Clean up LABS
- [ ] LABS becomes thin orchestrator
- [ ] view mode: RAWS → LUTE → VIBE → PNG
- [ ] tune mode: same + loss + diff output

### Phase 5: GPU Acceleration (DAWN)
- [ ] Move color transforms to WGSL shaders
- [ ] Benchmark vs OpenCV UMat

---

## What's Working (from previous impl)

- ✅ CameraLut accumulator with convergence tracking
- ✅ Profile save/load to JSON
- ✅ Key generation from EXIF (camera + style)
- ✅ 45 style dials
- ✅ GeoS optimizer

## What's Blocked

- ⏳ Profile LUT not wired to pipeline (will fix in LUTE module)
- ⏳ HSV LUT disabled (73° hue shifts)
- ⏳ DROP module deferred (DRO handling)

---

## Build & Test

```bash
./wire.sh && make

cd LABS
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune var/pics/DSC00144.ARW preview
```
