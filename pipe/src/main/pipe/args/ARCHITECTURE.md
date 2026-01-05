# Module Architecture

## Data Flow

```
Raw File → Head Decoder → PipeState (image metadata)
                              ↓
                        Module Chain
                              ↓
                         Tail Encoder
```

## Three Categories of Data

### 1. PipeState (pipe_state.h)
**Image-specific metadata** that flows through the pipeline.
- Populated by the head decoder from raw file headers
- Enriched by pipe_prepare from camera database
- Read-only to modules (except designated output fields)

Contains:
- `width`, `height`, `filters` - image geometry and Bayer pattern
- `adobe_XYZ_to_CAM` - color matrix for this camera model
- `exposure_bias` - camera-specific EV adjustment
- `temperature.coeffs` - white balance (set by temperature module)
- `chroma.*` - chromatic adaptation data

**Key principle**: If a value changes per-image, it belongs in PipeState.

### 2. Module Params (algorithm settings)
**User-tunable parameters** that control module behavior.
- Stored in XMP sidecar files
- Same values can be applied to different images
- Have sensible defaults from DT's code

Examples:
- `demosaicing_method = 5` (RCD algorithm)
- `mode = OPPOSED` (highlight recovery method)
- `contrast = 1.5` (filmic curve shape)
- `exposure = 0.0` (EV adjustment, 0 = no change)

**Key principle**: If it's an algorithm choice independent of the image, it's a Param.

### 3. Module Data (runtime computed)
**Computed values** derived from Params + PipeState.
- Created in `commit_params()` before processing
- May combine user settings with image metadata
- Not directly settable by user

Examples:
- `RawprepareData.sub[]` = computed from raw black levels
- `TemperatureData.coeffs[]` = from as-shot WB in raw header
- `FilmicRGBData.spline.*` = computed from contrast/latitude params

## Module Classification

### Metadata-driven (no sensible defaults)
These modules derive all runtime data from PipeState. They cannot have
meaningful `_defaults()` functions.

| Module | Why |
|--------|-----|
| rawprepare | black/white levels from raw header |
| temperature | WB coefficients from as-shot metadata |

### Algorithm-driven (have defaults)
These modules have algorithm parameters with DT-defined defaults.
They provide `module_defaults()` functions.

| Module | Default behavior |
|--------|------------------|
| highlights | OPPOSED mode, clip=1.0 |
| demosaic | RCD method |
| exposure | 0 EV (passthrough) |
| colorin | Enhanced matrix, Rec2020 working space |
| channelmixer | D65 illuminant, identity mix |
| colorbalance | Neutral grading |
| filmic | V5 tone mapping curve |
| bilat | Local laplacian, subtle contrast |
| colorout | sRGB output |

### Mixed modules
Some modules need both PipeState and algorithm params:
- `highlights` - mode is param, but clip threshold may use WB data
- `channelmixer` - illuminant can come from metadata or be overridden

## Error Handling

If a module requires PipeState data that wasn't populated:
- This is a **pipeline error**, not a module error
- The head decoder or pipe_prepare failed to provide required metadata
- Modules should not provide fallback "Sony values" or similar

## Adding New Modules

1. Identify which values are image metadata vs algorithm params
2. Add image metadata fields to PipeState if needed
3. Create Params struct for algorithm settings only
4. Create Data struct for runtime values (Params + PipeState)
5. Implement `_defaults()` only if module has algorithm params
6. Implement `_process()` taking Data struct
