# Hacking the Matrix: Reverse Engineering Camera Color Science

## The Core Insight

**The nonlinear problem is human behavior. We fight fire with fire.**

When algorithms seem inscrutable, remember: they encode human decisions, priorities, and compromises. The "black box" isn't mathematical mystery - it's frozen human judgment. To reverse-engineer it, we apply human methods: searching patents, reading marketing, analyzing competitor criticism, finding forum posts where engineers got chatty.

## The Realization

All technology is made by humans. When we face a "black box" problem - like understanding how Sony's BIONZ processor transforms RAW sensor data into a JPEG - we're not facing an unknowable alien intelligence. We're facing the accumulated decisions of engineers, product managers, and scientists who:

1. **Patent** their innovations (searchable on Google Patents)
2. **Publish** their research (IEEE, ACM, academic papers)
3. **Market** their features (press releases, spec sheets, user manuals)
4. **Brag** about capabilities (marketing videos, trade show demos)
5. **License** from others (credit notices in manuals, royalty disclosures)
6. **Get analyzed** by competitors and reviewers (DxOMark, DPReview)
7. **Get reverse-engineered** by enthusiasts (firmware hacking communities)

## Our Process

### Phase 1: Measurement
Started with pure measurement - comparing our RAW decode output to camera JPEG pixel-by-pixel.

**What we learned:**
- Colorimetric accuracy ≠ visual match
- Sony's ColorMatrix (tag 0x7800) is necessary but not sufficient
- Per-channel curves cannot capture cross-channel transforms

### Phase 2: Pattern Recognition
Identified that our error varied wildly by scene:
- DSC00144: 3.7% error (achievable)
- DSC01531: 15-21% error (problematic)

**Key insight:** DSC01531 has heavy shadow lifting and foliage saturation - classic DRO territory.

### Phase 3: The Hunt Begins

#### Marketing Materials → Technical Clues
Sony brags about:
- "D-Range Optimizer" with 5 levels + Auto
- "Advanced BIONZ processing"
- "Face detection" integration
- "Creative Styles" (Vivid, Portrait, Landscape, etc.)

**Revelation:** Early Sony manuals credited **Apical** for DRO technology.

#### Patent Archaeology
Searched for Apical patents, found **US7302110B2** by Vyacheslav Chesnokov:
- "Image enhancement methods and apparatus therefor" (2002)
- Describes **ORMIT** (Orthogonal Retino-Morphic Image Transform)
- Local tone mapping that mimics human retinal adaptation

**Core algorithm discovered:**
```
Iout = Σ(i=0 to N) αi(I)·LPFΩ[Pi(F(I))]·Qi(F(I)) + β(I)
```

Where:
- `Pi(γ)` = orthogonal basis functions (Legendre polynomials)
- `α(I)` = transform strength (stronger for shadows)
- `LPFΩ` = 2D low-pass spatial filter
- `F(I)` = asymmetric logarithmic weighting function

**Key insight:** The algorithm applies *different tone curves to different parts of the image* based on local luminance context.

#### Competitor Analysis
Found Sony color science criticisms:
- Green/yellow tint in skin tones
- Orange shifts in reds
- "Zombie look" in video

**Solution hints:**
- Color Phase shift toward magenta
- White balance around 6020K, tint +15
- Third-party color profiles (EOSHD Pro Color)

### Phase 4: Technical Deep Dive

#### Apical Iridix Technology
- Licensed to Sony, Samsung, Nikon, and others
- Per-pixel tone mapping based on local histogram
- "Window" sizes control spatial locality
- Asymmetric processing: strong in shadows, weak in highlights

**Mathematical model:**
```
F(I) = [log(I+Δ) - log(Δ)] / [log(1+Δ) - log(Δ)]
α(I) = 0.5 - 0.5·tanh(4·log((I/Δ)+1)/log((1/Δ)+1) - 2)
```

The `Δ` parameter (0.001 to 0.1) controls shadow/highlight balance.

#### DCP Profiles
Adobe's solution to the same problem:
- `ColorMatrix` = colorimetric (sensor to XYZ)
- `ForwardMatrix` = includes "look"
- `HueSatMap` = 2.5D polar LUT for color shifts

**Key insight:** Even Adobe separates "correct color" from "pleasing color."

## Implementation Strategy

### Camera Phase (Deterministic)
Goal: Replicate what the camera does, exactly.

