Here is a summary of our reverse-engineering process so far. We are working to mathematically replicate a **Sony "Standard" Camera Profile** by comparing a raw **Linear image** to a processed **Display image**.

We have discovered that the transformation is not a simple curve, but a **three-stage pipeline** following the DCP (DNG Camera Profile) standard.

## Pipeline Order (Corrected)

The correct order, validated against DCP specification and raw processing research:

```
Linear RAW → ColorMatrix (3x3) → HueSatMap (green dampening) → ToneCurve (S-curve)
```

### Stage 1: Color Matrix (applied FIRST to linear data)
A **3x3 Matrix** maps the camera's color gamut to a standard colorspace.
* **The Math:**
    $$
    \begin{bmatrix} 0.66 & 0.32 & 0.11 \\ 0.09 & 0.80 & -0.01 \\ \dots \end{bmatrix}
    $$
* **Key Characteristic:** Mixes 32% of Green into Red output (richer earth tones), reduces pure Green intensity to 80%.
* **Why first:** Must operate on **linear scene-referred data** to preserve colorimetric accuracy.

### Stage 2: HueSatMap (Hue-Specific Dampening)
Applied after matrix, targets specific hue ranges - particularly yellow-greens.
* **The Discovery:** Raw "neon" greens are **dampened**, not boosted:
    * **Brightness:** Reduced by ~40%
    * **Saturation:** Reduced by ~55%
* **The Lesson:** The "vibrant" Sony look comes from *removing* the radioactive/digital quality of raw greens, making foliage look deep and rich.
* **Why second:** Operates in HSV space on already color-corrected data.

### Stage 3: Tone Curve (applied LAST)
A Sigmoidal S-Curve for contrast and dynamic range compression.
* **Key Characteristic:**
    * **Lifts blacks:** input 0 → output 23
    * **Compresses highlights:** input 255 → output 147
* **Why last:** Tone curves shift hues if applied to uncorrected data (e.g., hue 11° becomes 22° after naive gamma). Must come after color corrections.

## Linear-Only Baseline Results

Testing with purely linear operations (no LUT, no tone curve):
| Mode | Loss |
|------|------|
| No optimization | 2.48% |
| Linear-only (exposure, WB, saturation, selective color) | **1.13%** |
| With LUT (non-linear) | 0.18% |

The ~1% gap between linear (1.13%) and LUT-assisted (0.18%) represents the **non-linear tone curve** contribution.

## Diff Analysis

The linear-only diff image shows errors concentrated in:
- **Shadows/dark areas** - tree trunk, dark foliage, shadows under plants
- **Cyan/green tint in shadows** - camera lifts these more than linear can
- **Highlights are clean** - linear operations handle bright areas well

This confirms the S-curve shadow lift is the critical missing piece.

---

## Key Insight: The Problem is Model-Agnostic

The reference image's source is **irrelevant**. Whether it came from:
- Sony camera JPEG
- Lightroom with user edits
- Capture One
- Any other software

...doesn't matter. The transform exists. It's deterministic. It maps input pixels to output pixels. We have both images - that's a complete dataset of the unknown function.

For every pixel we have:
- **Input:** (R, G, B) linear scene-referred
- **Output:** (R', G', B') transformed display-referred

That's millions of sample points.

---

## Model Comparison: Options for Capturing the Transform

| Model | Parameters | What it captures | Limitations |
|-------|------------|------------------|-------------|
| **3× 1D LUT** | 3 × 32 = 96 | Per-channel curves | No cross-channel, no hue-dependent |
| **3×3 Matrix** | 9 | Linear cross-channel mixing | Linear only |
| **Matrix + 1D LUT** | 9 + 96 = 105 | Linear mix + per-channel curves | Still no hue-dependent |
| **HueSatMap** | 90 × 25 × 3 = 6,750 | Hue+Sat dependent adjustments | Assumes specific pipeline |
| **Full 3D LUT** | 33³ × 3 = 107,811 | **Everything** | Large, needs good color coverage |

### Current Approach: 3× 1D LUT
- Achieves **0.18% loss** (down from 2.48% baseline)
- Limitation: Can't capture hue-dependent adjustments
- The residual error is concentrated in specific hues (greens) and tonal regions (shadows)

### Proposed: Full 3D LUT
A 3D LUT can capture **any** RGB→RGB transform with no assumptions:
- No guessing pipeline order
- No assuming DCP structure
- Works for camera profiles, user edits, film emulations, anything
- Just maps: for this input color → this output color

**Trade-offs:**
- Large (33³ grid = 35,937 points × 3 channels)
- Needs good coverage across color space (our images have lots of colors)
- Interpolation quality matters for smooth gradients

---

## Decision Points for Tomorrow

### Option A: Implement 3D LUT Estimator
**Pros:**
- Most general solution
- Captures any transform regardless of source
- No assumptions about pipeline structure
- Single unified approach

**Cons:**
- Large parameter space
- May need multiple images for full color coverage
- Interpolation artifacts possible in sparse regions

### Option B: Implement DCP-style Pipeline (Matrix → HueSatMap → Curve)
**Pros:**
- Matches industry standard
- Smaller parameter space
- Could extract Sony's actual DCP values as starting point

**Cons:**
- Assumes specific pipeline structure
- Won't capture arbitrary user edits that don't follow this model
- More complex implementation (multiple stages)

### Option C: Hybrid - 3D LUT with Constraints
Use 3D LUT structure but:
- Regularize to prefer smooth mappings
- Initialize from Matrix + 1D LUT estimate
- Let optimization refine

### Option D: Stay with Current 1D LUT + Accept 0.18% Floor
**Pros:**
- Already working
- Simple
- Fast

**Cons:**
- Known limitation with hue-dependent errors
- Can't improve beyond current floor

---

## Recommendation

**3D LUT** is the most principled approach because:
1. It makes no assumptions about how the transform was created
2. It can capture everything (matrix, curves, hue shifts, luminance-dependent color, cross-channel)
3. We have the data - millions of input/output pixel pairs
4. Standard size (33³) is well-established in color grading industry

The question is: do we have sufficient color coverage in our test images, or do we need multiple images to populate the full color cube?

---

## References
- [DCamProf Documentation](http://rawtherapee.com/mirror/dcamprof/dcamprof.html)
- [DCP Files Structure](https://dcptool.sourceforge.net/DCP%20FIles.html)
- [RawPedia Color Management](https://rawpedia.rawtherapee.com/Color_Management)