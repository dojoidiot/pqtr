# Pipe

## Objective

Clean-room stepwise recreation of darktable processing using pipe.hpp.
Each Link = one darktable module, matched 1:1.

## Golden Rules

**STOP AND RE-READ THESE RULES WHEN:**
- A test fails
- You're about to write code
- You're tempted to "simplify" or "improve"

### Rule 0: No LLM plans
**This file is the only plan.** No agent plans, no EnterPlanMode, no `~/.claude/plans/`.

### Rule 1: DT is always right
We are always wrong until we match DT exactly. No exceptions.

### Rule 2: Monkey see, monkey do
**CLEAN COPY from DT source.** Not "inspired by". Not "simplified".
- Find the exact DT source file
- Copy the algorithm line-by-line
- Keep the same variable names where possible

### Rule 3: Stop on failure
Each step must pass before moving to the next.
- **Test FAILS?** STOP. Apply Rule 6 to find the bug.
- Ask user if stuck after one iteration.

### Rule 4: C++ tests verify each module
- One test file per module: `src/test/<module>.cpp`
- Test must PASS before SIGNED OFF

### Rule 5: Use dark tool to decode XMP params
**ALWAYS** use `./tmp/build/dark <file.xmp> --dump` to decode module parameters.
Don't decode hex/base64 manually.

### Rule 6: Step-wise verification loop
**When a test fails, go back to Step 1.**

```
for module in pipeline_order:
    if verify(module) != PASS:
        FIX module  # Don't investigate downstream
        break
```

---

## Pipeline Architecture

**Two-stage pipeline:**
1. **Image Processing** - IOP order with colorspace swaps (gets color/tone correct)
2. **Geometric** - runs LAST on correct image data (moves correct pixels)

### Image Processing Pipeline

IOP order, woven by working space swaps. Each module runs in its correct colorspace.

```
SENSOR SPACE (Bayer float):
  rawprepare → temperature → highlights → demosaic

LINEAR RGB:
  exposure → colorin → [swap Lab→RGB]

RGB WORKING SPACE:
  channelmixer → sigmoid → [swap RGB→Lab]

LAB SPACE:
  bilat → [swap Lab→RGB]

RGB WORKING SPACE:
  exposure2 → filmicrgb → [swap RGB→Lab]

DISPLAY:
  colorout → gamma
```

### Geometric Pipeline (runs LAST)

After image processing is correct, geometric transforms move pixels:
```
  flip → lens
```

### Swap Functions

Clean copy from `common/colorspaces_inline_conversions.h`:
- `swapLabToRGB()` - Lab → XYZ → linear sRGB (D50)
- `swapRGBToLab()` - linear sRGB → XYZ → Lab (D50)

---

## Reference

- **darktable**: v5.3.0 (in `dark/`)
- **LibRaw**: (in `LibRaw/`)
- **Test file**: `src/test/DSC00144.ARW` (Sony A7III, 3968x2648)
- **XMP files**: `default.xmp` (6 modules), `sony.xmp` (11 modules)

---

## Status

### Phase 1: Default Pipeline (default.xmp)

| Module | Status | Notes |
|--------|--------|-------|
| rawprepare | ✓ DONE | exact match |
| temperature | ✓ DONE | exact match |
| demosaic | ✓ DONE | 0.984 correlation |
| colorin | ✓ DONE | LCMS matrix |
| colorout | ✓ DONE | Lab→sRGB |
| gamma | ✓ DONE | sRGB transfer |
| **TOTAL** | **0.98 correlation** | Phase 1 verified |

### Phase 2: Sony Pipeline (sony.xmp)

| Module | Status | Notes |
|--------|--------|-------|
| swap Lab↔RGB | ✓ DONE | clean copy from DT |
| sigmoid | ✓ DONE | 0.98 correlation with swaps |
| exposure | ✓ DONE | fixed formula |
| channelmixerrgb | ✓ DONE | identity in XMP (no-op) |
| highlights | ✓ DONE | opposed inpaint (0.994 corr) |
| **TOTAL** | **0.994 correlation** | Phase 2 complete |

---

### Phase 3: Canon Pipeline (canon.xmp)

Canon EOS 5D Mark III style applied on top of sony.xmp.
Adds filmicrgb, colorbalancergb, bilat for "Canon color science" look.
**sigmoid disabled, filmicrgb enabled** for Canon tone mapping.

**Processing order (IOP order v3.0):**