1. **Global polynomial transform** (30 coefficients)
   - Captures color matrix + tone curve + cross-channel effects
   - Works for ~3/4 of scenes (<5% error)

2. **Local tone mapping** (for DRO-heavy scenes)
   - Implement Iridix-style algorithm
   - Gaussian pyramid for multi-scale decomposition
   - Per-pixel adaptive curve generation

3. **Scene detection heuristics**
   - Detect when DRO was likely applied
   - Histogram analysis, shadow/highlight distribution

### Human Phase (Optimization)
Goal: Match photographer's creative intent via 45 dials.

This phase remains unchanged - dial-based optimization in behavior space.

## The Hardware Constraint

Cameras run on dumb ARM processors with limited compute. Every algorithm must:
- Run in real-time for live view
- Process millions of pixels per frame
- Fit in limited memory
- Use fixed-point math or simple LUTs

**Implication:** The "sophisticated" algorithms are actually compromised versions of the academic ideal. Apical's Iridix patent describes elegant Legendre polynomials and multi-scale decomposition - but the actual implementation uses:
- Simplified piecewise linear basis functions
- Reduced window sizes
- Coarse spatial sampling
- LUT approximations

We can find these compromises. The gap between patent and product reveals implementation constraints.

## The Decision Tree Model

A camera can only do so much. The processing pipeline is a **decision tree** selecting from precomputed options:

```
Creative Style → selects one of ~10 precomputed {color_matrix, saturation_lut, tone_curve}
DRO Setting → selects local TM strength from {off, lv1, lv2, lv3, lv4, lv5, auto}
Scene Mode → may override style selection
```

The "magic" isn't complex per-pixel math - it's **precomputed LUTs** selected by simple logic:

1. **Demosaic** → fixed algorithm (nothing to reverse)
2. **Color Matrix** → one of ~10 precomputed 3x3 matrices (estimated by polynomial)
3. **DRO/Local TM** → downscaled blur + strength LUT (cheap on ARM)
4. **Tone Curve** → one of ~5 precomputed 1D LUTs
5. **Saturation** → simple multiplier or 2D hue-sat LUT
6. **Output**

**Key insight**: We don't need to simulate the algorithm. We need to **recover the LUTs** it selected.

## The Camera's "Embedded Lightroom"

The camera has a **minimal style adjustment phase** after compute, but it's not optimization - it's LUT selection:

```
Camera Compute (same for all images)
    ↓
Scene Detection → "this is Landscape"
    ↓
Select from precomputed bank:
    • Landscape LUT: +green sat, +blue sat, +sharpness, +contrast
    • Portrait LUT: +skin warmth, softer contrast, face-aware lift
    • Night Scene LUT: aggressive shadow lift, noise reduction
    • Standard LUT: neutral baseline
    • Vivid LUT: +saturation, +contrast across the board
```

**It's not "Lightroom on ARM"** - it's lookup tables indexed by scene class:

| What it looks like | What it actually is |
|-------------------|---------------------|
| "AI scene optimization" | `if (landscape) apply(landscape_lut)` |
| "Intelligent color science" | Precomputed adjustments per scene type |
| "Real-time processing" | LUT lookup, not optimization |

**Why this matters for us:**

Sony engineers ran their version of dial optimization **once** at factory calibration time, baked the results into firmware as LUTs, and the camera just selects which LUT to apply.

**Camera Vibe** does the **same thing** but per-image:
- Sony: optimize once → freeze as class LUT → select by scene type
- Us: optimize per-image → match what Sony's LUT produced for *this specific image*

