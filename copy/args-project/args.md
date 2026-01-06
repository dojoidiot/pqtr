# Args - Sony ARW RAW Processor

Minimal RAW processing pipeline for Sony ARW files.

## Pipeline Module Dependency Chain

```
                              ┌─────────────────────────────────────────────────┐
                              │              METADATA EXTRACTION                │
                              │                   (sony.c)                      │
                              │                                                 │
                              │  RAW file → Parse TIFF/EXIF → Extract:          │
                              │    • White balance coefficients                 │
                              │    • Color matrix (D65)                         │
                              │    • Black/white levels                         │
                              │    • ISO, DRO, Picture Profile                  │
                              │    • CFA pattern                                │
                              │    • Preview JPEG offset/length                 │
                              └───────────────────┬─────────────────────────────┘
                                                  │
                                                  ▼
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                    PIPELINE STAGES                                      │
├─────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                         │
│  SENSOR DOMAIN (Bayer)                                                                  │
│  ─────────────────────                                                                  │
│                                                                                         │
│  1. black.c ──────────► Subtract black level, scale to white                            │
│     Input:  bayer_u16 (raw sensor data)                                                 │
│     Output: bayer_f32 (normalized 0-1)                                                  │
│     Deps:   black_level, white_level from metadata                                      │
│                   │                                                                     │
│                   ▼                                                                     │
│  2. temperature.c ──► Apply white balance coefficients                                  │
│     Input:  bayer_f32                                                                   │
│     Output: bayer_wb (white balanced bayer)                                             │
│     Deps:   wb_coeffs from metadata                                                     │
│     State:  Stores wb_coeffs in PipeState for highlights                                │
│                   │                                                                     │
│                   ▼                                                                     │
│  3. highlights.c ───► Reconstruct clipped highlights                                    │
│     Input:  bayer_wb                                                                    │
│     Output: bayer_hl (highlight-recovered bayer)                                        │
│     Deps:   temperature coefficients from PipeState                                     │
│     Method: LCh-based reconstruction using channel ratios                               │
│                   │                                                                     │
│                   ▼                                                                     │
│  4. demosaic.c ─────► Convert Bayer to RGB (AMaZE algorithm)                            │
│     Input:  bayer_hl (single channel per pixel)                                         │
│     Output: rgb_linear (3 channels per pixel)                                           │
│     Deps:   CFA pattern, filters array from PipeState                                   │
│                                                                                         │
├─────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                         │
│  CAMERA RGB → SCENE (Rec2020)                                                           │
│  ────────────────────────────                                                           │
│                                                                                         │
│  5. colorin.c ──────► Camera RGB to Rec2020 (scene-referred)                            │
│     Input:  rgb_linear (camera color space)                                             │
│     Output: rec2020_linear                                                              │
│     Deps:   color_matrix from metadata                                                  │
│                   │                                                                     │
│                   ▼                                                                     │
│  6. exposure.c ─────► Apply exposure compensation                                       │
│     Input:  rec2020_linear                                                              │
│     Output: rec2020_exposed                                                             │
│     Deps:   ISO, DRO level, camera style exposure                                       │
│                   │                                                                     │
│                   ▼                                                                     │
│  7. denoise.c ──────► Profiled wavelet denoising                                        │
│     Input:  rec2020_exposed                                                             │
│     Output: rec2020_denoised                                                            │
│     Deps:   ISO (for noise profile selection)                                           │
│                                                                                         │
├─────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                         │
│  CREATIVE / STYLE                                                                       │
│  ────────────────                                                                       │
│                                                                                         │
│  8. vibrance.c ─────► Saturation boost for less-saturated colors                        │
│     Input:  rec2020_denoised                                                            │
│     Output: rec2020_vibrant                                                             │
│     Deps:   Picture Profile vibrance setting                                            │
│                   │                                                                     │
│                   ▼                                                                     │
│  9. saturation.c ───► Global saturation adjustment                                      │
│     Input:  rec2020_vibrant                                                             │
│     Output: rec2020_saturated                                                           │
│     Deps:   Picture Profile saturation setting                                          │
│                                                                                         │
├─────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                         │
│  SCENE → DISPLAY                                                                        │
│  ──────────────                                                                         │
│                                                                                         │
│  10. filmic.c ──────► Scene-referred to display-referred (S-curve)                      │
│      Input:  rec2020_saturated (scene linear, unbounded)                                │
│      Output: rec2020_display (0-1 range, display-referred)                              │
│      Deps:   Filmic parameters (grey, black_ev, white_ev)                               │
│                   │                                                                     │
│                   ▼                                                                     │
│  11. sigmoid.c ─────► Soft highlight/shadow rolloff                                     │
│      Input:  rec2020_display                                                            │
│      Output: rec2020_sigmoid                                                            │
│      Deps:   Contrast parameter                                                         │
│                                                                                         │
├─────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                         │
│  OUTPUT ENCODING                                                                        │
│  ───────────────                                                                        │
│                                                                                         │
│  12. colorout.c ────► Rec2020 to sRGB color space conversion                            │
│      Input:  rec2020_sigmoid                                                            │
│      Output: srgb_linear                                                                │
│      Deps:   Output color matrix                                                        │
│                   │                                                                     │
│                   ▼                                                                     │
│  13. gamma.c ───────► Apply sRGB gamma curve                                            │
│      Input:  srgb_linear                                                                │
│      Output: srgb_gamma (0-1)                                                           │
│                   │                                                                     │
│                   ▼                                                                     │
│  14. output.c ──────► Quantize to 8-bit PNG                                             │
│      Input:  srgb_gamma (float 0-1)                                                     │
│      Output: srgb_u8 (uint8 0-255)                                                      │
│                                                                                         │
└─────────────────────────────────────────────────────────────────────────────────────────┘

Data Types:
  • bayer_*: Single channel per pixel (RGGB mosaic)
  • rgb_*/rec2020_*/srgb_*: 3 channels per pixel (RGB)
  • _u16: 16-bit unsigned integer
  • _f32: 32-bit float
  • _linear: Linear light values
  • _gamma: Gamma-encoded values
```

## PipeState Dependencies

The `PipeState` struct carries inter-module state:

| Field | Set By | Used By |
|-------|--------|---------|
| `wb_coeffs[3]` | temperature.c | highlights.c |
| `filters` | main.c (from metadata) | demosaic.c |
| `width`, `height` | main.c | all modules |

## External Dependencies

- **libjpeg-turbo**: JPEG decoding for autotune
- **stb_image.h**: Header-only JPEG loading (bundled)
- **stb_image_write.h**: Header-only PNG writing (bundled)
- **exiftool** (optional): For Sony encrypted tags (Picture Profile, DRO)
  - If not available, defaults to neutral settings
