# The Four-Stage Pipeline: RAWS → FLAT → VIEW → POPS

## Core Insight

The semantics matter more than the math. We have 45 dials, but thinking "optimize 45 dials" conflates distinct concerns. The right model is **stages with different optimization objectives**.

## The Stages

```
RAWS → FLAT → VIEW → POPS
```

### RAWS (Camera → Linear)
- **Input**: Sensor data (Bayer pattern, 14-bit)
- **Output**: Camera-agnostic scene-linear RGB
- **Operations**: Black level, demosaic, white balance gains, color matrix
- **Dials**: None (deterministic decode)
- **Goal**: Faithful scene capture, no "look"

### FLAT (Linear Hold)
- **Input**: Scene-linear RGB
- **Output**: Scene-linear RGB (unchanged)
- **Operations**: None - this is a **diagnostic checkpoint**
- **Dials**: None
- **Goal**: Verify clean decode, no spurious transforms
- **Note**: The image looks "flat" here - this is **correct**

### VIEW (Linear → Display)
- **Input**: Scene-linear RGB
- **Output**: Gamma-encoded display RGB
- **Operations**: Tone curve, gamma, contrast, black/white points
- **Dials**: ~5 (exposure, black, white, contrast, toe/shoulder)
- **Goal**: Map linear light to perceptual display space
- **Loss**: Match **absolute tone structure** (percentiles, contrast magnitude)

### POPS (Display → Style)
- **Input**: Display-referred RGB
- **Output**: Styled display RGB
- **Operations**: Saturation, vibrance, split tone, selective color, detail
- **Dials**: ~40 (all color/style adjustments)
- **Goal**: Apply creative intent via **relative adjustments**
- **Loss**: Match **relative relationships** (color ratios, pop factors)

## The Key Distinction

**VIEW** wants absolute structure:
- "Shadows at luminance 0.03"
- "Highlights at luminance 0.95"
- "Contrast ratio of 1:15"

**POPS** wants relative relationships:
- "Greens pop 20% more than neutrals"
- "Shadows warmer than highlights"
- "Saturated areas 30% more vivid than reference"

The reference image encodes both. The math can measure both. But **optimizing both with one loss function causes interference**.

## Why Stages Fix the Interference Problem

Current behavior (observed in DSC00144):
1. Optimizer boosts saturation dial to match reference chroma
2. This affects overall luminance perception
3. Optimizer reduces contrast dial to compensate
4. Result: Flat, oversaturated image

Stage-aware behavior:
1. **VIEW stage**: Lock down tone structure first
   - Optimize exposure, contrast, black/white points
   - Loss only measures tone percentiles
   - Ignore color features during this phase
2. **POPS stage**: Adjust color relationships
   - Optimize saturation, vibrance, selective color
   - Loss only measures color ratios
   - Tone structure is **frozen** - can't interfere

## Dial-to-Stage Mapping

### VIEW Stage (5 dials) - Absolute Structure

| Index | Dial | Purpose |
|-------|------|---------|
| 0 | Exposure | Linear light scaling (2^EV) |
| 3 | Contrast | Global S-curve shape |
| 6 | Toe | Shadow curve region |
| 7 | Shoulder | Highlight curve region |
| 8 | Black Point | Shadow crush |
| 9 | White Point | Highlight stretch |

**Loss features** (weight heavily):
- `L_p10, L_p25, L_p75, L_p90` (percentile positions)
- `std_L` (contrast magnitude)
- `skew_L` (tone curve asymmetry)

### POPS Stage (40 dials) - Relative Relationships

#### Color Balance (3 dials)
| Index | Dial | Purpose |
|-------|------|---------|
| 1 | Temperature | White balance shift (preference) |
| 2 | Tint | Magenta-green cast (preference) |

#### Global Color (3 dials)
| Index | Dial | Purpose |
|-------|------|---------|
| 10 | Vibrance | Boost muted colors relatively |
| 11 | Saturation | Proportional saturation everywhere |
| 12 | Color Density | Chroma intensity |

#### Split Tone (4 dials)
| Index | Dial | Purpose |
|-------|------|---------|
| 13-14 | Shadow Temp/Tint | Color cast in shadows |
| 15-16 | Highlight Temp/Tint | Color cast in highlights |

#### Selective Color (24 dials)
| Index | Dial | Purpose |
|-------|------|---------|
| 17-40 | 8 hues × 3 (H/S/L) | Per-hue adjustments |

#### Detail (4 dials)
| Index | Dial | Purpose |
|-------|------|---------|
| 41-44 | Sharpen/Denoise | Edge enhancement, noise reduction |

**Loss features** (weight heavily):
- `mu_C` (mean chroma)
- `std_C` (chroma spread)
- `C_p50, C_p90` (saturation distribution)
- `C_shadow` (shadow chroma)
- `a_shadow, b_shadow, a_highlight, b_highlight` (split tone)

## Evidence: Code Already Hints at This

### Block Structure in spsa.hpp

```cpp
// Block A: ColorCorrection (3) + ToneMapping (7) = 10 dials
// Block B: GlobalColor (3) + SplitTone (4) = 7 dials
// Block C: SelectiveColour (24) = 24 dials
```

Block A ≈ VIEW stage (mixed with some POPS)
Block B + C = POPS stage

The code optimizes in phases, but **uses the same loss function for all phases**.

### Feature Weights in diff.hpp