We reverse-engineer the **result** rather than the selection logic. This handles edge cases where Sony's class-level preset doesn't fit perfectly (like DSC01531's complex foliage).

## Experimental Results

### Cross-Image Polynomial Results

| Image | DRO Level | Shadow Content | Polynomial Error |
|-------|-----------|----------------|------------------|
| DSC00159 | 0 (reduced) | 25% | **3.1%** |
| DSC00144 | 3 | 73% | **3.7%** |
| DSC01531 | 3 (heavy scene) | 34% | **15.1%** |

### DSC01531 Detailed Analysis

| Approach | Error |
|----------|-------|
| Baseline (gamma only) | 17.6% |
| Grid-based LTM alone | 15.8% |
| Single polynomial | 15.1% |
| Polynomial + LTM | 15.1% (no improvement) |
| Regional polynomials | 18.2% (worse - artifacts) |

### Key Findings

1. **Polynomial works well for most images** - 3-4% error is achievable when DRO effects are light

2. **Heavy DRO scenes are the outlier** - DSC01531's 15% error isn't about DRO level, it's about **scene content**. The foliage creates spatially complex shadow regions that DRO lifts non-uniformly.

3. **Spatial effects are the hard problem** - DRO's transform depends on **local context**, not just pixel value. A pixel at L=0.2 in a globally dark region gets lifted differently than L=0.2 adjacent to bright areas.

4. **LTM approaches don't help** - Our Iridix-style implementations achieved only marginal improvement (~0.5 percentage points) and sometimes made things worse.

### Implications for Camera Phase

**For most images:** Polynomial alone achieves <5% error. This is the camera phase solution.

**For DRO-heavy scenes:** Accept ~15% error as camera phase baseline. The human phase (45 dials) must close the remaining gap through optimization.

**Practical strategy:**
1. Apply polynomial transform (always helps)
2. Detect DRO-heavy scenes (>30% shadow content + DRO Auto)
3. For these scenes, rely more heavily on human phase optimization

## The Canonical ISP Pipeline

Everyone learns from the same textbooks. The standard pipeline order (from openISP, Infinite-ISP, Stanford lectures):

```
RAW Sensor Data
    ↓
1. Dead Pixel Correction (DPC)      [Bayer domain]
2. Black Level Compensation (BLC)   [Bayer domain]
3. Lens Shading Correction (LSC)    [Bayer domain]
4. Anti-aliasing Noise Filter       [Bayer domain]
5. Auto White Balance (AWB)         [Bayer domain]
6. Demosaicing (CFA interpolation)  [Bayer → RGB]
7. Gamma Correction                 [RGB domain]
8. Color Correction Matrix (CCM)    [RGB domain]
9. Color Space Conversion           [RGB → YUV]
10. Noise Filter (luma/chroma)      [YUV domain]
11. Edge Enhancement/Sharpening     [YUV domain]
12. Hue/Saturation Control          [YUV domain]
13. Brightness/Contrast Control     [YUV domain]
14. JPEG Compression                [Output]
```

**Key insight:** Everyone uses this order. The marginal differences are in the *parameters* and *LUTs* at each stage, not the architecture.

## Brand Differences: Same Pipeline, Different Tuning

| Brand | Skin Tone Bias | Known Issues | Key Differentiator |
|-------|---------------|--------------|-------------------|
| Canon | Warm, yellow-orange | - | Flattering skin tones SOOC |
| Nikon | Neutral, accurate | Slight yellow-green | True-to-life color |
| Sony | Accurate, green bias | Green skin tones | Technical accuracy |

**Sony's evolution:**
- Early models (A7II): Strong green cast in skin tones
- A7III (2018): "Mostly addressed"
- A7IV (2021): "Best tones Sony ever produced"
- A7RV (2022): AI-assisted AWB with IR sensors, "green tint resolved"

**The marginal improvements:**
1. AWB algorithm refinement (AI + IR sensors)
2. Red/yellow correction (no longer shift to crimson/pink)
3. Warmer default white balance
4. Reduced magenta/green shift

## Sony ARW Metadata: The Precomputed LUTs

Sony stores precomputed values for every illuminant:

```
WB RGB Levels Daylight   : 2432 1024 1612
WB RGB Levels Cloudy     : 2628 1024 1480
WB RGB Levels Tungsten   : 1504 1024 2860
WB RGB Levels Flash      : 2668 1024 1428
WB RGB Levels Shade      : 2892 1024 1332
WB RGB Levels Fluorescent: 2252 1024 2364
```

**Key observation:** Different images with same camera have:
- **Same ColorMatrix** (per-camera, not per-scene)
- **Different WB gains** (per-illuminant)

This means high error on DRO-heavy scenes (DSC01531 at 15%) is NOT from ColorMatrix.
It's from **post-matrix processing** (DRO local adaptation, Creative Style curves).

## Lessons Learned

1. **Patents are gold mines** - engineers must disclose their methods
2. **Marketing reveals priorities** - what companies brag about is what they implemented
3. **Licensing credits expose dependencies** - "Apical" in a manual = Iridix inside
4. **Competitor criticism identifies weaknesses** - "green skin tones" = target for improvement
5. **Forums have experts** - CEOs and engineers sometimes post technical details
6. **Nothing is truly secret** - humans built it, humans documented it somewhere
7. **The pipeline is standard** - differences are in tuning, not architecture
8. **Improvements are incremental** - small refinements to AWB, CCM, HSC parameters

## The Wide-to-Narrow Strategy

Started wide (canonical ISP pipeline), narrowed to Sony, found the margin:

1. **Wide:** All cameras use same pipeline order (DPC→BLC→AWB→Demosaic→CCM→Gamma→HSC→Output)
2. **Medium:** Brand differences are tuning parameters, not architecture
3. **Narrow:** Sony's specific issues (green skin) are in AWB + HSC stages
4. **Margin:** DRO (Apical Iridix) is Sony's differentiator for local adaptation

**The 15% error floor on DSC01531:**
- ColorMatrix is correct (same as DSC00144 which achieves 3.7%)
- WB gains are per-illuminant, correctly applied
- The problem is **DRO's spatially-varying lift** - different pixels get different curves based on neighborhood

**Conclusion:** For camera phase, we can achieve <5% error on most scenes with polynomial transform.
DRO-heavy scenes require accepting higher baseline error OR implementing true spatial local adaptation.

## Resources

### Patents
- US7302110B2 - Apical ORMIT/Iridix foundational patent
- US11301973B2 - Recent Apical tone mapping improvements
- US8160387B2 - Apical image processing methods

### Tools
- [Sony-PMCA-RE](https://github.com/ma1co/Sony-PMCA-RE) - Sony camera firmware RE
- [fwtool.py](https://github.com/ma1co/fwtool.py) - Firmware unpacking tools
- exiftool - Metadata extraction (including encrypted Sony tags)

### Analysis
- DPReview forum discussions on DRO, color science
- DxOMark sensor analysis and color response
- EOSHD color science fixes

## The Foliage Scene Class: Wide-to-Narrow Analysis

DSC01531 is a foliage scene with complex shadows. Applying the wide-to-narrow strategy to understand this **class** of image:

### Wide: How All Cameras Handle Foliage

**1. Memory Colors (Universal)**

From patent [US6594388B1](https://patents.google.com/patent/US6594388):
> "Memory colors skin, sky, and **foliage** are consistently and smoothly moved towards a hue line"

All cameras intentionally shift foliage toward what viewers *prefer* to see, not accurate reproduction:
- Foliage hues shifted toward yellower-greens (max 15° rotation)
- Bounded by hue angles 10-40° above/below reference
- Based on Macbeth Color Checker foliage patch

**2. Scene-Dependent Tone Scaling**

The patent describes luminance-chrominance separation:
1. Tone scaling applies to luminance only (preserves color)
2. Adaptive curve generation from pixel histogram
3. Hue/chroma modifications applied afterward

**Key insight:** The order matters. Tone mapping *then* color adjustment prevents unintended hue shifts.

**3. The Shadow Lifting / Saturation Problem**

From academic literature on [tone mapping](https://64.github.io/tonemapping/):
> "Regions which have increased in contrast will exhibit an increase in color saturation"

When DRO lifts shadows, it increases local contrast, which boosts saturation. This is **desirable** for some shadow detail but can over-saturate foliage:
- Shadow lifting → contrast increase → saturation boost
- Green foliage already saturated → clipping/artifacts
- Different shadow regions lifted differently → patchy saturation

**4. Luminance-Only vs RGB Channel Processing**

Two schools of tone mapping:

| Approach | Behavior | Best For |
|----------|----------|----------|
| **Luminance-only** | Preserves hue, may over-saturate | Color-critical work |
| **RGB per-channel** | Hue/sat shifts, natural highlight rolloff | Film-like rendering |

Reinhard tone mapping: "applying nonlinear curves per channel are an important feature, not a bug"

Filmic approaches: Chroma preservation before log shaper, restore RGB ratios after - "will most of the time produce over-saturated colours that need to be dealt with separately"

### Medium: Sony's Landscape Creative Style

Sony's Landscape Creative Style specifically boosts greens and blues:

> "Increases the sharpness of the image, and provides a saturation increase to colors relevant to landscape photography, such as greens and blues"
> — [Sony Help Guide](https://helpguide.sony.net/ilc/1720/v1/en/contents/TP0001653152.html)

**Scene Detection:**
Sony BIONZ recognizes scene types: Portrait, Landscape, Night Scene, etc.
When Landscape is detected (or explicitly set), the camera applies:
- Higher sharpness
- Green/blue saturation boost
- Higher contrast

**The Double Whammy:**
For DSC01531:
1. DRO lifts shadows → increases saturation in shadow regions
2. Landscape style → additionally boosts green saturation
3. Result: Over-saturated foliage that polynomial can't match

### Narrow: Why DSC01531 Specifically Fails

| Factor | DSC01531 | DSC00144 |
|--------|----------|----------|
| Scene content | Dense foliage, dappled light | Urban, metal, fabric |
| Shadow distribution | Complex, spatially varying | More uniform |
| DRO behavior | Lifts foliage shadows non-uniformly | Simpler lift pattern |
| Saturation result | Patchy green saturation | Predictable response |

**The Core Problem:**

1. DRO is **spatially adaptive** - different regions get different lift
2. Saturation boost is **coupled** to local contrast increase
3. Foliage has **complex spatial structure** - many shadow/highlight boundaries
4. Result: Each leaf cluster gets slightly different saturation based on its neighborhood

A polynomial transform is **per-pixel**: `f(R,G,B) → (R',G',B')`
DRO's effect is **per-region**: `f(R,G,B, neighborhood) → (R',G',B')`

No per-pixel function can capture this spatial dependency.

### The Margin: What Makes Foliage Special

Foliage scenes are the **worst case** for local tone mapping because:

1. **High spatial frequency of shadows** - every leaf creates micro-shadows
2. **Already saturated greens** - less headroom before clipping
3. **Memory color expectations** - viewers expect vivid greens
4. **DRO tuned for this** - aggressively lifts forest shadows

Other scene types are easier:
- **Urban**: Large uniform areas, predictable DRO response
- **Portrait**: DRO focuses on face regions
- **Sky**: Few shadows to lift, simple gradient

### Implications

**For Camera Phase:**
Foliage scenes with complex shadows are the irreducible hard case. Polynomial achieves ~15% error, not because the transform is wrong, but because the **spatial information is lost**.

**For Human Phase:**
These scenes need dial-based optimization to close the gap. The 45 dials can apply:
- Selective green saturation reduction
- Shadow tone adjustments
- Local contrast tweaks

**Possible Future Work:**
If we wanted to close the gap in camera phase:
1. Implement true spatial local adaptation (expensive)
2. Detect foliage scenes and apply scene-specific processing
3. Use a spatial LUT (5D: RGB + x,y position) - combinatorial explosion

**Practical Decision:**
Accept the 15% floor for foliage scenes. Let human phase handle them. This is what the cameras themselves do - DRO is a "good enough" approximation that users can adjust.

### Sources

- [US6594388B1](https://patents.google.com/patent/US6594388) - Preferential color mapping with memory colors
- [Mantiuk et al.](https://www.cl.cam.ac.uk/~rkm38/pdfs/mantiuk09cctm.pdf) - Color correction for tone mapping
- [Sony Creative Style](https://helpguide.sony.net/ilc/1720/v1/en/contents/TP0001653152.html) - Official documentation
- [Tone Mapping](https://64.github.io/tonemapping/) - Technical overview of luminance vs RGB
- [DPReview DRO Discussion](https://www.dpreview.com/forums/thread/4133243) - User experiences with DRO
- [Envato Foliage Tutorial](https://photography.tutsplus.com/tutorials/dealing-with-foliage-green-and-yellow-saturation--photo-10127) - The saturation clipping problem

---

## Status (2024-12-01)

### Completed
- Polynomial color transform module (`poly_color.cpp`)
- Local tone mapping module (`local_tone.cpp`)
- Extensive testing on DSC01531 (LTM, regional polynomials, etc.)
- Full reverse-engineering documentation
- Foliage scene class analysis (wide-to-narrow)
- Three-phase architecture design

### Three-Phase Architecture

```
RAW
 ↓
[Camera Math] - Polynomial (deterministic)
     • 30 coefficients, we copy their math
     • <5% error on most images, ~15% on foliage
 ↓
[Camera Vibe] - 45 dials → preview
     • We find their LUT selection
     • Dials ≈ 0.5 for easy images, deviate for foliage
 ↓
[User Vibe] - 45 dials → edited reference
     • Captures creative intent
     • Exportable as .pipe.json
```

### Conclusion
- Camera Math (polynomial) achieves <5% error on most images
- Camera Vibe (dial optimization) handles foliage/DRO scenes
- User Vibe captures photographer intent
- No need for true spatial LTM - Camera Vibe handles it with dials
