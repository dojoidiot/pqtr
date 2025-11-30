# Per-Camera Base Curve Learning

## Problem

The current pipeline produces "washed out" images because the RAW processing starts from a flat baseline. Camera JPEGs apply a base tone curve BEFORE their adjustments that we don't have.

**Measured gap:** Our max std_L = 0.1303, target = 0.2244 (42% shortfall)

## Key Insight: Photographer Intent

The base curve is not just a technical correction - it represents **what the photographer saw and intended**.

```
Photographer's Workflow:
┌─────────────────────────────────────────────────────────────────┐
│ 1. Photographer sets camera to "Vivid" / "Standard" / etc.     │
│ 2. Composes shot while looking at LCD preview (WITH base curve)│
│ 3. Judges exposure and color based on what they SEE            │
│ 4. Makes creative decisions assuming that visual baseline      │
│ 5. Takes the shot                                              │
│ 6. Later expects editing software to show similar baseline     │
└─────────────────────────────────────────────────────────────────┘
```

**The camera JPEG preview IS the photographer's reference point.**

When we show them a "washed out" image, we're showing them something different from what they saw when they took the photo. This breaks their mental model and editing expectations.

### Picture Style = Creative Intent

| Camera Setting | Base Curve Effect | Photographer Intent |
|----------------|-------------------|---------------------|
| Vivid          | High contrast, saturated | "I want punchy, dramatic colors" |
| Standard       | Balanced, neutral | "I want a clean starting point" |
| Portrait       | Soft contrast, warm | "I want flattering skin tones" |
| Landscape      | High contrast, cool | "I want crisp scenery" |
| Neutral/Flat   | Minimal curve | "I'll grade this heavily in post" |

### Implication for Our System

We're not trying to find "objectively correct" rendering. We're trying to match:
1. **What the photographer saw** when composing
2. **What they expected** as their editing baseline
3. **The creative direction** they chose via camera settings

This is why matching the camera JPEG preview is the right target - it represents the photographer's visual intent at capture time.

## Solution Architecture

```
RAW → [Camera Base Curve] → [Our Dials] → [LUT] → Output
            ↑
   Learned per camera/style
```

## Data Structure

```
etc/curves/
  Sony/
    ILCE-7M3/
      standard.json     # Default picture style
      vivid.json        # Vivid picture style
      portrait.json     # Portrait picture style
  Canon/
    EOS_R5/
      ...
```

## Curve Format (base_curve.json)

```json
{
  "manufacturer": "Sony",
  "model": "ILCE-7M3",
  "style": "standard",
  "training_images": 50,

  "curve": {
    "type": "parametric",  // or "lut"
    "contrast_boost": 1.8,  // Multiplier to apply before dials
    "black_point": 0.02,    // Lift shadows by this amount
    "white_point": 0.98,    // Compress highlights above this
    "gamma": 2.2            // Base gamma
  },

  "color_matrix": [         // Optional: base color correction
    [1.0, 0.0, 0.0],
    [0.0, 1.0, 0.0],
    [0.0, 0.0, 1.0]
  ]
}
```

## Learning Algorithm

1. For each camera model in training set:
   a. Collect RAW+JPEG pairs
   b. Process RAW with neutral dials (0.5)
   c. Measure feature gap (especially std_L) between output and JPEG
   d. Grid search over base curve parameters to minimize gap
   e. Save learned curve

2. Validation:
   a. Run tune on held-out images WITH learned curve
   b. Verify std_L is now achievable
   c. Compare visual quality

## Implementation Plan

### Phase 1: Infrastructure
- [ ] Add base curve module to pipeline (before our dials)
- [ ] Load curve from etc/curves/{mfr}/{model}/{style}.json
- [ ] Fallback to identity if no curve found

### Phase 2: Learning Tool
- [ ] Create train_curve.cpp
- [ ] Grid search over curve parameters
- [ ] Output etc/curves/ files

### Phase 3: Integration
- [ ] Modify tune to apply base curve before optimization
- [ ] Update bounds diagnostic to test with curve
- [ ] Re-run validation on training set

## Camera Model Detection

From EXIF (verified available):
```
Make:           SONY
Model:          ILCE-7M3
Creative Style: Standard
```

Exiftool tags:
- `-Make` - Manufacturer
- `-Model` or `-CameraModelName` - Camera model
- `-CreativeStyle` (Sony), `-PictureStyle` (Canon), `-PictureControlName` (Nikon)

## Capture Protocol

**Equipment:**
- X-Rite ColorChecker Classic (24 patch) - ~$70
- Constant light source (D50/D65)
- Tripod

**Per camera, per style:**
```
1. Set picture style (Standard, Vivid, Portrait, etc.)
2. Shoot RAW+JPEG of color card
3. Repeat for each style
```

**Output per camera:**
```
ColorCard_Standard.ARW + ColorCard_Standard.JPG
ColorCard_Vivid.ARW    + ColorCard_Vivid.JPG
ColorCard_Portrait.ARW + ColorCard_Portrait.JPG
ColorCard_Landscape.ARW + ColorCard_Landscape.JPG
ColorCard_Neutral.ARW  + ColorCard_Neutral.JPG
```

## Service Model

**"YOUR Camera Tuned"** - Premium service for professional photographers.

| Generic profile | YOUR camera tuned |
|-----------------|-------------------|
| Model average | Your specific sensor |
| Manufacturing variance ignored | Captures your unit's quirks |
| Same as everyone else | Bespoke to your serial number |

**Workflow:**
1. Pro sends their camera body
2. Shoot color card in controlled lab with THEIR camera
3. Extract curves for all picture styles
4. Deliver personalized profile for their serial number

**Value prop:** Not "Sony A7III profile" - it's "YOUR A7III, serial #3847291, tuned."

## Priority

HIGH - This is blocking the "washed out" problem which affects most images.

## Related Files

- `src/main/part/pipe/mods/tone_map.cpp` - Current tone mapping
- `src/test/geos/bounds.cpp` - Feature achievability diagnostic
- `etc/curves/` - Where learned curves will be stored
