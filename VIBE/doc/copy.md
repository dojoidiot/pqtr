# COPY: Darktable → Vibe Mapping

This document defines the mapping rules from darktable XMP sidecar modules to the `vibe.hpp` model.

**RULES for the LLM**

You can change the type of a vibe.hpp property to eliminate conversion, however YOU CAN NEVER CHANGE THE header without approval.

## Pipeline Comparison

### Darktable Pipeline (Scene-Referred)
```
rawprepare → demosaic → temperature → highlights → channelmixerrgb →
exposure → sigmoid/filmicrgb → colorbalancergb → bilat → colorout → gamma
```

### Vibe Pipeline
```
ColorCorrection (Exposure, WB) → BaseCurve/PolyColor/LutCurve →
ToneMapping → GlobalColor → SplitTone → SelectiveColour → Detail →
[Linear→sRGB] → DisplayToneCurve
```

## Module Mappings

### 1. HEAD Stage (Not VIBE)

These modules are handled by the HEAD stage before VIBE:

| Darktable Module | HEAD Stage | Notes |
|-----------------|------------|-------|
| `rawprepare` | BLC | Black level, white point, crop |
| `demosaic` | Demosaic | Bayer interpolation |
| `temperature` | WB (partial) | Camera WB coefficients applied |
| `colorin` | CST | Color space transform to working space |
| `flip` | Warp | Rotation, handled with lens distortion |

### 2. ColorCorrection Stage

| Darktable Module | Vibe Target | Param Decoding |
|-----------------|-------------|----------------|
| `exposure` | `ColorCorrection::Exposure` | `params[8..11]` = EV (float LE) |
| `temperature` | `ColorCorrection::WhiteBalance` | See temperature decoding below |

**Temperature Decoding:**
```
params[0..3]   = R coefficient (float LE)
params[4..7]   = G coefficient (float LE)
params[8..11]  = B coefficient (float LE)
params[12..15] = unused
params[16..19] = preset (int32)

Approximate Kelvin: temp = 5500 * (R / B)
Tint: G coefficient (green/magenta shift)
```

### 3. ToneMapping Stage

| Darktable Module | Vibe Target | Notes |
|-----------------|-------------|-------|
| `sigmoid` | `ToneMapping::Contrast`, `ToneMapping::Skew` | Primary tone mapper |
| `filmicrgb` | `ToneMapping` (full) | Alternative tone mapper |
| `basecurve` | `BaseCurve` | Legacy display-referred |

**Sigmoid Decoding:**
```
params[0..3]   = contrast (float LE, default 1.5) → ToneMapping::Contrast
params[4..7]   = skew (float LE, default 0.0)     → ToneMapping::Skew
params[8..11]  = display_max (100 nits)
params[12..15] = primaries rotation
...
```

**Filmic RGB Decoding:** (gzip compressed)
```
After decompression:
- grey_point_source (18.45% default)  → ToneMapping::GreyPoint
- black_point_source (EV below grey)  → ToneMapping::ClippingPoint::black
- white_point_source (EV above grey)  → ToneMapping::ClippingPoint::white
- contrast                            → ToneMapping::Contrast
- latitude                            → (affects curve shape)
- balance (shadows/highlights)        → ToneMapping::CurveAdjustment
- saturation                          → GlobalColor::Saturation
```

### 4. GlobalColor Stage

| Darktable Module | Vibe Target | Notes |
|-----------------|-------------|-------|
| `colorbalancergb` | `GlobalColor`, `SplitTone` | Complex mapping |
| `channelmixerrgb` | (chromatic adaptation) | Usually auto |

**ColorBalanceRGB Decoding:** (gzip compressed)
```
After decompression:
- shadows_C (chroma)
- shadows_H (hue)
- midtones_C, midtones_H
- highlights_C, highlights_H
- global saturation
- global vibrance
- contrast
```

Maps to:
- `GlobalColor::Saturation` ← global saturation
- `GlobalColor::Vibrance` ← global vibrance
- `SplitTone::shadows` ← shadows_C, shadows_H → temp/tint
- `SplitTone::highlights` ← highlights_C, highlights_H → temp/tint

### 5. Detail Stage

