# LABS TLDR

## Core Model

```
RAW sensor data
     │
     ▼
   [RAWS] ─── camera decode ──► Scene-linear RGB (flat)
     │                          ├── Bayer decode
     │                          ├── White balance
     │                          └── Color matrix
     ▼
   [HEAD] ─── generic baseline ──► "Looks decent" starting point
     │                             ├── +0.7 EV exposure
     │                             ├── Highlight recovery
     │                             └── Works for any camera
     ▼
   [BODY] ─── 45 dials ──► Any look you want
     │
     ├── Camera Vibe: find dials that → camera JPEG
     ├── Darktable: human moves sliders → their edit
     └── User Vibe: find dials that → photographer's edit
```

**The insight:** RAWS handles camera-specific decoding. HEAD applies generic baseline (darktable-equivalent defaults). BODY optimizes style. The camera JPEG is ONE dial setting. A Lightroom edit is ANOTHER dial setting. Same 45 dials, different targets.

**In DESK:** The Pipe panel shows this structure - click HEAD/base to see the baselined image, HEAD/view to see the camera preview, or any BODY link to see processed output.

**The only limit:** Transforms outside our dial space (see DRO below).

---

## Success Criteria

| Metric | Target | Current |
|--------|--------|---------|
| Non-DRO images | <3% error | 2-4% ✅ |
| DRO-light images | <5% error | 3-5% ✅ |
| DRO-heavy images | <5% + DRO penalty | 12-16% (expected) |

### DRO Error Budget

Sony's DRO (Dynamic Range Optimizer) applies **spatially-varying** shadow lift - different regions get different adjustments based on local content. This is outside our dial space.

| Image Type | Example | Our Error | DRO Contribution |
|------------|---------|-----------|------------------|
| No DRO / simple | DSC00202 | 2-4% | ~0% |
| Moderate DRO | DSC00144 | 5-8% | ~3-5% |
| Heavy DRO (foliage) | DSC01531 | 12-16% | ~10-12% |

**Parked:** DRO is a spatial problem requiring local tone mapping (Iridix-style). Out of scope. If non-DRO images achieve <3%, the system works.

**Done = non-DRO images at <3% error.** DRO penalty is known and accepted.

---

## Workflow

```bash
# 1. Optimize dials to match a reference
tune photo.ARW reference.jpg --save-area output/

# 2. Apply the vibe
labs photo.ARW --tune output/tune.json --output photo.png

# 3. Debug: see pipeline stages
labs photo.ARW --tune output/tune.json --output photo.png --debug
```

Debug outputs:
- `photo_0_flat.png` - Scene-linear RAW (before processing)
- `photo_0_preview.png` - Camera's embedded JPEG
- `photo_1_body.png` - After all links applied

---

## Core Insight: Photographers Select Based on Style

Photographers don't see flat RAW. They see the camera's styled preview:

1. **Compose** while looking at LCD (with picture style applied)
2. **Expose** based on what they see (styled histogram, styled highlights)
3. **Select** keepers based on that appearance
4. **Edit** as adjustments TO what they chose, not FROM scratch

**The camera JPEG isn't "a reference" - it's the photographer's intent.**

This drives the entire architecture: **Camera first, then vibes.**

---

## Three-Phase Architecture

```
RAW
 ↓
[Camera Math] ─ Polynomial transform (deterministic)
     • 30 coefficients estimated from flat→preview
     • We copy their math
     • Gets most images to <5% error, foliage hits ~15%
 ↓
[Camera Vibe] ─ Optimize dials to match camera JPEG
     • 45 dials, target = embedded preview
     • We find their LUT selection
     • Closes the gap on DRO-heavy scenes
 ↓
[User Vibe] ─ Optimize dials to match photographer edit
     • Same 45 dials, target = edited reference
     • Captures creative intent
     • Exportable as .pipe.json
 ↓
Output
```

