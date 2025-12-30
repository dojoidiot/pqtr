# Transpiler

**COPY, don't think.** Read DT source, copy to C++. If you're interpreting, redesigning, or "improving" - you're failing. The goal is exact behavioral match with darktable, verified by diff.

**Forbidden:**
- Adding error handling DT doesn't have
- Creating abstractions or helpers
- "Cleaning up" DT's approach
- Inventing when stuck (read more DT code instead)

**When copying:**
- Follow CPU path only (not OpenCL/SSE/intrinsics)
- Inline DT helper functions, don't create separate files
- Expand DT macros (FC, CLAMP, FOR_EACH_CHANNEL) to plain C++
- Replace dt_* types with std:: equivalents
- Line-by-line correspondence - your code should read like DT source with type substitutions

**Why exact copy:** This is a cleanroom reimplementation. Legal isolation requires behavioral exactness. diff MUST pass - this is not optional.

**Execution order:** main{} defines the step order. Complete each module fully (diff passes) before starting the next. No skipping. No parallelizing. Sequential.

---

# Phase 1: Copy

Pure DT transpile. No framework. Binary data + hardcoded defaults.

**External dependencies:**
- darktable source: `dark/lib/desk/src/iop/`
- darktable CLI: `dark/lib/desk/build/bin/darktable-cli`
- Test RAW files: user provides `src/test/raws/*.ARW`

**Execution order:** main{} defines sequential order. Each step must pass before the next. head passes before copy by definition.

## Output Convention (Phase 1)

```
src/main/labs/mods/<module>.cpp       // cleanroom DT copy: params + reset() + process()
src/test/labs/mods/<module>_test.cpp  // test: load binary → run → diff
src/test/dark/<module>_out.pfm        // DT reference output (PFM from --dump-pipe)
```

- `mods/<module>.cpp` - pure C++, NO framework, only std:: types
- Test reads binary, runs module, writes binary, calls diff CLI

## Reference Generation

Generate DT reference binaries using `--dump-pipe`:

```bash
# Run darktable-cli with dump-pipe for target module
# IMPORTANT: --disable-opencl forces CPU path (we only copy CPU code, not OpenCL)
./dark/lib/desk/build/bin/darktable-cli \
    src/test/raws/sony.ARW \
    src/test/raws/sony.xmp \
    /tmp/out.png \
    --core --disable-opencl --dump-pipe <module> --dumpdir /tmp/dtdump

# Output: /tmp/dtdump/export/NNNN_<module>_GPU_in_M.ppm   (input)
#         /tmp/dtdump/export/NNNN_<module>_GPU_out_M.pfm  (output)

# Copy PFM directly - diff auto-strips PFM headers
cp /tmp/dtdump/export/*_<module>_*_out_M.pfm src/test/dark/<module>_out.pfm
```

**Test XMP:** `src/test/raws/sony.xmp` - reference pipeline configuration created with darktable desktop (sony raw, sony style, autumn effect). Enables maximum modules for comprehensive testing.

**Reference file format:**
- float32, row-major order
- Pre-demosaic: 1 float per pixel (width * height)
- Post-demosaic: 4 floats per pixel RGBA (width * height * 4)

## Diff Tool

```bash
./diff <reference> <output> --bits|--bayer|--rgb|--lab|--display
```

Auto-detects and strips PFM headers - can compare .pfm files directly.

| Flag       | Threshold | Use for                            |
|------------|-----------|-------------------------------------|
| --bits     | exact     | head output (u16 bayer), raw data  |
| --bayer    | 1e-6      | rawprepare, temperature, highlights |
| --rgb      | 0.01 ΔE   | demosaic through colorout          |
| --lab      | 0.01 ΔE   | Lab colorspace modules             |
| --display  | 1.0 ΔE    | gamma, sigmoid, filmicrgb          |

## Module Colorspace Table

| Module          | Input      | Output     | Diff flag  |
|-----------------|------------|------------|------------|
| rawprepare      | bayer u16  | bayer f32  | --bayer    |
| temperature     | bayer f32  | bayer f32  | --bayer    |
| highlights      | bayer f32  | bayer f32  | --bayer    |
| demosaic        | bayer f32  | RGBA f32   | --rgb      |
| exposure        | RGBA f32   | RGBA f32   | --rgb      |
| channelmixer    | RGBA f32   | RGBA f32   | --rgb      |
| channelmixerrgb | RGBA f32   | RGBA f32   | --rgb      |
| colorbalancergb | RGBA f32   | RGBA f32   | --rgb      |
| colorin         | RGBA f32   | Lab f32    | --lab      |
| bilat           | Lab f32    | Lab f32    | --lab      |
| colorout        | Lab f32    | RGBA f32   | --rgb      |
| gamma           | RGBA f32   | RGBA f32   | --display  |
| sigmoid         | RGBA f32   | RGBA f32   | --display  |
| filmicrgb       | RGBA f32   | RGBA f32   | --display  |
| flip            | RGBA f32   | RGBA f32   | --rgb      |


