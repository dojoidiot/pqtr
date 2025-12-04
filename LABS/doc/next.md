# Next Steps

## Context

Read this file after `README.md` to understand current LABS development state.

**BASE** = 435b907 (2025-12-05)

### Where We Are

| Image | Poly | Final | Notes |
|-------|------|-------|-------|
| DSC00144 | 12.8% | **12.0%** | Hard image - narrow gamut |
| DSC00202 | 2.6% | **3.7%** | Green foliage, neutral wood |
| DSC01559 | 7.2% | **8.0%** | Higher resolution test |

TUNE works. The 45 dials optimize to match reference images. But dial interference limits accuracy on hard images.

---

## Current Direction: Stage-Aware Optimization

**Full documentation: [stages.md](stages.md)**

### The Problem

All 45 dials optimize against one loss function. VIEW dials (contrast, exposure) fight POPS dials (saturation, vibrance) because they both affect the same loss features.

Example (DSC00144): Optimizer boosts saturation → this affects luminance perception → optimizer flattens contrast to compensate → result is muddy.

### The Semantic Reframe

Instead of "optimize 45 dials", think in **stages**:

```
RAWS → FLAT → VIEW → POPS
```

| Stage | Purpose | Dials | Loss |
|-------|---------|-------|------|
| RAWS | Camera → Linear | 0 | Deterministic |
| FLAT | Checkpoint | 0 | Verify decode |
| VIEW | Linear → Display | 5 | **Absolute tone** |
| POPS | Display → Style | 40 | **Relative color** |

### Key Insight

**VIEW** = absolute structure ("shadows at L=0.03")
**POPS** = relative relationships ("greens pop 20% more than neutrals")

The reference encodes both. But optimizing both with one loss causes interference.

### Implementation Plan

1. **Stage-aware loss functions**
   - `viewLoss()`: Weight `std_L, skew_L, L_p10-90` heavily
   - `popsLoss()`: Weight `mu_C, std_C, C_p50, C_shadow` heavily

2. **Mode::STAGED optimizer**
   - Phase 1: VIEW dials with viewLoss()
   - Phase 2: POPS dials with popsLoss()
   - Phase 3: Optional joint refinement

3. **Test on problem images**

---

## Key Files

| File | Purpose |
|------|---------|
| `doc/stages.md` | Full stage-aware optimization documentation |
| `doc/hack.md` | Reverse engineering notes, camera color science |
| `src/main/part/geos/spsa.cpp` | SPSA optimizer implementation |
| `src/main/part/geos/diff.cpp` | Loss function (geodesic distance) |
| `inc/dials.h` | Dial definitions |

## Test Commands

```bash
cd LABS
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune \
  var/pics/DSC00144.ARW preview \
  --save-area tmp/var/tune \
  --full --optimizer hybrid --fine --logs
```

## Test Outputs

```
tmp/var/tune/DSC00144/
  head.png    - camera preview (reference)
  tail.png    - pipeline output
  diff.png    - difference x5
  result.png  - final output
  tune.json   - dial settings
```