```cpp
constexpr std::array<float, STYLE_DIM> FEATURE_WEIGHTS = {
    // Tone features (VIEW)
    5.0f,  // std_L (contrast - critical!)
    5.0f,  // skew_L (tone asymmetry)
    5.0f,  // L_p10, L_p25, L_p75, L_p90 (percentiles)

    // Color features (POPS)
    2.7f,  // mu_C (saturation)
    3.7f,  // std_C (chroma spread)
    5.0f,  // C_p50, C_shadow (saturation structure)
};
```

The weights **already separate concerns** - but all dials optimize against all features equally.

## Implementation: Stage-Aware Loss

### Current (Single Loss)

```cpp
float loss = geodesicLoss(target, candidate);
// All 23 features weighted, all dials optimize against all
```

### Proposed (Stage Loss)

```cpp
float viewLoss(const StyleFeatures& target, const StyleFeatures& cand) {
    // Heavy weight on tone features
    float err = 0.0f;
    err += 5.0f * sq(target.std_L - cand.std_L);
    err += 5.0f * sq(target.skew_L - cand.skew_L);
    err += 5.0f * sq(target.L_p10 - cand.L_p10);
    // ... other percentiles

    // Light weight on color (ignore during VIEW)
    err += 0.1f * sq(target.mu_C - cand.mu_C);
    return sqrt(err);
}

float popsLoss(const StyleFeatures& target, const StyleFeatures& cand) {
    // Heavy weight on color features
    float err = 0.0f;
    err += 5.0f * sq(target.mu_C - cand.mu_C);
    err += 5.0f * sq(target.std_C - cand.std_C);
    err += 5.0f * sq(target.C_p50 - cand.C_p50);
    // ... other color features

    // Light weight on tone (frozen during POPS)
    err += 0.1f * sq(target.std_L - cand.std_L);
    return sqrt(err);
}
```

### Two-Stage Optimization

```cpp
// Stage 1: VIEW
optimizeBlock(body, link, targetStyle, theta,
              VIEW_DIAL_START, VIEW_DIAL_COUNT,
              params, max_iter,
              LossMode::VIEW);  // Uses viewLoss()

// Stage 2: POPS
optimizeBlock(body, link, targetStyle, theta,
              POPS_DIAL_START, POPS_DIAL_COUNT,
              params, max_iter,
              LossMode::POPS);  // Uses popsLoss()

// Optional: Joint refinement with full loss
optimizeBlock(body, link, targetStyle, theta,
              0, ALL_DIALS,
              params, max_iter / 4,  // Fewer iterations
              LossMode::FULL);
```

## Why This Aligns with hack.md

From hack.md:

> "The camera has a **minimal style adjustment phase**... it's not optimization - it's LUT selection"

Cameras precompute styles as LUTs, then select based on scene type. **POPS is exactly this concept** - the relative adjustments that make a "Landscape" look different from "Portrait".

> "Sony engineers ran their version of dial optimization **once** at factory calibration time, baked the results into firmware as LUTs"

They separated VIEW (tone mapping, exposure) from POPS (creative style). We should too.

## The POPS Insight: Relative, Not Absolute

A key realization: **POPS dials don't care about absolute pixel values**.

When you turn up "vibrance", you're not saying "make pixel (128, 64, 32) become (140, 58, 28)". You're saying "make muted colors relatively more saturated".

This means POPS optimization should measure **ratios and relationships**:

| What to measure | Not this | But this |
|-----------------|----------|----------|
| Green pop | Green chroma = 0.45 | Green chroma / neutral chroma = 1.3× |
| Shadow warmth | Shadow a* = 0.08 | Shadow a* - highlight a* = +0.12 |
| Saturation spread | C_p90 = 0.72 | C_p90 - C_p10 = 0.45 |

The reference image encodes these ratios. We don't need object recognition - the math tells us if our pops are right.

## POPS Can Also Mean "Negative Pops"

POPS isn't just "make things pop". It's the relative dial space:

- **Positive pop**: Boost saturation, increase contrast, sharpen
- **Negative pop**: Desaturate, flatten, soften
- **Selective pop**: Greens vivid, skin muted

The term captures the **stage function** (style adjustments), not the direction.

## Connection to the Three-Phase Architecture

From hack.md's existing model:

```
[Camera Math] → [Camera Vibe] → [User Vibe]
```

Mapped to RAWS → FLAT → VIEW → POPS:

| hack.md | New Model | Function |
|---------|-----------|----------|
| Camera Math | RAWS + Polynomial | Deterministic decode + color transform |
| Camera Vibe | VIEW + POPS | Match camera preview (absolute + relative) |
| User Vibe | VIEW + POPS | Match photographer edit (absolute + relative) |

The new model is **more granular** - it separates the optimization concerns within Vibe phases.

## Summary

1. **RAWS**: Get data out of camera format (deterministic)
2. **FLAT**: Scene-linear checkpoint (no dials, just verify)
3. **VIEW**: Tone mapping for display (5 dials, absolute loss)
4. **POPS**: Style adjustments (40 dials, relative loss)

The semantic reframe clarifies:
- Which dials belong together
- What loss function each group needs
- Why dial interference happens
- How to fix it (stage-aware optimization)

---

## Next Steps

1. **Implement stage-aware loss functions** in `diff.cpp`
2. **Add Mode::STAGED** to optimizer config
3. **Test on problem images** (DSC00144, DSC00202, DSC01559)
4. **Measure convergence speed** and final error vs current approach
