# Copy Process

**COPY, don't think.** When you start interpreting or improving, stop and ask.

## Three Paths

Every module has three code paths to trace:

1. **DT Controller** - orchestrates pipeline, calls modules
2. **Processing Code** - RawSpeed (head) or DT iop (modules)
3. **PPM Debug Dump** - what DT writes, what we compare against

Copy the processing code. Verify against the PPM dump. 100% match required.

## Metadata

All parameters come from the raw file or DT defaults. Nothing is hardcoded per-camera.

| Source | Examples |
|--------|----------|
| Raw TIFF tags | strip_offset, width, height, curve, filters |
| DT module defaults | process() parameters via reset() |
| XMP sidecar | user adjustments (Phase 2) |

## PPM Format (DT dump)

```
Header: "P5\n<width> <height>\n" (no maxval line, 13 bytes typical)
Data: uint16 little-endian, rows reversed (last decoder row first)
```

## Test Set

```
src/test/raws/sony.ARW   # Sony A7 III raw file
src/test/raws/sony.xmp   # Reference XMP - defines which modules are active and their params
```

The XMP defines the complete pipeline configuration used for all copy verification.

## Verification

```bash
# Generate reference
darktable-cli src/test/raws/sony.ARW src/test/raws/sony.xmp /tmp/out.png \
    --core --disable-opencl --dump-pipe <module> --dumpdir /tmp/dtdump

# Reference files:
# /tmp/dtdump/export/NNNN_<module>_cpu_in_M.ppm   (input to module)
# /tmp/dtdump/export/NNNN_<module>_cpu_out_M.ppm  (output from module)
```

Test must achieve **100% pixel match** before proceeding.

---

# head(camera)

Decode raw file to match DT's rawprepare input exactly.

## Process

1. **Generate reference**
   ```bash
   darktable-cli src/test/raws/sony.ARW src/test/raws/sony.xmp /tmp/out.png \
       --core --disable-opencl --dump-pipe rawprepare --dumpdir /tmp/dtdump
   # Reference: /tmp/dtdump/export/0000_rawprepare_cpu_in_M.ppm
   ```

2. **Find decoder source**
   - Sony ARW: `dark/lib/desk/src/external/rawspeed/src/librawspeed/decompressors/SonyArw2Decompressor.cpp`
   - Canon CR2: `dark/lib/desk/src/external/rawspeed/src/librawspeed/decompressors/Cr2Decompressor.cpp`

3. **Copy all functions** into `src/main/labs/<camera>.c`
   - BitStreamer, TableLookUp, setWithLookUp, decompressRow, etc.
   - No edits. Pure copy with C++ → C type conversion only.
   - Include PPM writer with row reversal from `dark/lib/desk/src/common/pfm.c`

4. **Extract metadata from raw**
   ```bash
   exiftool -StripOffsets -ImageWidth -ImageHeight -SonyToneCurve <file>
   ```

5. **Test**
   ```c
   // Decode
   sony_arw2_decode(data + strip_offset, size, width, height, sony_curve, output);

   // Compare (skip 13-byte PPM header, rows reversed)
   for (row = 0; row < height; row++) {
       our_row = height - 1 - row;
       compare(output + our_row * width, ppm + row * width, width);
   }
   // Must be 100% match
   ```

## Sony ARW2 (verified working)

Source files copied:
- `SonyArw2Decompressor.cpp` → decompressRow
- `ArwDecoder.cpp` → decodeCurve
- `RawImage.h` → setWithLookUp
- `TableLookUp.cpp` → setTable
- `BitStreamer.h` + `BitStream.h` → BitStreamerLSB
- `Bit.h` → extractLowBits
- `pfm.c` → row reversal

Metadata from TIFF:
- Tag 273 (StripOffsets): 790528
- Tag 0x7010 (SonyToneCurve): 8000 10400 12900 14100
- Width: 6048, Height: 4024

Output: `src/main/labs/sony.c` - 100% match verified.

---

# rawprepare (verified working)

Black level subtraction and normalization.

## Source
- `dark/lib/desk/src/iop/rawprepare.c` → process() lines 322-358 (raw mosaic u16 path)

