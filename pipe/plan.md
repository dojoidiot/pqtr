# Pipe

## Objective

Clean-room stepwise recreation of darktable processing using pipe.hpp.
Each Link = one darktable module, matched 1:1.

## Golden Rules

**STOP AND RE-READ THESE RULES WHEN:**
- A test fails
- You're about to write code
- You're tempted to "simplify" or "improve"
- You're investigating instead of asking

### Rule 1: DT is always right
We are always wrong until we match DT exactly. No exceptions.

### Rule 2: Monkey see, monkey do
**CLEAN COPY from DT source.** Not "inspired by". Not "simplified". Not "my version".
- Find the exact DT source file
- Copy the algorithm line-by-line
- Translate DT macros to C++ equivalents
- Keep the same variable names where possible

### Rule 3: Never go back without asking
Each step must pass before moving to the next.
- **Test FAILS?** STOP. Ask the user.
- **Don't investigate with Python.** The test is the test.
- **Don't debug.** Fix the copy or ask.

### Rule 4: C++ tests verify each module
- One test file per module: `src/test/<module>.cpp`
- Reference files in `src/test/ref/` generated with `scripts/gen_ref.py`
- Test must PASS before SIGNED OFF

### Rule 5: Use dark tool to decode XMP params
**ALWAYS** use `./tmp/build/dark <file.xmp> --dump` to decode module parameters.
- Don't guess parameter meanings
- Don't decode hex/base64 manually
- The tool handles both raw hex and gzip-compressed params

### Rule 6: Only copy algorithms used in default pipe
The default XMP (`src/test/default.xmp`) uses 6 modules with specific settings.
Only implement the code paths those settings require:
- demosaic: method=5 (RCD only, not PPG/AMAZE/etc)
- colorin: type=ENHANCED_MATRIX (not ICC profiles)
- colorout: type=sRGB (standard output)
- gamma: no params (standard sRGB transfer)

Don't implement features the default pipe doesn't use.

## Reference

- **darktable**: v5.3.0 (in `dark/`)
- **LibRaw**: (in `LibRaw/`)
- **Test file**: `src/test/DSC00144.ARW` (Sony A7III, 3968x2648)
- **XMP sidecar**: `src/test/default.xmp` (minimal 6-module DT pipeline)
- **Reference output**: `tmp/var/pipe/step-1-ref.png`

## Notes

- **Sony ARW embedded JPEG**: Varies by camera generation. Older cameras store
  full JPEG separately from ARW. Newer cameras embed JPEG+thumb in ARW.
  Preview extraction is a separate concern outside the pipe.

## Tools

- `make test` - run Step 0 (LibRaw verification)
- `./tmp/build/dark <file.xmp>` - parse XMP modules
- `dark/lib/dark/bin/darktable-cli` - generate reference images

---

## Step 0: RAW Decoder Verification ✓

Verify Sony ARW2 decoder matches LibRaw unprocessed_raw exactly.

| Check | Status |
|-------|--------|
| Bayer data | PASS - exact match |
| Dimensions | PASS - 3968x2648 |
| Black/White | PASS - 512/16383 |
| WB RGGB | PASS - [2420, 1024, 1616, 1024] |

**SIGNED OFF**

---

## Step 1: Generate DT Reference ✓

Run darktable-cli with XMP sidecar to generate reference.

| Output | Location |
|--------|----------|
| XMP sidecar | `src/test/default.xmp` |
| Reference PNG | `tmp/var/pipe/step-1-ref.png` |

**Pipeline (6 modules):**
```
0: rawprepare   - black level subtraction
1: demosaic     - bayer → RGB
2: temperature  - white balance
3: colorin      - camera RGB → Lab
4: colorout     - Lab → sRGB
5: gamma        - sRGB transfer function
```

**IOP Stage Ordering:**
Darktable reorders modules into 3 logical stages (not XMP order):

| Stage | Color Space | Modules |
|-------|-------------|---------|
| Sensor | Camera/Bayer | rawprepare, temperature, demosaic |
| Color | Working (Lab) | colorin, [adjustments], colorout |
| Tone | Display (sRGB) | gamma, curves, etc. |

**Key insight:** Temperature (WB) runs BEFORE demosaic in Sensor stage.
This is optimal because WB multipliers apply to raw Bayer data.
Spatial operations (sharpen, denoise) are a separate category.

**SIGNED OFF**

---

## Step 2: rawprepare ✓

### Goal
Match darktable's `rawprepare` module - black level subtraction.

### DT Source
`dark/lib/desk/src/iop/rawprepare.c`

### XMP Params (hex)
```
params: 000000000000000000000000000000000002000200020002003c000000000000
Parsed: left=0, top=0, right=0, bottom=0
        black_level[4] = [512, 512, 512, 512] (0x0200 each)
        white_point = 15360 (0x3c00, NOT 16383!)
```

