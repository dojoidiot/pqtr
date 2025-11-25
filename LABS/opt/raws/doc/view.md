# VIEW: Camera Look Extraction

[back](../README.md)

Extract display-referred rendering parameters from Sony ARW metadata and map to pipe module dials, producing output that matches the camera's embedded JPEG.

---

## Goal

Given a Sony ARW file, deterministically produce pipe dial settings that recreate the camera's in-body rendering. No optimization at runtime - pure metadata extraction and lookup.

---

## Architecture

```
ARW file ──┬──► raws (decoder) ──► scene-referred linear RGB ──► pipe ──► output PNG
           │                                                        ▲
           └──► VIEW ──► extract metadata ──► lookup table ──► dial settings
                              │
                              ▼
                    embedded JPEG ──► diff ──► validate (spectral loss ≈ 0)
```

**Separation preserved:**
- raws: scene-referred only (no display knowledge)
- pipe: display-referred processing (golden modules)
- VIEW: bridge between camera metadata and pipe dials

---

## Sony Metadata Available

From ARW MakerNotes (ILCE-7M3):

| Tag | Example Value | Meaning |
|-----|---------------|---------|
| Creative Style | Standard | Look preset (Standard, Vivid, Portrait, etc.) |
| Dynamic Range Optimizer | Auto | Shadow recovery level |
| Contrast | 0 (Normal) | -3 to +3 adjustment |
| Saturation | 0 (Normal) | -3 to +3 adjustment |
| Sharpness | 0 (Normal) | -3 to +3 adjustment |
| Sony Tone Curve | 8000 10400 12900 14100 | 4-point curve data |
| WB RGGB Levels | 2420 1024 1024 1616 | White balance multipliers |
| Exposure Compensation | 0 | EV shift |

---

## Pipe Modules to Map

| Module | Dials | Sony Source |
|--------|-------|-------------|
| color_correction | 3 (exposure, temperature, tint) | Exposure Comp, WB RGGB |
| tone_mapping | 5 (contrast, highlights, shadows, white_point, black_point) | Sony Tone Curve, DRO, Contrast |
| global_color | 3 (vibrance, saturation, color_density) | Saturation, Creative Style |
| detail_output | 4 (sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma) | Sharpness |

**Total: 15 dials** (geometric and selective_color not used for camera matching)

---

## Strategy

The embedded JPEG is just another reference image. Use tune.

```
ARW ──► exiftool -b -PreviewImage ──► reference.jpg (1616x1080)
    └──► raws ──► pipe ──► tune (against reference.jpg) ──► dials
```

No special "camera look extraction" logic needed. Tune handles it.

### Optional: Cache Results

If the same Creative Style + DRO combo is used repeatedly, cache the tuned dials as a preset. But this is optimization, not core functionality.

---

## Lookup Table Structure

```
tmp/view/
├── calibration/
│   ├── a7iii_standard_dro_auto.json    ← calibrated dials
│   ├── a7iii_standard_dro_off.json
│   ├── a7iii_vivid_dro_auto.json
│   └── ...
└── output/
    └── DSC00144.view.json              ← extracted dial settings
```

### Calibration Entry Format

```json
{
  "camera": "ILCE-7M3",
  "creative_style": "Standard",
  "dro": "Auto",
  "calibration_date": "2025-11-26",
  "calibration_images": ["DSC00144.ARW", "DSC00159.ARW"],
  "spectral_loss": 0.0023,
  "dials": {
    "color_correction": {
      "exposure": 0.5,
      "temperature": 0.5,
      "tint": 0.5
    },
    "tone_mapping": {
      "contrast": 0.52,
      "highlights": 0.48,
      "shadows": 0.45,
      "white_point": 0.5,
      "black_point": 0.5
    },
    "global_color": {
      "vibrance": 0.5,
      "saturation": 0.53,
      "color_density": 0.5
    },
    "detail_output": {
      "sharpen_amount": 0.5,
      "sharpen_radius": 0.5,
      "denoise_luma": 0.5,
      "denoise_chroma": 0.5
    }
  }
}
```

---

## WB Extraction

Sony provides RGGB multipliers. Convert to temperature/tint:

```
WB RGGB: 2420 1024 1024 1616
Normalized: R=2.36, G=1.0, B=1.58

R/B ratio → color temperature (lookup or approximation)
G offset from average → tint (green-magenta)
```

This is a known conversion - darktable and others implement it.

---

## Offset Mapping

Sony's -3 to +3 adjustments map to dial offsets:

| Sony Value | Dial Offset |
|------------|-------------|
| -3 | -0.15 |
| -2 | -0.10 |
| -1 | -0.05 |
| 0 | 0.00 |
| +1 | +0.05 |
| +2 | +0.10 |
| +3 | +0.15 |

Applied to:
- Contrast → tone_mapping.contrast
- Saturation → global_color.saturation
- Sharpness → detail_output.sharpen_amount

---

## Validation

After applying VIEW-extracted dials:

1. Process ARW through pipe with extracted settings
2. Compare output to embedded JPEG using diff
3. Spectral loss should be < 0.01 (near-identical color/tone)
4. Frequency loss may differ (resolution/sharpening differences)

---

## Implementation Steps

1. **view_extract**: Read ARW, extract embedded JPEG + metadata
2. **view_calibrate**: Run SPSA to find base dials for a Creative Style
3. **view_apply**: Look up calibration + apply offsets → dial settings
4. **view_validate**: Compare pipe output to embedded JPEG

---

## Analysis

### Sony Tone Curve (8000 10400 12900 14100)

Tested across 10 ARW files - **all identical**. This confirms the values are tied to Creative Style, not scene content.

The 4 values are 14-bit (max 16383). Likely interpretation:
- **Input breakpoints** at fixed percentages (shadows, midtones-low, midtones-high, highlights)
- **Output targets** defining the transfer curve shape

Second tag **0x7011** (4000 7200 10050 12075) is also fixed - possibly a related curve or gamma parameter.

**Conclusion**: These can be decoded once per Creative Style, not per-image.

### DRO Auto Behavior

DRO Auto shows as "Auto" in metadata - no per-image parameters stored. The camera analyzes the scene and applies adjustments, but doesn't record what it did.

**Implications**:
- DRO Auto output varies per-image (scene-dependent)
- But Creative Style + DRO Auto produces a *family* of looks, not one exact look
- Calibration against multiple images should capture the "average" of this family
- For exact matching, DRO Off would be more predictable

**Conclusion**: Start with DRO Auto (what the test images have). Accept some variation. Can add DRO Off calibration later for tighter matching.

### Resolution Scaling

Embedded JPEG is 1616x1080. Spectral loss compares color distributions, not pixel correspondence - should remain valid across resolutions. Frequency loss (sharpness) won't transfer directly, but we're targeting color/tone matching primarily.

**Conclusion**: Proceed with embedded JPEG for calibration. Validate empirically.

---

## Scope: Lab Rat

Starting minimal with what we have:
- **Camera**: ILCE-7M3 (A7III)
- **Creative Style**: Standard only
- **DRO**: Auto (as-shot)
- **Adjustments**: Contrast 0, Saturation 0, Sharpness 0

One calibration target. Expand later.

See [todo.md](./todo.md) for calibration image requirements.

---

## Files

```
opt/raws/
├── doc/
│   ├── sony.md         ← decoder documentation
│   └── view.md         ← this file
├── src/
│   └── main/part/
│       └── view/       ← VIEW implementation (future)
│           ├── extract.cpp
│           ├── calibrate.cpp
│           └── apply.cpp
└── tmp/
    └── view/           ← calibration data and outputs
```