**Key insight**: Camera Vibe does per-image what Sony does per-class. Sony bakes dial settings into LUTs indexed by scene type. We optimize to match what their LUT produced for *this specific image*.

## Camera Math (Deterministic)

Polynomial transform captures camera's global RGB→RGB mapping:

```
Out_c = c0 + c1*R + c2*G + c3*B + c4*R² + c5*G² + c6*B² + c7*RG + c8*RB + c9*GB
```

30 coefficients (10 per output channel) capture:
- Color matrix (linear terms c1-c3)
- Tone curve nonlinearity (quadratic terms c4-c6)
- Cross-channel interactions (product terms c7-c9)

**No optimization** - coefficients estimated directly from flat→preview pixel correspondence.

## Camera Vibe & User Vibe (45 Dials)

Both phases use the same 45 dials, different targets:

| Phase | Target | Purpose |
|-------|--------|---------|
| **Camera Vibe** | Embedded preview | Find their LUT selection |
| **User Vibe** | Edited reference | Match creative intent |

**The 45 dials:**
- 3 color correction (exposure, temperature, tint)
- 7 tone mapping (contrast, highlights, shadows, toe, shoulder, black, white)
- 3 global color (vibrance, saturation, density)
- 4 split tone (shadow temp/tint, highlight temp/tint)
- 24 selective color (8 hues × 3 HSL)
- 4 detail (sharpen amount/radius, denoise luma/chroma)

## Feature Space (23D)

Images are compared via a **23-dimensional feature vector**:

```
[0-2]   σ₁, σ₂, σ₃           # SVD singular values
[3-4]   μ_L, μ_C             # Mean luminance, chroma
[5-6]   std_L, std_C         # Contrast, saturation spread
[7]     skew_L               # Tone asymmetry
[8-9]   cov_LC, cov_HC       # Correlations
[10-11] μ_a, μ_b             # Global color cast
[12-15] L_p10..L_p90         # Tone curve percentiles
[16-17] C_p50, C_p90         # Saturation percentiles
[18]    C_shadow             # Shadow chroma
[19-20] a_shadow, b_shadow   # Shadow color (split tone signal)
[21-22] a_highlight, b_highlight  # Highlight color
```

## Jacobian: Dial→Feature Sensitivity (45×23)

The Jacobian matrix J[d][f] measures how much feature f changes when dial d moves by 1 unit. Computed via central difference (±5% perturbation from neutral).

**Uses:**
1. **Gradient-informed optimization** - take steps in high-impact directions
2. **Feature weight adjustment** - low-sensitivity features are unreachable
3. **Understanding dial→feature relationships**

**Key file:** `etc/jacob.json` (45×23 matrix with dial/feature names)

## Current Results (2025-12-05)

### Latest Metrics (633c1aa - STAGED optimizer)

| Image | After Poly | STAGED | HYBRID | Notes |
|-------|-----------|--------|--------|-------|
| DSC00202 | **2.6%** | **2.4%** | 3.7% | STAGED wins on easy images |
| DSC00144 | **12.8%** | **12.8%** | 12.0% | Hard image - VIEW oscillates |

**STAGED optimizer**: VIEW (6 tone dials) → POPS (axis groups) → Joint (45 dials).

### Key Finding

The polynomial transform (Camera Math) provides the heavy lifting:
- DSC00202: 2.6% error from polynomial alone
- Visual output shows proper greens, neutral wood (no more pink cast)

Dials (Camera Vibe) provide refinements. STAGED optimizer improves on HYBRID for easy images by separating tone from color optimization.

### STAGED Optimizer Architecture

