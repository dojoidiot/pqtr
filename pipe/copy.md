# Pipeline Status

## Phase Complete: Copy

The copy phase is **complete**. All DT modules have been copied and verified:

| Module | Status | Tolerance |
|--------|--------|-----------|
| sony.c (decoder) | verified | 100% match |
| rawprepare | verified | 100% match |
| temperature | verified | 100% match |
| highlights | verified | 1e-2 |
| demosaic | verified | 1e-2 |
| exposure | verified | 100% match |
| colorin | verified | 1e-3 |
| channelmixerrgb | verified | functional |
| colorbalancergb | verified | functional |
| filmicrgb | verified | 3e-2 |
| bilat | verified | 3e-1 |
| colorout | verified | 100% match |

The pipeline produces output that is structurally correct but requires tuning for optimal visual match.

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
- exposure_bias: 1.05 EV (tuned - overall brightness matches)
- d65_coeffs: from cameras.xml matrix (correct)
- contrast: 0.80 (testing - lifts shadows)

**Outstanding Issue:**
- Shadow crush: Gold has ~2x more very dark pixels than DT reference
- Root cause: NOT a code bug (verified filmicrgb spline is correct)
- Solution: Parameter tuning in colorbalancergb

---

## Architecture

```
RAW file
    ↓
Sony decoder (sony.c)
    - Extracts: dimensions, strip_offset, curve, WB, black/white levels
    - Extracts: color matrix from cameras.xml
    - Computes: d65_coeffs from matrix
    - Sets: exposure_bias per camera model
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
src/test/raws/sony.ARW            # Test input
tmp/var/gold.png                  # Our output
tmp/var/dt_gold_ref.png           # DT reference
```

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