# Mapping Rules (DT → Tree)

When transpiling, map darktable pipeline state to Tree lookups:

| darktable code                        | C++ equivalent                                      |
|---------------------------------------|-----------------------------------------------------|
| `piece->pipe->dsc.filters`            | `(uint32_t)flow.head().leaf("filters").dial()`      |
| `roi_in->width`, `roi_out->width`     | `(int)flow.head().leaf("width").dial()`             |
| `roi_in->height`, `roi_out->height`   | `(int)flow.head().leaf("height").dial()`            |
| `piece->pipe->dsc.temperature.coeffs` | `flow.head().next("wb").leaf("r").dial()` etc       |
| `FC(row, col, filters)`               | inline: `(filters >> (((row<<1&14)+(col&1))<<1))&3` |


## function copy(module)

    READ THE PREAMBLE FIRST. Copy, don't think. Line-by-line. diff MUST pass.

    Step 0. Ensure reference exists:
        - `src/test/dark/<module>_out.pfm` (DT reference output)
        - If missing, generate per Reference Generation section

    Step 1. Create `src/main/labs/mods/<module>.cpp`:
        - struct <Module>Params { ... } - copy exactly from dt_iop_<module>_params_t
        - Use std:: types only (float, int, std::array, std::vector)
        - NO framework headers, NO pqtr:: types
        - Add: void reset() with hardcoded DT defaults

    Step 2. Add process() - copy DT's process():
        - Read DT's process(), follow FULL/CPU path only
        - Transpile to standalone C++ using std:: types
        - Signature: void process(float* in, float* out, int width, int height, const <Module>Params& p)
        - Data in, data out - copy what DT does

    Step 3. Create test `src/test/labs/mods/<module>_test.cpp`:
        - Load input from previous module (or head for rawprepare)
        - Call reset() to get default params
        - Run process()
        - Write output to /tmp/<module>_out.bin
        - Call: ./diff src/test/dark/<module>_out.pfm /tmp/<module>_out.bin <flag>
        - Exit 0 if pass, exit 1 if fail

    Step 4. Add to Makefile, run `make test-<module>` until PASS

