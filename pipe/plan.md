# Pipe

## Objective

Clean-room recreation of darktable processing. Each Link = one DT module, matched 1:1.

---

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
- **Test FAILS?** STOP. Fix before continuing.
- Ask user if stuck after one iteration.

### Rule 4: Use dark tool to decode XMP params
**ALWAYS** use `./tmp/build/dark <file.xmp> --dump` to decode module parameters.
Don't decode hex/base64 manually.

### Rule 5: Step-wise verification
When a test fails, fix the earliest failing module first.

```
for module in pipeline_order:
    if verify(module) != PASS:
        FIX module  # Don't investigate downstream
        break
```

### Rule 6: Neutral Pipeline
**Camera-agnostic testing.** Neutral = camera data normalized + DT module defaults.

- **Head normalizes** camera-specific data to standard tree schema
- **Modules read** from standard tree paths (never camera-specific)
- **Module params** use DT defaults when XMP not provided
- **Tests work unchanged** for any camera after head

### Rule 7: Separation of Concerns
**Extract → Params → Process.** Modules are pure computation.

```
params = extract(tree)     # Tree read (knows schema)
assertColorspace(expected) # Colorspace gate
module.process(flow, params)  # Pure processing (no tree access)
```

- **Extract**: Centralized tree→params conversion
- **Params**: Typed structs per module
- **Process**: Pure computation, no tree coupling

---

## Testing Strategy

### Principle: DT is the oracle

We don't write test code. We compare our output directly against darktable's output.
Same input (RAW + XMP) → same output = we match DT.

### How it works

```
                    ┌─────────────┐
   test.xmp ───────►│  pipe-cli   │───► our.png ──┐
                    └─────────────┘               │
                                                  ▼
   input.raw ──────────────────────────────► diff_test ──► delta-E
                                                  ▲
                    ┌─────────────┐               │
   test.xmp ───────►│darktable-cli│───► dt.png ──┘
                    └─────────────┘
```

1. Run both CLIs with same XMP:
   ```bash
   pipe-cli input.raw test.xmp our.png
   darktable-cli input.raw test.xmp dt.png
   ```

2. Compare outputs:
   ```bash
   diff_test our.png dt.png
   ```

3. Delta-E = signal for what to fix

### Test coverage

DT integration tests: 176 XMPs in `dark/lib/desk/src/tests/integration/`

| Image | Tests | Status |
|-------|-------|--------|
| mire1.cr2 (Canon) | 162 | ✓ have decoder |
| hlrecovery.arw (Sony) | 4 | ✓ have decoder |
| mire1-xtrans.raf (Fuji) | 8 | need X-Trans |
| xtransIV.raf (Fuji) | 2 | need X-Trans |

**166 tests runnable now** with Canon + Sony decoders.

### Tolerance

```
delta-E < 1:   PASS (imperceptible)
delta-E 1-2:   WARN (investigate)
delta-E > 2:   FAIL
```

### Head requirement

Before running DT unit tests, head must decode all 3 formats identically to LibRaw:

| Format | Camera | Status | Notes |
|--------|--------|--------|-------|
| Sony ARW | A7III, RX100M3 | ✓ DONE | Bayer + info tree verified |
| Canon CR2 | EOS 40D | ✓ DONE | LJpeg decode matches LibRaw exactly |
| Fuji RAF | X-Trans | TODO | Different CFA pattern |

Once head matches LibRaw for all formats → safe to run DT unit tests.
Any delta-E difference is then purely in our modules, not input.

---

## Pipeline Architecture

**Two-stage pipeline:**
1. **Image Processing** - IOP order with colorspace swaps
2. **Geometric** - runs LAST (moves correct pixels)

### Processing Order

```
SENSOR (Bayer float):
  rawprepare → temperature → highlights → demosaic

LINEAR RGB:
  exposure → colorin → [swap Lab→RGB]

RGB WORKING SPACE:
  channelmixer → sigmoid → [swap RGB→Lab]

LAB:
  bilat → [swap Lab→RGB]

RGB WORKING SPACE:
  exposure2 → filmicrgb → [swap RGB→Lab]

DISPLAY:
  colorout → gamma
```

### Geometric (runs LAST)

```
flip → lens
```

### Colorspace Swaps

Clean copy from `common/colorspaces_inline_conversions.h`:
- `swapLabToRGB()` - Lab → XYZ → linear sRGB (D50)
- `swapRGBToLab()` - linear sRGB → XYZ → Lab (D50)

---

## Info Tree Schema

Standard tree paths populated by head. Modules read these, never camera-specific data.

### Image Data
| Path | Type | Unit | Description |
|------|------|------|-------------|
| `width` | float | pixels | RAW width |
| `height` | float | pixels | RAW height |
| `black` | float | 0-65535 | Black level (averaged if per-channel) |
| `white` | float | 0-65535 | White/saturation level |
| `bayer` | string | "RGGB" | CFA pattern |