| Darktable Module | Vibe Target | Notes |
|-----------------|-------------|-------|
| `bilat` | `Detail::LocalContrast` | Bilateral filter / local contrast |
| `sharpen` | `Detail::Sharpen` | USM sharpening |
| `denoise` | `Detail::Denoise` | Noise reduction |

**Bilat Decoding:**
```
params[0..3]   = mode (int32: 0=local contrast, 1=bilateral)
params[4..7]   = sigma_r (float, range control)
params[8..11]  = sigma_s (float, spatial control)  → Detail::LocalContrast::radius
params[12..15] = detail (float, local contrast)    → Detail::LocalContrast::amount
params[16..19] = midtone (float)
```

### 6. Unmapped Modules

These darktable modules have no direct Vibe equivalent:

| Darktable Module | Reason | Workaround |
|-----------------|--------|------------|
| `highlights` | Clipped highlight recovery | Pre-HEAD processing |
| `colorout` | Output profile | Post-VIBE sRGB conversion |
| `gamma` | Display gamma | Implicit in sRGB output |

## Compressed Params

Many darktable modules use gzip compression with base64 encoding:
```
"gz04eJxj..." → base64 decode → gzip decompress → raw bytes
```

Modules using compression:
- `filmicrgb` (v6)
- `colorbalancergb` (v5)
- `channelmixerrgb` (v3)
- `colorin` (v7)
- `colorout` (v5)
- `basecurve` (v6)

## Priority Rules

When multiple modules affect the same parameter:

1. **Exposure**: Sum all `exposure` module EV values
2. **Tone Mapping**: Use whichever is enabled:
   - `sigmoid` (scene-referred, preferred)
   - `filmicrgb` (scene-referred, alternative)
   - `basecurve` (display-referred, legacy)
3. **White Balance**: HEAD applies camera WB, `temperature` module adjusts

## Output Format

The flow.json `vibe` node matches vibe.hpp API structure:

```json
{
  "vibe": {
    "_modules": "15",
    "linear": {
      "colorCorrection": {
        "exposure": "1.6",
        "whiteBalance": {
          "temperature": "8282.2",
          "tint": "1"
        }
      },
      "toneMapping": {
        "contrast": "1.5",
        "skew": "0",
        "_raw": "gz02eJyb..."
      },
      "globalColor": {
        "_raw": "gz04eJxj..."
      },
      "detail": {
        "localContrast": {
          "amount": "0.25",
          "radius": "0.5"
        }
      }
    }
  }
}
```

Maps directly to vibe.hpp API:
```cpp
vibe.linear().colorCorrection().exposure().set(1.6f);
vibe.linear().colorCorrection().whiteBalance().temperature(8282.2f);
vibe.linear().toneMapping().contrast().set(1.5f);
vibe.linear().toneMapping().skew().set(0.0f);
vibe.linear().detail().localContrast().amount(0.25f);
```

## Implementation Status

| Module | Decoded | Vibe Target | Status |
|--------|---------|-------------|--------|
| exposure | ✓ | `linear.colorCorrection.exposure` | ✓ summed |
| temperature | ✓ | `linear.colorCorrection.whiteBalance` | ✓ mapped |
| sigmoid | ✓ | `linear.toneMapping.contrast/skew` | ✓ mapped |
| filmicrgb | ✓ | `linear.toneMapping.greyPoint/clippingPoint` | ✓ decoded |
| colorbalancergb | ✓ | `linear.globalColor.saturation/vibrance` | ✓ decoded |
| bilat | ✓ | `linear.detail.localContrast` | ✓ mapped |
| basecurve | - | `linear.baseCurve` | todo |

## vibe.hpp Changes (Applied)

1. `Dial` typedef: Changed comment from "0.0-1.0 normalized" to "Unbounded parameter"
2. `ToneMapping::Skew`: Added for sigmoid skew parameter
3. `ToneMapping::GreyPoint`: Added for filmic grey point (18.45% default)
4. `Detail::LocalContrast`: Added for bilat local contrast enhancement

## Next Steps

1. ~~Implement gzip decompression for compressed params~~ ✓
2. ~~Decode `filmicrgb` → ToneMapping~~ ✓
3. ~~Decode `colorbalancergb` → GlobalColor~~ ✓
4. Verify colorbalancergb struct offsets (saturation/vibrance currently 0)
5. Add basecurve decoder
6. Test round-trip: darktable export vs VIBE output