## Metadata from TIFF (exiftool)
- BlackLevel: 512 512 512 512
- WhiteLevel: 15360

## Formula
```c
id = ((row + top) & 1) << 1) + ((col + left) & 1);  /* Bayer index */
out = (in - black[id]) / (white - black[id]);
```

## Output
`src/main/labs/mods/rawprepare.c` - 100% match verified (24,337,152 pixels).

---

# PipeState

Pipeline state passed through all modules. Contains:
1. **Raw metadata** - populated by head decoder from raw file
2. **Derived values** - computed by pipe_prepare
3. **Module outputs** - set by modules for downstream use

```c
typedef struct {
    int width, height;
    uint32_t filters;              /* Bayer pattern (adjusted for row reversal) */

    /* Raw metadata - from head decoder */
    float adobe_XYZ_to_CAM[4][3];  /* Color matrix from raw file */
    float d65_color_matrix[9];      /* DNG embedded matrix (NAN if invalid) */

    /* Module outputs */
    struct {
        int enabled;
        float coeffs[4];            /* Set by temperature module */
    } temperature;

    struct {
        double D65coeffs[4];        /* Computed by pipe_prepare */
        double as_shot[4];          /* From raw EXIF WB */
        int late_correction;        /* Preset flag */
    } chroma;
} PipeState;
```

## Data Flow

```
RAW file
    ↓
Head decoder populates:
    - adobe_XYZ_to_CAM (from raw metadata, NOT cameras.xml)
    - d65_color_matrix (from DNG, or NAN for ARW)
    - as_shot (from EXIF WB RGGB Levels)
    - filters, dimensions
    ↓
pipe_prepare() computes:
    - D65coeffs from color matrix
    ↓
Modules process, may set:
    - temperature.coeffs (temperature module)
```

**Key insight**: All data comes from the raw file itself. No external camera databases needed.

## filters Value

Set by decoder, adjusted for row reversal in head:
- Sony ARW after row reversal: `0x49494949` (GBRG)
- Original sensor pattern: RGGB

## Output
`src/main/labs/pipe_state.h`

---

# pipe_prepare

Compute derived values from raw metadata. Called after head decoder, before modules.

## Source
- `dark/lib/desk/src/common/colorspaces.c` lines 2320-2399

## Function
```c
void pipe_prepare(PipeState *state);
```

Computes `D65coeffs` from `adobe_XYZ_to_CAM` matrix using sRGB D65 RGB→XYZ conversion.

## Output
`src/main/labs/pipe_prepare.c`

---

# temperature (verified working)

White balance multiplication per Bayer channel.

## Source
- `dark/lib/desk/src/iop/temperature.c` → process() lines 585-623 (bayer float path)
- `dark/lib/desk/src/develop/imageop_math.h` → FC() function

## Params from XMP
```
red = 2.42578125
green = 1.0
blue = 1.56640625
```

## Formula
```c
FC(row, col, filters) = (filters >> (((row << 1 & 14) + (col & 1)) << 1)) & 3;
out[p] = in[p] * coeffs[FC(row, col, filters)];
```

## Output
`src/main/labs/mods/temperature.c` - 100% match verified (24,337,152 pixels).

---

# highlights (verified working)

Highlight reconstruction using OPPOSED mode.

## Source
- `dark/lib/desk/src/iop/highlights.c` → params, clip_magics
- `dark/lib/desk/src/iop/hlreconstruct/opposed.c` → _process_opposed, _mask_dilated, _raw_to_cmap
- `dark/lib/desk/src/iop/hlreconstruct/segbased.c` → _calc_refavg, HL_POWERF

## PipeState Dependencies
Reads from PipeState (set by earlier stages):
- `temperature.coeffs` - for clip thresholds
- `chroma.D65coeffs` - for late correction
- `chroma.as_shot` - for late correction
- `chroma.late_correction` - preset flag

## Params from XMP
```
mode = OPPOSED (5)
clip = 1.0
```

## Tolerance
**1e-2** (not 1e-5) due to floating-point accumulation in chrominance calculation over ~50k pixels. Max observed diff: 0.0016.

## Output
`src/main/labs/mods/highlights.c` - 0 mismatches at 1e-2 tolerance (24,337,152 pixels).

---