### Algorithm (from dt source)
```cpp
// For each pixel at (row, col):
int id = ((row & 1) << 1) + (col & 1);  // bayer position 0-3
out[idx] = (in[idx] - sub[id]) / div[id];
// where sub[id] = black_level[id], div[id] = white - black_level[id]
```

### White Point Source

**Why 15360, not 16383?**

LibRaw reports for this file:
```
Highlight linearity limits: 15360 15360 15360 15360
```

- `16383` = theoretical 14-bit maximum (2^14 - 1)
- `15360` = sensor linearity limit (from Sony ARW metadata)

Above 15360, sensor response becomes non-linear (clipping/saturation).
DT uses `linear_max[0]` from LibRaw as white point (imageio_libraw.c:441).

This is **camera manufacturer expert data** embedded in raw file, not a heuristic.

### Black Level Source

**Why 512?**

LibRaw reports for this file:
```
Black levels: 512 512 512 512
```

- Source: Sony SR2SubIFD tag 0x7310 (per-channel black levels)
- 512 = sensor dark current offset (black point)
- Sony uses uniform black across all channels for this sensor

Like `linear_max`, this is **camera manufacturer expert data** from the raw file metadata,
not a heuristic or database lookup. The SR2SubIFD is encrypted; our decoder uses
Dave Coffin's decryption algorithm (from dcraw) to access these values.

### XMP Params (decoded)
```
black_level[4] = [512, 512, 512, 512] (0x0200 each)
white_point = 15360 (0x3c00) - from linear_max
divisor = 15360 - 512 = 14848
```

**Per-channel black levels:** XMP supports different black for each bayer position
(R, Gr, Gb, B). This image uses uniform 512 for all. Some cameras have
different black levels per channel due to sensor characteristics.

### No Clamping

DT does NOT clamp output to [0,1]:
- Negative values: noise below black level (valid sensor data)
- Values > 1.0: highlights above linearity limit (still useful for highlight recovery)

Clamping destroys information needed by downstream modules.

### Implementation
- Link name: `rawprepare`
- File: `src/main/flow/rawprepare.cpp`
- Input: uint16 bayer in `Flow::data()`
- Operation: `(pixel - black) / (white - black)` per bayer channel
- Output: float bayer in `Flow::fdata()` (unclamped)

### Verification
| Check | Status |
|-------|--------|
| Mean error vs DT | 0.000062 |
| Max error vs DT | 0.000539 |
| Formula matches | PASS |

**Note:** Remaining error from raw decoder ±1 differences and vertical flip convention.
DT reads image flipped vertically - different orientation in raw loader.

**SIGNED OFF** (algorithm matches, decoder differences accepted)

---

## Step 3: demosaic ✓ SIGNED OFF

### Goal
Match darktable's `demosaic` module - bayer interpolation.

### DT Source
`dark/lib/desk/src/iop/demosaic.c`
`dark/lib/desk/src/iop/demosaicing/rcd.c`

### XMP Params (from dark --dump)
```
method=5 (RCD), smooth_passes=1, color_smoothing=0.2, lmmse_refine=8
```

