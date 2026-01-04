# Pipeline Status

## Phase Complete: Copy

The copy phase is **complete**. All DT modules have been copied and verified:

| Module | Status | Tolerance | Colorspace |
|--------|--------|-----------|------------|
| sony.c (decoder) | verified | 100% match | RAW |
| rawprepare | verified | 100% match | RAW |
| temperature | verified | 100% match | RAW |
| highlights | verified | 1e-2 | RAW |
| demosaic | verified | 1e-2 | RAW→RGB |
| exposure | verified | 100% match | RGB |
| colorin | verified | 1e-3 | RGB |
| channelmixerrgb | verified | functional | RGB |
| colorbalancergb | verified | functional | RGB |
| filmicrgb | verified | 3e-2 | RGB |
| bilat | verified | functional | LAB (auto-converted) |
| colorout | verified | 100% match | RGB |

**Colorspace API:** bilat declares `inputColorspace() = Lab` and the pipeline auto-inserts RGB↔Lab conversions.

---

## Colorspace Architecture

### The Problem

DT's pixelpipe manages colorspace automatically:
1. Each module declares `input_colorspace()` and `output_colorspace()`
2. Pixelpipe tracks current colorspace in `pipe->dsc.cst`
3. Before each module, if colorspaces differ, auto-inserts conversion

Our pipe currently assumes all modules work in RGB - **this is wrong**.

### DT Module Colorspace Declarations

From darktable source (`src/iop/*.c`):

```c
// exposure.c
dt_iop_colorspace_type_t default_colorspace(...) { return IOP_CS_RGB; }

// colorbalancergb.c
dt_iop_colorspace_type_t default_colorspace(...) { return IOP_CS_RGB; }

// filmicrgb.c
dt_iop_colorspace_type_t default_colorspace(...) { return IOP_CS_RGB; }

// bilat.c
dt_iop_colorspace_type_t default_colorspace(...) { return IOP_CS_LAB; }  // ← Different!
```

### Required Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Flow tracks: current_colorspace (RAW, RGB, Lab, XYZ)   │
└─────────────────────────────────────────────────────────┘

Each Step declares:
  - inputColorspace()   → what it expects
  - outputColorspace()  → what it produces

Pipe::body() checks:
  if (current_cs != step.input_cs) → insert conversion step

Conversion steps needed:
  - RgbToLabStep: RGB → XYZ (matrix) → Lab (CIE D50)
  - LabToRgbStep: Lab → XYZ → RGB (inverse matrix)
```

### Colorspace Flow (Correct)

```
RAW ──────────────────────────────────────────────────────►
     rawprepare  temperature  highlights  demosaic
                                              │
RGB ◄─────────────────────────────────────────┘
     exposure  colorin  channelmixer  colorbalance  filmic
                                                       │
                                          ┌────────────┘
                                          ▼
                                    [RGB → Lab]  ← auto-insert
                                          │
LAB                                       ▼
                                        bilat
                                          │
                                          ▼
                                    [Lab → RGB]  ← auto-insert
                                          │
RGB ◄─────────────────────────────────────┘
     colorout
```

---

## Pipeline Phases

### Phase 1: Baseline (RAW → Linear RGB)

Modules: `rawprepare → temperature → highlights → demosaic → colorin → channelmixer → colorout`

- Fixed processing, metadata-driven
- Channelmixer included (DT auto-applies in scene-referred mode)
- No user-tunable parameters
- Output: sRGB PNG

Test: `make test-phase1` (labs/)

### Phase 2: Color Science (Match Camera JPEG)

Modules: `phase1 → exposure → colorbalance → filmic → [rgb_to_lab] → bilat → [lab_to_rgb] → colorout`

- Optimizable parameters
- Goal: Match manufacturer camera JPEG output
- All operate in linear RGB (scene-referred until filmic)
- Bilat requires Lab colorspace (auto-converted)

Test: `make test-phase2` (labs/) - identical to `make test-gold`

**Official DT Sony Camera Style (ILCE-7M3, ILCE-7RM5):**
```
exposure (EV=1.1) → colorbalancergb → bilat → filmicrgb
```

### Phase 3: Effects (User Creative)

Modules: `bilat` (local contrast)

- Optional, after color science
- bilat requires Lab colorspace conversion
- Not part of color optimization

---

## Current Phase: Tuning

### Goal
Optimize pipeline parameters to match DT output without code changes.

### Approach
See **tune.md** for:
- Objective functions (brightness, shadow preservation, channel balance)
- Tunable parameters per module
- Optimization strategy

### Current State

**Sony ILCE-7M3 Profile:**
- exposure_bias: 0.0 EV (DT module default - XMP/style provides actual value)
- d65_coeffs: from cameras.xml matrix (correct)

**Outstanding Issue:**
- Shadow crush: Gold has ~2x more very dark pixels than DT reference
- Root cause: NOT a code bug (verified filmicrgb spline is correct)
- Solution: Parameter tuning in colorbalancergb

**Bilat Issue:** FIXED
- Implemented Colorspace enum in Step interface
- BilatStep declares `inputColorspace() = Lab`
- Pipeline inserts RgbToLabStep before bilat, LabToRgbStep after
- Conversion uses Rec2020→XYZ→Lab (D50 white point)

---

## Next Steps

### 1. Implement Colorspace API in Step

```cpp
enum class Colorspace { RAW, RGB, Lab, XYZ };

class Step {
    virtual Colorspace inputColorspace() { return Colorspace::RGB; }
    virtual Colorspace outputColorspace() { return Colorspace::RGB; }
    virtual void* exec(Flow& flow) = 0;
};
```

### 2. Add Conversion Steps

```cpp
class RgbToLabStep : public Step {
    Colorspace inputColorspace() override { return Colorspace::RGB; }
    Colorspace outputColorspace() override { return Colorspace::Lab; }
    // RGB → XYZ (Rec2020 matrix) → Lab (CIE D50)
};