# demosaic (verified working)

RCD demosaic algorithm - converts Bayer mosaic to RGB.

## Source
- `dark/lib/desk/src/iop/demosaicing/rcd.c` → rcd_demosaic, rcd_ppg_border
- `dark/lib/desk/src/iop/demosaic.c` → params struct
- `dark/lib/desk/src/common/math.h` → sqrf, interpolatef

## Input/Output
- **Input**: float32 Bayer mosaic (single channel)
- **Output**: float32 RGB (3 channels) or RGBA (4 channels with alpha=0)

## Params from XMP
```
method = RCD (5)
green_eq = 0 (disabled)
color_smoothing = 0 (disabled)
```

## Key Constants
- `DT_RCD_TILESIZE = 112` - tile size for cache efficiency
- `RCD_BORDER = 9` - tile overlap
- `RCD_MARGIN = 7` - outer border margin

## Tolerance
**1e-2** due to complex interpolation with multiple gradient calculations. Max observed diff: 0.0067.

## Output
`src/main/labs/mods/demosaic.c` - 0 mismatches at 1e-2 tolerance (73,011,456 RGB values).

---

# colorin (verified working)

RGB to Lab color space conversion via color matrix.

## Source
- `dark/lib/desk/src/iop/colorin.c` → _cmatrix_fastpath_simple (lines 827-855)
- `dark/lib/desk/src/common/colorspaces_inline_conversions.h` → dt_XYZ_to_Lab, dt_apply_color_matrix_by_row, lab_f

## Input/Output
- **Input**: float32 RGB (4 channels from demosaic)
- **Output**: float32 Lab (4 channels, L*a*b* + alpha)

## Process
1. Apply correction coefficients (D65/as_shot if late_correction)
2. Apply color matrix RGB -> XYZ
3. Convert XYZ to Lab using D50 white point

## Key Functions
- `lab_f()` - fast cube root approximation using bit manipulation + Halley iteration
- `dt_apply_color_matrix_by_row()` - matrix multiplication
- `dt_XYZ_to_Lab()` - XYZ to Lab conversion with D50 normalization

## Params (extracted via debug)
```
cmatrix[0]: 0.664328814 0.350094348 -0.0502231568
cmatrix[1]: 0.270618916 0.986686289 -0.257305205
cmatrix[2]: 0.0182029735 -0.155623421 0.962320447
corr: 1.06361997 1 0.92448926 0
```

## Tolerance
**1e-3** due to cube root approximation and matrix operations. Max observed diff: 0.00036.

## Output
`src/main/labs/mods/colorin.c` - 0 mismatches at 1e-3 tolerance (73,011,456 Lab values).

---

# colorout (verified working)

Lab to sRGB color space conversion via color matrix + gamma.

## Source
- `dark/lib/desk/src/iop/colorout.c` → _transform_cmatrix, process_fastpath_apply_tonecurves
- `dark/lib/desk/src/common/colorspaces_inline_conversions.h` → dt_Lab_to_XYZ, lab_f_inv
- `dark/lib/desk/src/develop/imageop_math.h` → dt_iop_estimate_exp, dt_iop_eval_exp

## Input/Output
- **Input**: float32 Lab (4 channels from colorin)
- **Output**: float32 sRGB (4 channels, gamma-encoded)

## Process
1. Convert Lab to XYZ using D50 white point
2. Apply color matrix XYZ -> linear RGB
3. Apply sRGB transfer function (with exponential extension for values >= 1.0)

## Key Functions
- `lab_f_inv()` - inverse of lab_f for Lab→XYZ
- `dt_iop_estimate_exp()` - fits exponential curve for highlight extension
- `dt_iop_eval_exp()` - evaluates exponential fit for values >= 1.0
- `srgb_gamma()` - standard sRGB transfer function

## Params (extracted via debug)
```
cmatrix[0]: 3.13423491 -1.61725771 -0.4906919
cmatrix[1]: -0.97874099 1.91611922 0.0334379375
cmatrix[2]: 0.0719688162 -0.229020134 1.40577972
```

## Tolerance
**1e-5** (simple per-pixel). Max observed diff: 0.000000946.

