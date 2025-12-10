# Next Steps

## Current State (2025-12-10)

### Modular Pipe Refactor - PHASE 0 COMPLETE

New Task-based pipe API added alongside legacy HEAD→BODY→TAIL.

**Done:**
- `pipe::Info` - Tree-structured metadata (nodes + leaves)
- `pipe::Data` - Concrete `View` + `Info` bundle
- `pipe::Task` - Universal interface with `view()` and `tune()`
- `pipe::Pipe` - Chain runner with `add()`, `view()`, `tune()`
- Legacy API preserved (`LegacyData`, `Head`, `Body`, `Tail`)

**Next:** Extract LUTE module using new Task interface.

## Architecture

**LABS** = orchestrator with two pipe modes:
- `view` - render image
- `tune` - optimize + render + diff

**Modules contribute to both pipes:**

| Module | View | Tune | State |
|--------|------|------|-------|
| **RAWS** | decode flat | decode flat | - |
| **LUTE** | apply profile LUT | accumulate profile LUT | `~/.pqtr/var/profiles/*.json` |
| **DROP** | apply DRO curves | learn DRO curves | `~/.pqtr/var/dro/*.json` |
| **VIBE** | apply 45 dials | optimize 45 dials | `.pipe.json` |

```
LABS view: RAWS.view → LUTE.view → DROP.view → VIBE.view → PNG
LABS tune: RAWS.tune → LUTE.tune → DROP.tune → VIBE.tune → PNG + DIFF + loss
```

### Module Responsibilities

| Module | Purpose | Input | Output |
|--------|---------|-------|--------|
| **RAWS** | RAW decode | RAW file | flat scene-linear |
| **LUTE** | Camera profile | flat + preview (same file) | profile LUT |
| **DROP** | DRO correction | RAW pairs with varying DRO | DRO curves |
| **VIBE** | Style matching | any two images | 45 dials |

### Key Principles

1. **RAWS** extracts flat + embedded preview from same RAW file
2. **LUTE** only learns from RAWS output (paired flat/preview)
3. **VIBE** is camera-agnostic (any source/reference pair)
4. **DROP** deferred (handles 11% DRO error floor)
5. Modules are independent - LABS just runs the pipe

---

## Implementation Plan

### Phase 1: Extract LUTE module
- [ ] Create `LUTE/` project structure
- [ ] Move CameraLut from RAWS to LUTE
- [ ] LUTE.view(): apply profile LUT
- [ ] LUTE.tune(): accumulate profile LUT
- [ ] Wire LUTE into LABS

### Phase 2: Extract VIBE module
- [ ] Create `VIBE/` project structure
- [ ] Move 45 dials + optimizer from LABS
- [ ] VIBE.view(): apply dials
- [ ] VIBE.tune(): optimize dials
- [ ] Wire VIBE into LABS

### Phase 3: Clean up LABS
- [ ] LABS becomes thin orchestrator
- [ ] view mode: RAWS → LUTE → VIBE → PNG
- [ ] tune mode: same + loss + diff output

### Phase 4: DROP (deferred)
- [ ] DRO characterization research
- [ ] DROP.tune(): learn DRO curves
- [ ] DROP.view(): apply DRO correction

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
- ⏳ DRO handling (deferred to DROP)

---

## Build & Test

```bash
./wire.sh && make

cd LABS
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune var/pics/DSC00144.ARW preview
```