class LabToRgbStep : public Step {
    Colorspace inputColorspace() override { return Colorspace::Lab; }
    Colorspace outputColorspace() override { return Colorspace::RGB; }
    // Lab → XYZ → RGB (inverse matrix)
};
```

### 3. Update Pipe to Auto-Insert Conversions

```cpp
Pipe& Pipe::body(const std::string& name, std::unique_ptr<Step> step) {
    Colorspace current = flow_.colorspace();
    Colorspace required = step->inputColorspace();

    if (current != required) {
        // Auto-insert conversion step
        insertConversion(current, required);
    }

    // Add actual step
    steps_.push_back(std::move(step));
    flow_.setColorspace(steps_.back()->outputColorspace());

    return *this;
}
```

### 4. Fix BilatStep

```cpp
class BilatStep : public Step {
    Colorspace inputColorspace() override { return Colorspace::Lab; }
    Colorspace outputColorspace() override { return Colorspace::Lab; }
    // Process L channel only, preserve a/b
};
```

### 5. Verify with DT Reference

- Run darktable-cli with DSC00458.ARW + XMP
- Compare output pixel-perfect with our pipeline
- Verify colorspace conversions match DT's dt_ioppr_transform_image_colorspace()

---

## Architecture

```
RAW file
    ↓
Sony decoder (sony.c)
    - Extracts: dimensions, strip_offset, curve, WB, black/white levels
    - Extracts: color matrix from cameras.xml
    - Computes: d65_coeffs from matrix
    - exposure_bias: 0.0 (DT default, XMP/style provides actual value)
    ↓
PipeState populated
    ↓
rawprepare → temperature → highlights → demosaic
    ↓
exposure (uses state.exposure_bias)
    ↓
colorin → channelmixerrgb → colorbalancergb → filmicrgb → bilat → colorout
    ↓
PNG output
```

---

## Files

### Core Pipeline
```
src/main/labs/sony.c              # Sony decoder + metadata
src/main/labs/pipe_state.h        # Pipeline state struct
src/main/labs/pipe_prepare.c      # Derived value computation
src/main/labs/mods/*.c            # All copied modules
```

### Test
```
src/test/labs/gold.cpp            # Full pipeline test
src/test/raws/sony.ARW            # Test input (ILCE-7M3)
src/test/raws/DSC00458.ARW        # Sony style test input
src/test/raws/DSC00458.ARW.xmp    # DT XMP with Sony style
src/test/raws/DSC00458.JPG        # Camera-generated reference
tmp/var/gold.png                  # Our output
tmp/var/dt_gold_ref.png           # DT reference
```

### DT Camera Styles
```
dark/lib/dark/share/darktable/styles/darktable_Sony_ILCE-7M3.dtstyle
dark/lib/dark/share/darktable/styles/darktable_Sony_ILCE-7RM5.dtstyle
```

Official Sony styles use: `exposure → colorbalancergb → bilat → filmicrgb`
- sigmoid: disabled
- basecurve: disabled

### Documentation
```
tune.md                           # Tuning parameters and objectives
plan.md                           # Project overview
tree.md                           # Parameter tree
```

---

## Commands

```bash
# Build and run gold pipeline
make test-gold

# Compare with DT reference
python3 -c "
from PIL import Image
import numpy as np
g = np.array(Image.open('tmp/var/gold.png'))[:,:,:3].astype(float)
d = np.array(Image.open('tmp/var/dt_gold_ref.png'))[:,:,:3].astype(float)
print(f'Mean diff: {(g-d).mean():.2f}')
print(f'Gold mean: {g.mean():.2f}, DT mean: {d.mean():.2f}')
"
```

---

## Historical Reference

The original copy process documentation is preserved below for reference when adding new camera support or modules.

<details>
<summary>Copy Process (for new modules)</summary>

**COPY, don't think.** When you start interpreting or improving, stop and ask.

**Each module is an independent program.** The only common structure is PipeState.

**Dump runtime data, not sources.** When a module uses internal data, dump the exact runtime values from DT via fprintf.

### Path to Success (per module)

1. **Add fprintf to DT** - dump ALL runtime data the module uses
2. **Rebuild DT module** - `cmake --build . --target <module>`
3. **Run darktable-cli** - capture stderr with all dumped values
4. **Copy process() code** - inline all dependencies
5. **Copy runtime data** - paste exact values from step 3
6. **Test** - must achieve functional match

### Verification

```bash
darktable-cli src/test/raws/sony.ARW src/test/raws/sony.xmp /tmp/out.png \
    --core --disable-opencl --dump-pipe <module> --dumpdir /tmp/dtdump
```

</details>

<details>
<summary>Module Order (DT v50_order)</summary>

| Order | Module | Status |
|-------|--------|--------|
| 1 | rawprepare | done |
| 3 | temperature | done |
| 4 | highlights | done |
| 8 | demosaic | done |
| 21 | exposure | done |
| 28 | colorin | done |
| 39 | channelmixerrgb | done |
| 41.5 | colorbalancergb | done |
| 46 | filmicrgb | done |
| 54 | bilat | done |
| 70 | colorout | done |

</details>

<details>
<summary>Color Matrices</summary>

Sony raw files embed a color matrix, but use DT's cameras.xml matrix for compatibility.

D65coeffs are computed from XYZ_to_CAM matrix:
1. Multiply by RGB_to_XYZ to get RGB_to_CAM
2. Sum each row, invert to get per-channel multipliers
3. Normalize to G=1.0

</details>