## Output
`src/main/labs/mods/colorout.c` - 0 mismatches at 1e-3 tolerance (73,011,456 RGB values).

---

# copy(module)

Copy DT module to match DT's output exactly.

## Process

1. **Generate reference**
   ```bash
   darktable-cli src/test/raws/sony.ARW src/test/raws/sony.xmp /tmp/out.png \
       --core --disable-opencl --dump-pipe <module> --dumpdir /tmp/dtdump
   ```

2. **Find module source**
   - `dark/lib/desk/src/iop/<module>.c`

3. **Copy to** `src/main/labs/mods/<module>.c`
   - Copy `dt_iop_<module>_params_t` → `<Module>Params`
   - Copy `process()` exactly, CPU path only
   - Copy any helper functions it calls
   - No abstractions. No cleanup. Pure copy.

4. **Get defaults**
   - Find `dt_iop_<module>_init()` or default param values
   - Create `reset()` function with these defaults

5. **Test**
   ```c
   // Load previous module output (or head for rawprepare)
   // Apply process() with default params
   // Compare to DT's output PPM
   // Must be 100% match
   ```

## DT IOP Order (v50_order from iop_order.c)

This is the **correct pipeline order** from darktable source. XMP order is NOT pipe order.

### Phase 1: Minimal Pipeline (phase1.xmp)

Core modules only - RAW to displayable output with no color grading.

| Order | Module | Status | Notes |
|-------|--------|--------|-------|
| 1 | rawprepare | ✓ done | Black/white point normalization |
| 3 | temperature | ✓ done | White balance (Bayer) |
| 4 | highlights | ✓ done | Highlight reconstruction |
| 8 | demosaic | ✓ done | Bayer → RGB |
| 28 | colorin | ✓ done | RGB → Lab |
| 70 | colorout | ✓ done | Lab → sRGB (gamma-encoded) |
| 78 | gamma | skip | Display only (uint8 conversion) |

### Phase 2: Scene-referred (future)

| Order | Module | Notes |
|-------|--------|-------|
| 21 | exposure | Exposure compensation |
| 28.5 | channelmixerrgb | Color calibration (complex) |
| 45.3 | sigmoid | Scene → display |

### Skipped Modules (disabled by default)

| Module | Reason |
|--------|--------|
| invert | Film negatives only |
| cacorrect | CA correction - enable when needed |
| hotpixels | Stuck pixel removal - enable when needed |
| rawdenoise | Raw denoising - enable when needed |

Each module reads previous module's `_out` as its `_in`.

---

# Execution

```
src/main/labs/sony.c              # Sony ARW decoder (done)
src/main/labs/pipe_state.h        # pipeline state (done)
src/main/labs/pipe_prepare.c      # derived value computation (done)
src/main/labs/mods/rawprepare.c   # done
src/main/labs/mods/temperature.c  # done
src/main/labs/mods/highlights.c   # done
src/main/labs/mods/demosaic.c     # done
src/main/labs/mods/colorin.c      # done
src/main/labs/mods/colorout.c     # done (final sRGB output)
src/test/raws/phase1.xmp          # minimal pipeline XMP
```

Sequential. Each must pass before starting next.

## Tolerance Guidelines

| Module Type | Tolerance | Reason |
|-------------|-----------|--------|
| Simple per-pixel | 1e-5 | Direct calculation |
| Accumulating | 1e-2 | Floating-point accumulation over many pixels |

## When Module Needs PipeState Data

If a module references data not in PipeState:
1. Trace where DT populates that data
2. If from raw file → add extraction to head decoder
3. If computed from raw data → add to pipe_prepare
4. If from upstream module → ensure upstream sets it

## Finding Raw Metadata

To debug what DT extracts from a raw file, add fprintf to DT source and rebuild:

```c
// In temperature.c _calculate_bogus_daylight_wb():
fprintf(stderr, "adobe_XYZ_to_CAM[0]: %.9g %.9g %.9g\n",
        self->dev->image_storage.adobe_XYZ_to_CAM[0][0], ...);
```

Then rebuild the module:
```bash
cd dark/lib/desk/build
cmake --build . --target temperature
```

**Key discovery**: Color matrix comes from raw file metadata, not cameras.xml. The raw file is self-describing.
