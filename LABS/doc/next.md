# Next Steps

## Context

Read this file after `README.md` to understand current LABS development state.

**BASE** = 633c1aa (2025-12-05)

---

## Current Direction: VIBE Neural Dial Prediction

**Goal:** Replace slow optimizer with instant MLP prediction.

### Status: Trained, Testing Render Modes

Training complete on 537 ARW+JPG pairs. MLP trained with 0.23% validation loss.

**Problem discovered:** Raw MLP predictions are conservative (dials cluster near 0.5). The optimizer learned to match camera JPGs by pulling back our pipeline's saturation.

**Solution:** Camera Base + Deltas mode. Use average dials across training as "camera base", then add per-image deltas from MLP.

### Render Modes

```bash
# Raw MLP prediction (conservative)
bin/vibe render image.ARW --model etc/camera.vibe --out tmp/

# With lush boost (+0.15 to vibrance/saturation)
bin/vibe render image.ARW --model etc/camera.vibe --out tmp/ --lush 0.15

# Camera base + deltas (recommended)
bin/vibe render image.ARW --model etc/camera.vibe --out tmp/ --base
```

### Camera Base Dials

Mean across 537 training samples:
- **VIEW:** exposure=0.574, contrast=0.535, highlights=0.465, shadows=0.491
- **POPS:** vibrance=0.461, saturation=0.463, colourDensity=0.468

The sub-0.5 color dials reveal: our pipeline's neutral is more saturated than camera JPGs.

### Architecture

```
Features (23) → MLP → Raw Dials (45)
                         ↓
              Delta = Raw - 0.5
                         ↓
              Final = CameraBase + Delta
```

### Next Steps

1. Evaluate --base mode on more images
2. Consider per-vibe-class base dials (portrait_off, vivid_on, etc.)
3. Warm-start optimizer integration

See `doc/vibe.md` for full details.

---

## Previous Direction: Grid Search VIEW

Tested but didn't improve DSC00144 beyond 12.8%. The problem isn't VIEW search - it's that dial optimization has limits. Neural prediction may generalize better.

---

## Completed: Stage-Aware Optimization

### Implementation

| Component | File | Status |
|-----------|------|--------|
| `viewLoss()` | `diff.cpp` | Done |
| `popsLoss()` | `diff.cpp` | Done |
| `Mode::STAGED` | `geos.hpp` | Done |
| `optimizeStaged()` | `staged.cpp` | Done |
| `bin/gold` | `gold.cpp` | Done |

---

## Key Files

| File | Purpose |
|------|---------|
| `src/main/vibe.cpp` | VIBE training/inference binary |
| `src/main/part/vibe/mlp.hpp` | MLP forward/backward |
| `etc/camera.vibe` | Trained Stage 1 model |
| `tmp/var/vibe/train_full.json` | 537 training samples |

## Test Commands

```bash
cd LABS

# VIBE render with camera base
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/vibe render \
  /home/z/base/pics/DSC00144.ARW \
  --model etc/camera.vibe \
  --out tmp/var/vibe/base \
  --base
```

## Test Outputs

```
tmp/var/vibe/base/DSC00144/
  _preview.png  - camera preview (reference)
  _vibe.png     - pipeline with predicted dials
  _camera.png   - camera JPG resized
  _diff.png     - difference x5
```