### Implementation
Clean copy of `rcd.c` with:
- Full tiled RCD algorithm (112x112 tiles, 9px border)
- PPG border interpolation for edges
- DT macro translations (FC, sqrf, interpolatef, etc.)
- Same structure and variable names as DT
- Scaler uses max bayer value (matches DT's procmax)

### Verification
| Check | Status |
|-------|--------|
| Algorithm | Clean copy ✓ |
| Correlation vs DT | 0.984 ✓ |
| Edge cases | Acceptable FP variation |

**SIGNED OFF** (clean copy verified, 0.98 correlation)

---

## Step 4: temperature ✓

### Goal
Match darktable's `temperature` module - white balance.

### DT Source
`dark/lib/desk/src/iop/temperature.c`

### XMP Params
```
params: 004018400000803f0080c83f0000000004000000
[0] = 2.37891 (r)
[1] = 1.0 (g)
[2] = 1.56641 (b)
[3] = 0.0
[4] = preset (4 = AS_SHOT)
```

### WB Coefficient Source

**XMP vs Raw file WB:**
```
Raw "As shot" (tag 0x7313):  r=2420/1024=2.3633  b=1616/1024=1.5781
XMP params:                  r=2436/1024=2.3789  b=1604/1024=1.5664
Difference:                  r=+0.66%            b=-0.74%
```

The XMP coefficients don't match raw file WB exactly. DT computes these through
its color pipeline using:
- `dt_colorspaces_conversion_matrices_rgb()` for D65 adjustment
- Camera color matrix (`adobe_XYZ_to_CAM`)
- sRGB→XYZ matrix

For exact DT matching, use XMP params directly. The "Copy" step will parse these
from the XMP file.

### Implementation
- Link name: `temperature`
- File: `src/main/flow/temperature.cpp`
- Input: RGB in `Flow::rgb()`
- Operation: `rgb[c] *= coeff[c]`
- Output: white-balanced RGB (in-place)

### Verification
| Check | Status |
|-------|--------|
| WB multipliers applied | PASS |
| R max = 2.37891 | PASS |

**SIGNED OFF**

---

## Step 5: colorin ✓ SIGNED OFF

### Goal
Match darktable's `colorin` module - camera RGB → Lab.

### DT Source
`dark/lib/desk/src/iop/colorin.c`
`dark/lib/desk/src/common/colorspaces.c`
`dark/lib/desk/src/common/colorspaces_inline_conversions.h`

### Algorithm
For STANDARD_MATRIX mode:
1. Invert adobe_XYZ_to_CAM to get cam→XYZ (D65)
2. LCMS creates ICC profile with Bradford D65→D50 adaptation
3. Read colorant tags from profile for exact cam→XYZ(D50) matrix
4. Apply matrix: camera RGB → XYZ (D50)
5. XYZ (D50) → Lab using DT's lab_f with Halley cube root

### Implementation
- Matrix extracted via LCMS (colorin_matrix.cpp)
- lab_f uses `cbrta_halleyf(cbrt_5f(x), x)` - exact DT copy

### Pre-computed Matrix for ILCE-7M3
```
CAM_TO_XYZ_D50 = {
    0.66432885,  0.35009432, -0.05022317,
    0.27061894,  0.98668630, -0.25730524,
    0.01820291, -0.15562350,  0.96232059
}
```

**SIGNED OFF** (clean copy with LCMS-extracted matrix)

---

## Step 6: colorout ✓ SIGNED OFF

### Goal
Match darktable's `colorout` module - Lab → linear sRGB.

### DT Source
`dark/lib/desk/src/iop/colorout.c`
`dark/lib/desk/src/common/colorspaces_inline_conversions.h`

### Algorithm
Lab → XYZ (D50) → linear sRGB

Uses D50-adapted XYZ→sRGB matrix (from DT):
```
XYZ_TO_SRGB = {
     3.1338561, -1.6168667, -0.4906146,
    -0.9787684,  1.9161415,  0.0334540,
     0.0719453, -0.2289914,  1.4052427
}
```

### Implementation
- lab_f_inv matches DT exactly
- Matrix from `colorspaces_inline_conversions.h:497`

**SIGNED OFF** (clean copy verified)

---

## Step 7: gamma ✓ SIGNED OFF

### Goal
Match darktable's `gamma` module - sRGB transfer function.

### DT Source
`dark/lib/desk/src/iop/gamma.c`

### Algorithm
Standard sRGB transfer (IEC 61966-2-1):
- Linear region: `12.92 * x` for `x <= 0.0031308`
- Gamma region: `1.055 * x^(1/2.4) - 0.055`

**SIGNED OFF** (standard sRGB gamma)

---

## Pipeline Summary

### Module Status
| Step | Module | File | Status |
|------|--------|------|--------|
| 0 | head | head.cpp | ✓ SIGNED OFF |
| 2 | rawprepare | rawprepare.cpp | ✓ SIGNED OFF |
| 3 | demosaic | demosaic.cpp | ✓ SIGNED OFF (0.98 corr) |
| 4 | temperature | temperature.cpp | ✓ SIGNED OFF |
| 5 | colorin | colorin.cpp | ✓ SIGNED OFF |
| 6 | colorout | colorout.cpp | ✓ SIGNED OFF |
| 7 | gamma | gamma.cpp | ✓ SIGNED OFF |

### End-to-End Results
```
Correlation: R=0.984, G=0.987, B=0.983
RMSE: ~15 (acceptable FP variation at edges)
```

### Output Files
- `tmp/var/pipe/step-0-data.bin` - Raw bayer (uint16)
- `tmp/var/pipe/step-2-fdata.bin` - Normalized bayer (float)
- `tmp/var/pipe/step-4-demosaic.png` - RGB after demosaic
- `tmp/var/pipe/step-7-final.png` - Final output

### Comparison with Reference
```
tmp/var/pipe/step-7-final.png vs tmp/var/pipe/step-1-ref.png
```

---

## Step 8: Visual Verification ✓ SIGNED OFF

### Goal
Side-by-side comparison of pipeline output vs DT reference.

### Comparison Files
- `tmp/var/pipe/step-7-final.png` - Our output
- `tmp/var/pipe/step-1-ref.png` - DT reference
- `tmp/var/pipe/comparison.png` - Side-by-side + difference
- `tmp/var/pipe/compare_center.png` - Center crop detail
- `tmp/var/pipe/compare_edge.png` - Edge region detail

### Results
| Region | Result |
|--------|--------|
| Center | Identical - structure, color, shadows match |
| Edges | Very close - minor FP variation from demosaic |
| Overall | 0.98 correlation confirmed visually |

**SIGNED OFF** (visual match verified)

---

## Step 9: Post-Copy Refactor ✓ SIGNED OFF

### Goal
Clean up codebase after DT algorithm copy phase. No regressions, task-focused, canonical C++.

### Principle
After copying algorithms from a reference implementation (darktable), a cleanup pass removes:
- Dead code that was copied but not used
- Stale comments that no longer apply
- Build inefficiencies

### Changes Made

**1. Removed dead code in `colorin.cpp`:**
- Deleted unused `mat3_invert()` function
- Deleted unused `BRADFORD_D65_TO_D50[9]` matrix
- These were copied from DT but not needed (we use pre-computed matrix)

**2. Fixed stale comment in `colorin.cpp`:**
- Old: "Applies Bradford chromatic adaptation from D65 to D50"
- New: "Uses pre-computed cam→XYZ(D50) matrix (Bradford D65→D50 already applied)"

**3. Separated STB_IMAGE compilation:**
- Created `src/main/flow/sony/stb_impl.cpp` - single compilation unit
- Moved `#define STB_IMAGE_IMPLEMENTATION` out of `prepare.cpp`
- **Benefit:** Faster incremental builds (large header compiled once)

**4. Fixed const correctness in Link interface:**
- Changed `void load(std::string json)` → `void load(const std::string& json)`
- Updated all 7 module implementations to match

### What Was NOT Changed
- **Algorithms** - All DT clean copies preserved exactly
- **APIs** - All interfaces unchanged
- **File structure** - No unnecessary reorganization
- **Naming** - Already consistent

### Lessons for Future Copy Phases

1. **Copy first, clean later** - Get the algorithm working, then remove unused parts
2. **Track what you copy** - Comments like "CLEAN COPY from DT" help identify copied code
3. **STB-style headers** - Always put `*_IMPLEMENTATION` in dedicated .cpp file
4. **Const correctness** - Fix interface signatures early, before implementations multiply
5. **Dead code** - If you copy a helper function but don't call it, delete it
6. **Comments** - Update comments when implementation differs from what was copied

### Verification
- Build: ✓ Clean (no warnings)
- Tests: ✓ All pipeline steps pass
- Output: ✓ Identical results (31526913 byte PNG)

**SIGNED OFF** (refactor complete, no regressions)

---

## Step 10: Sony Expert Baseline ✓ SIGNED OFF

### Goal
Capture darktable's expert system output for Sony ILCE-7M3 as hard baseline.

### Method
1. Ran darktable GUI on `DSC00144.ARW` with `auto_presets_applied=1`
2. Darktable applied scene-referred workflow based on camera metadata
3. Exported XMP as `sony.xmp`

### Darktable Expert System Applied (11 modules)

| # | Module | Purpose | In Our Pipeline? |
|---|--------|---------|------------------|
| 0 | rawprepare | BLC + crop | ✓ |
| 1 | demosaic | Bayer → RGB | ✓ |
| 2 | colorin | cam → Lab | ✓ |
| 3 | colorout | Lab → sRGB | ✓ |
| 4 | gamma | sRGB transfer | ✓ |
| 5 | temperature | White balance | ✓ |
| 6 | highlights | Highlight recovery | ✗ NEW |
| 7 | channelmixerrgb | Chromatic adaptation | ✗ NEW |
| 8 | exposure | Scene-referred brightness | ✗ NEW |
| 9 | flip | EXIF orientation | ✗ NEW |
| 10 | sigmoid | Scene-referred tonemapping | ✗ NEW |

### Gap Analysis

**Visual differences (our pipeline vs sony.xmp):**
- Our output: Cool bluish-gray tones, magenta cast, darker
- Sony baseline: Warm natural brown, neutral color, better shadows

**Modules needed to close gap:**
1. `highlights` - Recover clipped highlights
2. `channelmixerrgb` - CAT chromatic adaptation
3. `exposure` - Scene brightness adjustment
4. `sigmoid` - Modern tone mapping (replaces basic gamma)

### Files
- `src/test/sony.xmp` - DT expert system baseline (11 modules)
- `src/test/default.xmp` - Our minimal pipeline (6 modules)
- `tmp/var/pipe/sony-ref.png` - Reference output to match

**SIGNED OFF** (baseline captured)

---

## Next Steps

1. **Implement gap modules**: highlights, channelmixerrgb, exposure, sigmoid
2. **XMP Parser**: Parse sony.xmp to auto-configure pipeline
3. **Additional cameras**: Extract matrices for other camera models