| IOP | Module | Colorspace | Status | Notes |
|-----|--------|------------|--------|-------|
| 1.0 | rawprepare | SENSOR | ✓ | Phase 1 |
| 3.0 | temperature | SENSOR | ✓ | Phase 1 |
| 5.0 | highlights | SENSOR | ✓ | Phase 2 |
| 9.0 | demosaic | SENSOR→RGB | ✓ | Phase 1 |
| 21.0 | exposure (0.7 EV) | RGB | ✓ | sony.xmp default |
| 28.0 | colorin | RGB→Lab | ✓ | Phase 1 |
| 30.5 | channelmixerrgb | RGB (swap) | ✓ | Phase 2 (no-op) |
| 41.5 | colorbalancergb | RGB (Jzazbz) | - | saturation+0.2, vibrance+0.2 |
| 46.0 | filmicrgb | RGB | ✓ | **CLEAN COPY** grey=18.45, black=-5, white=4 |
| 46.0+ | exposure (1.2 EV) | RGB | ✓ | Canon style, runs AFTER filmicrgb |
| 54.0 | bilat | Lab (auto-swap) | - | local Laplacian, complex |
| 70.0 | colorout | Lab→RGB | ✓ | Phase 1 |
| 78.0 | gamma | RGB | ✓ | Phase 1 |

Note: sigmoid is DISABLED in canon.xmp. filmicrgb handles tone mapping.
Canon style adds second exposure (+1.2 EV) that runs after filmicrgb.

**filmicrgb implementation:**
- Clean copy from `dark/lib/desk/src/iop/filmicrgb.c`
- Key functions: `log_tonemapping_v2_1ch()` (line 907), `filmic_spline()` (line 947)
- Gaussian elimination for 4th order polynomial spline coefficients
- Power norm for ratio-preserving per-pixel processing

| **TOTAL** | **0.986 correlation** | filmicrgb working |

**Remaining for 0.99+:**
- colorbalancergb: mild saturation boost (0.2), low impact expected
- bilat: local Laplacian (~500 lines), complex multi-scale filter

---

### Phase 4: Final Pipeline (final.xmp)

Complete pipeline with all modules. Uses **both sigmoid AND filmicrgb**.
Adds lens correction, local contrast, and color grading.

**Enabled modules (history order):**

| # | Module | Status | Notes |
|---|--------|--------|-------|
| 0 | rawprepare | ✓ | Phase 1 |
| 1 | demosaic | ✓ | Phase 1 |
| 2 | colorin | ✓ | Phase 1 |
| 3 | colorout | ✓ | Phase 1 |
| 4 | gamma | ✓ | Phase 1 |
| 5 | temperature | ✓ | Phase 1 |
| 6 | highlights | ✓ | Phase 2 |
| 7 | channelmixerrgb | ✓ | Phase 2 |
| 8 | exposure (0.7 EV) | ✓ | Phase 2 |
| 9 | flip | - | orientation, no-op for this image |
| 10 | sigmoid | ✓ | Phase 2 (ENABLED, runs before filmicrgb) |
| 11 | lens | ✓ | warp.cpp (lensfun) |
| 13 | bilat | ✓ | local Laplacian (99.9% pixels modified) |
| 14 | colorbalancergb | ✓ | identity (all params default) |
| 15 | exposure (1.1 EV) | ✓ | second instance |
| 16 | filmicrgb | ✓ | Phase 3 (runs AFTER sigmoid) |

Note: basecurve (12) and second sigmoid (17) are DISABLED.

**Status: PROCESSING COMPLETE** (0.9968 correlation)
- All processing modules verified
- Geometric (lens, flip) deferred - doesn't affect color/tone accuracy

**Verified modules:**
- bilat: local Laplacian (0.9968 corr with sigmoid+bilat+filmicrgb)
- colorbalancergb: identity (all params at defaults)
- flip: implemented, no-op for this image

---

## Next Steps

1. ✓ Highlights implemented using "inpaint opposed" algorithm
2. ✓ Phase 2 (sony.xmp) complete at 0.994 correlation
3. ✓ Phase 3 (canon.xmp) filmicrgb at 0.986 correlation
4. ✓ Phase 4: bilat (local Laplacian contrast)
5. ✓ Phase 4: colorbalancergb (identity, all params default)
6. ✓ Phase 4: flip (orientation, implemented)
7. ✓ Phase 4 PROCESSING COMPLETE: 0.9968 correlation
8. TODO: lens geometric correction (separate from processing pipeline)