```
VIEW Phase (6 dials):
  exposure, contrast, highlights, shadows, black, white
  → Optimize with viewLoss() (luminance features weighted high)

POPS Phase (6 axis groups):
  GLOBAL:  vibrance, saturation, colourDensity (3 dials)
  SPLIT:   shadow_temp/tint, highlight_temp/tint (4 dials)
  R-C:     Red + Cyan HSL (6 dials) - opponent axis
  G-M:     Green + Magenta HSL (6 dials)
  B-Y:     Blue + Yellow HSL (6 dials)
  O-P:     Orange + Purple HSL (6 dials)
  → Optimize each group with popsLoss() (chroma features weighted high)

Joint Phase (45 dials):
  → Polish with geodesicLoss()
```

**Key insight**: Opponent color pairs (R-C, G-M, B-Y) are optimized together, matching how human vision processes color and keeping compensating adjustments in the same group.

## Why DRO-Heavy Scenes Hit 15% Floor

DSC01531's error isn't from ColorMatrix (same as DSC00144 which achieves 3.7%). It's from **DRO's spatially-varying lift**:

| Finding | Evidence |
|---------|----------|
| Same ColorMatrix | DSC00144 and DSC01531 have identical matrix |
| DRO is spatial | Lifts shadows based on **neighborhood**, not pixel value |
| Polynomial is global | Cannot capture "this shadow region lifted more than that one" |

**The 15% floor is a spatial problem** - polynomial transforms are per-pixel, DRO is per-region.

## Foliage Scenes: The Worst Case

DSC01531 is a foliage scene - the **worst case** for local tone mapping:

| Factor | Why It's Hard |
|--------|---------------|
| **High spatial frequency** | Every leaf creates micro-shadows |
| **Already saturated greens** | Less headroom before clipping |
| **Memory color expectations** | Viewers expect vivid greens |
| **DRO + Landscape Style** | Double saturation boost |

**The Double Whammy:**
1. DRO lifts shadows → increases local contrast → saturation boost
2. Sony Landscape style → additional green/blue saturation increase
3. Result: Patchy over-saturated foliage that no per-pixel function can match

**Other scene types are easier:**
- Urban: Large uniform areas, predictable DRO response
- Portrait: DRO focuses on face regions
- Sky: Few shadows to lift, simple gradient

See [hack.md](./hack.md) for full reverse-engineering analysis and foliage scene class study.

## Architecture

```
RAWS:
  1. Decode RAW → scene-linear data
  2. Extract embedded JPEG → preview
  3. Estimate baseCurve[768] from flat→preview (neutral pixels only)
  4. Estimate polyCoeffs[30] from flat→preview (all pixels)
  5. Serialize polyCoeffs to dataInfo["poly_coeffs"]
  6. Return {data, preview, baseCurve, dataInfo}

LABS Pipeline:
  BaseCurve:   baseCurve[768] → primary tone transform (12.6% baseline)
  PolyColor:   polyCoeffs[30] → optional, cross-channel (13.5% baseline)
  Camera Vibe: optimize(dials, target=preview) → find their LUT
  User Vibe:   optimize(dials, target=edit) → match photographer
```

**Key finding (2024-12-02)**: BaseCurve (768 params) outperforms polynomial (30 params) because per-channel curves can exactly match any 1D transform. Polynomial captures cross-channel interactions but has fewer degrees of freedom. BaseCurve is the primary camera transform; PolyColor is available for experimentation.

## Key Files

| File | Purpose |
|------|---------|
| `src/main/part/geos/staged.cpp` | STAGED optimizer (VIEW → POPS → Joint) |
| `src/main/part/geos/diff.cpp` | viewLoss(), popsLoss(), geodesicLoss() |
| `src/main/gold.cpp` | Test binary for STAGED mode |
| `src/main/part/pipe/mods/poly_color.cpp` | Polynomial color transform (Phase 1) |
| `src/main/part/pipe/mods/local_tone.cpp` | Local tone mapping (Iridix-style, research) |
| `RAWS/src/main/raws.cpp` | RAW decoding + coefficient estimation |
| `etc/jacob.json` | Jacobian matrix (45×23 dial→feature sensitivity) |
| `doc/hack.md` | Reverse-engineering Sony ISP + foliage analysis |