## function head(camera, fileType)

    head output MUST match DT's rawprepare input exactly.

    DT uses RawSpeed for most formats (ARW, CR2, NEF, etc.), LibRaw only for CR3.
    RawSpeed source: dark/lib/desk/src/external/rawspeed/src/librawspeed/decoders/

    **Cleanroom approach:** Read RawSpeed to understand, reimplement without copying GPL.

    **Head debugging lessons:**
    - When diff shows all values at extremes (0 or max), detection/format is wrong
    - RawSpeed has hidden special cases (multi-IFD overrides, dimension constraints)
    - Trace actual execution path, not just documented algorithms
    - Check dimension constraints that throw exceptions (they might reject your file)
    - LibRaw patches remove limits from some decoders but not all

    ### Sony ARW (RawSpeed ArwDecoder.cpp)

    Compression types (TIFF tag 259):
    - compression=1: uncompressed 16-bit, little-endian, BitOrder::LSB
    - compression=7: tiled LJPEG (modern cameras like A7)
    - compression=32767: ARW1/ARW2 proprietary (older cameras)

    **ARW1 vs ARW2 detection (compression=32767):**
    ```cpp
    arw1 = (strip_byte_count * 8) != (width * height * bits_per_sample)
    ```
    BUT: ArwDecoder.cpp lines 216-224 override bits_per_sample to 8 when
    multiple IFDs have MAKE="SONY" (no trailing space). A7 III has 2 such IFDs.
    With bpp=8: `(24337152 * 8) == (6048 * 4024 * 8)` → arw1=FALSE → ARW2.

    **Lesson learned:** Don't trust formulas alone. Trace RawSpeed's actual
    code path. When diff shows clamped extremes (0/8190), detection is wrong.
    SonyArw1Decompressor also has `h > 3072` constraint that throws for A7 III.

    Key differences from LibRaw:
    - RawSpeed reads uncropped dimensions from TIFF
    - Applies curve from SONYCURVE tag
    - Crops after decode using SONYRAWIMAGESIZE
    - Black/white from encrypted Sony metadata block

    ### Canon CR2 (RawSpeed Cr2Decoder.cpp)

    DT uses RawSpeed for CR2, LibRaw only for CR3 (Canon R-series mirrorless).

    **Format structure:**
    - TIFF container with 4 IFDs (modern) or fewer (old 1D/1DS/D2000)
    - IFD[3] contains raw data as Lossless JPEG (LJPEG)
    - CANONCR2SLICE tag defines slice layout: (numSlices, sliceWidth, lastSliceWidth)
    - Decoder must reassemble slices into final image

    **ColorData versioning (MakerNotes):**
    - Detected by count (582→v1, 653→v2) or version field (1-15)
    - Each version has different offsets for WB and black/white levels:
      - v1: WB@50, no black/white
      - v2: WB@68, no black/white
      - v3-v4: WB@126, black/white varies by sub-version
    - MakerNotes are always 14-bit; scale when LJPEG precision differs

    **Gotchas:**
    - Double-height fix: Some cameras (5Ds) encode doubled width, halved height
    - Old format (< 4 IFDs): Uses CANON_RAW_DATA_OFFSET instead of IFD[3]
    - Slice width inconsistency in sRaw mode requires 3/2 correction

    Step 0. Generate DT reference for head:
        ```bash
        ./dark/lib/desk/build/bin/darktable-cli \
            src/test/raws/<camera>.<fileType> \
            src/test/raws/sony.xmp \
            /tmp/out.png \
            --core --disable-opencl --dump-pipe rawprepare --dumpdir /tmp/dtdump

        # DT's rawprepare INPUT is our head reference:
        cp /tmp/dtdump/export/0000_rawprepare_cpu_in_M.ppm src/test/dark/head_<camera>.ppm
        ```

    Step 1. Read RawSpeed decoder for camera format:
        - ArwDecoder.cpp for Sony ARW
        - Cr2Decoder.cpp for Canon CR2
        - Understand compression type and data layout

    Step 2. Implement cleanroom decoder:
        - Parse TIFF structure to find raw data
        - Implement appropriate decompressor (LJPEG, uncompressed, etc.)
        - Match RawSpeed's crop/dimension handling
        - Output: `head_<camera>_bayer.bin` (u16 bayer matching DT)
        - Output: `head_<camera>.json` (metadata)

    Step 3. Test:
        - Load RAW file, run head decoder
        - Compare to DT reference PPM (strip 13-byte PPM header)
        - diff --bits MUST pass

---

# Phase 2: Pipe (later)

After all modules pass Phase 1, integrate into pqtr framework.

## JSON Params Format

Modules read params from JSON/XMP. Step adapter loads JSON, passes to module.

```json
{
  "width": 6000,
  "height": 4000,
  "black": 512,
  "white": 16383,
  "filters": 2492263094,
  "wb": { "r": 2.1, "g1": 1.0, "g2": 1.0, "b": 1.5 },
  "<module>": {
    "param1": 1.0,
    "param2": 0.5
  }
}
```

Add `void from_json(const std::string& json)` to each module in Phase 2.

## Output Convention (Phase 2)

```
src/main/labs/step/<Module>Step.hpp   // adapter: pqtr::Step → mods/<module>.cpp
```

## function pipe(module)

    Step 1. Create `src/main/labs/step/<Module>Step.hpp`:
        - #include pqtr.hpp and mods/<module>.cpp
        - class <Module>Step : public pqtr::Step
        - exec(Flow& flow):
            a. If flow.flow().next("<module>") exists, read params from it
            b. Else populate params from flow.head() defaults
            c. Write params to flow.flow().next("<module>")
            d. Call process()
            e. Update flow.flow() if dimensions changed

    Step 2. Register in pipeline, test end-to-end

---

# main

```
LibRaw source: <copy.md location>/LibRaw
module source: <copy.md location>/dark/lib/desk/src/iop/
raws source:   <copy.md location>/src/test/raws
mods output:   <copy.md location>/src/main/labs/mods/
diff tool:     <copy.md location>/src/main/labs/diff.cpp
dark tool:     <copy.md location>/src/main/labs/dark.cpp

class = CamelCase(module)  // exposure -> Exposure

# Phase 1: Copy (current)
do head => sony ARW
do head => canon cr2
do copy => rawprepare
do copy => temperature
do copy => highlights
do copy => demosaic
do copy => exposure
do copy => channelmixer
do copy => channelmixerrgb
do copy => colorbalancergb
do copy => colorin
do copy => bilat
do copy => colorout
do copy => gamma
do copy => sigmoid
do copy => filmicrgb
do copy => flip

# Phase 2: Pipe (after all copy passes)
do pipe => all modules
```