### White Balance
| Path | Type | Unit | Description |
|------|------|------|-------------|
| `wb/r` | float | multiplier | Red multiplier (normalized: g1=1.0) |
| `wb/g1` | float | multiplier | Green1 multiplier (always 1.0) |
| `wb/b` | float | multiplier | Blue multiplier |
| `wb/g2` | float | multiplier | Green2 multiplier |

### Color
| Path | Type | Unit | Description |
|------|------|------|-------------|
| `cam_xyz/0-8` | float | matrix | Camera RGB → XYZ D50 (row-major 3×3) |

### Camera
| Path | Type | Description |
|------|------|-------------|
| `camera/make` | string | Manufacturer |
| `camera/model` | string | Model name |

### EXIF
| Path | Type | Description |
|------|------|-------------|
| `exif/iso` | float | ISO speed |
| `exif/shutter` | float | Shutter speed (seconds) |
| `exif/aperture` | float | F-number |
| `exif/focal_length` | float | Focal length (mm) |
| `exif/lens` | string | Lens model |
| `exif/orientation` | float | EXIF orientation (1-8) |

### Crop
| Path | Type | Description |
|------|------|-------------|
| `crop/left` | float | Active area left offset |
| `crop/top` | float | Active area top offset |
| `crop/width` | float | Active area width |
| `crop/height` | float | Active area height |

---

## Tools

### diff_test - Delta-E Comparison

```bash
./tmp/build/diff_test <img1.png> <img2.png> [diff_output.png]
```

**Output:**
```
Delta-E Statistics:
  Mean:          4.980
  Max:           65.880
  >1 (JND):      75.05%
  >2 (visible):  59.03%
  Correlation:   0.9948
```

**API (pipe.hpp):**
```cpp
flow::diff(img1, img2, w, h, compute_map);
flow::diff_float(buf1, buf2, w, h, colorspace);  // For intermediate stages
flow::diff_image(img1, img2, w, h, mode, scale);
flow::print_diff_stats(result);
```

---

## Status

### Head (RAW Decoder)

| Format | Status | Test |
|--------|--------|------|
| Sony ARW | ✓ | Bayer matches LibRaw, info tree complete |
| Canon CR2 | ✓ | LJpeg + MakerNotes (WB, black) complete |
| Fuji RAF | - | X-Trans CFA |

### Modules

| Module | Status | Correlation | Notes |
|--------|--------|-------------|-------|
| rawprepare | ✓ | exact | BLC + normalize |
| temperature | ✓ | exact | WB on mosaic |
| highlights | ✓ | 0.994 | Opposed inpaint |
| demosaic | ✓ | 0.984 | Bilinear |
| exposure | ✓ | exact | EV multiply |
| colorin | ✓ | exact | Camera→XYZ→Lab |
| channelmixerrgb | ✓ | exact | 3×3 matrix |
| sigmoid | ✓ | 0.98 | Tone map |
| bilat | ✓ | 0.997 | Local Laplacian |
| filmicrgb | ✓ | 0.986 | Filmic tone |
| colorbalancergb | ✓ | exact | Identity (defaults) |
| colorout | ✓ | exact | Lab→sRGB |
| gamma | ✓ | exact | sRGB transfer |
| flip | ✓ | - | Orientation |
| lens | - | - | Geometric warp |

**Overall: 0.997 correlation** (processing complete, geometric pending)

---

## Reference

- **darktable**: v5.3.0 (in `dark/`)
- **DT tests**: `dark/lib/desk/src/tests/integration/`
- **Test images**: `dark/lib/desk/src/tests/integration/images/`
  - `mire1.cr2` - Canon EOS 40D (primary)
  - `hlrecovery.arw` - Sony RX100M3
  - `mire1-xtrans.raf` - Fuji X-Trans

---

## Next Steps

1. ~~**Canon CR2 head**~~ ✓ Done - LJpeg decode matches LibRaw
2. ~~**Canon MakerNotes**~~ ✓ Done - WB and black level from ColorData (0x4001)
3. **Refactor modules** - Implement Rule 7 (Extract → Params → Process)
4. **pipe-cli** - Match darktable-cli interface
5. **Run DT tests** - `DARKTABLE_CLI=./pipe-cli ./run.sh`
6. **Pass delta-E < 1** on all 176+ tests

---

## TODO: User Sliders (pqtr.md)

Modules needed to support final app sliders:

| Slider | DT Module | Status | Notes |
|--------|-----------|--------|-------|
| Brightness | exposure | ✓ | EV multiply |
| Contrast | filmicrgb | ✓ | contrast param |
| Highlights | shadhi | TODO | Bilateral shadows/highlights |
| Shadows | shadhi | TODO | Bilateral shadows/highlights |
| Saturation | colorbalancergb | ✓ | saturation_global param |
| Temperature | temperature | ✓ | Needs slider→coeffs mapping |
| Tint | channelmixerrgb | ⚠️ | Green-magenta axis, verify |
| Sharpness | sharpen | TODO | Unsharp mask |
| Vignette | vignetting | TODO | Radial falloff |

**Priority:**
1. shadhi - common user adjustment
2. sharpen - essential for output
3. vignetting - creative effect
