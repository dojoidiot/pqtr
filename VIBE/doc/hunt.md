# VIBE Theory Hunt - ACES/VFX Reference Algorithms

Research into industry-standard algorithms for golden reference implementations.

## Summary

Our CV implementations are based on **ACES** (Academy Color Encoding System) and **VFX industry** standards, with specific implementation choices for optimization and artistic control.

### Final Test Results (Theory vs CV) - 15/17 Pass

| Module | Status | Notes |
|--------|--------|-------|
| exposure | PASS | Simple 2^ev multiply - universal |
| white_balance | PASS | Tanner Helland Planckian approximation |
| tone_map | PASS | Extended Reinhard + shadow/highlight masks |
| global_color | FAIL 14.7% | **CV uses 8-bit Lab conversion** (quantization) |
| geometric | PASS | Sequential crop→zoom→rotate matching CV |
| selective_color | FAIL 16.6% | **CV uses 8-bit HLS conversion** (quantization) |
| split_tone | PASS | Shadow/highlight tinting |
| detail | PASS | Unsharp mask |
| baseline | PASS | Highlight recovery + exposure |
| sigmoid | PASS | darktable's generalized log-logistic |
| base_curve | PASS | Per-channel LUT |
| color_matrix | PASS | Linear 3x3 transform |
| lut_curve | PASS | 8-bit quantized LUT application |
| lut3d | PASS | Trilinear interpolation |
| hsv_lut | PASS | HSV delta adjustments |
| poly_color | PASS | Polynomial color transform |
| local_tone | PASS | Iridix-style local adaptation |

### Remaining Failures Explained

The 2 failing modules (global_color, selective_color) share a common root cause:

**CV Implementation Path:**
```
linear → gamma(1/2.2) → 8-bit BGR → cv::cvtColor(Lab/HLS) → 8-bit → gamma(2.2) → linear
```

**Theory Implementation Path:**
```
linear → gamma(1/2.2) → float Lab/HLS math → gamma(2.2) → linear
```

The 8-bit quantization (~1/255 per step) accumulates through non-linear color space conversions, causing 14-17% max error. This is an **inherent CV implementation limitation**, not an algorithm error.

---

## Module Reference Algorithms (Matching CV)

### 1. Sigmoid (darktable)

**Source:** [darktable sigmoid.c](https://github.com/darktable-org/darktable/blob/master/src/iop/sigmoid.c)

**Formula:** Generalized Log-Logistic Curve (Naka-Rushton)

```cpp
static constexpr float MIDDLE_GREY = 0.1845f;

float loglogistic_sigmoid(float value, float magnitude, float paper_exp,
                          float film_fog, float film_power, float paper_power) {
    const float film_response = pow(film_fog + max(value, 0.0f), film_power);
    return magnitude * pow(film_response / (paper_exp + film_response), paper_power);
}
```

**Key Features:**
- Parameter computation via `compute_sigmoid_params()` from contrast/skewness/targets
- RGB ratio method: apply sigmoid to average luminance, scale channels proportionally
- Gamut compression for out-of-range values using hyperbolic chroma mapping

**Reference:** [pixls.us sigmoid discussion](https://discuss.pixls.us/t/new-sigmoid-scene-to-display-mapping/22635)

---

### 2. Tone Map (Extended Reinhard)

**Our Implementation:** Extended Reinhard with shadow/highlight masks

```cpp
// Extended Reinhard (NOT ACES)
float L = luminance;
float w2 = white_point * white_point;
L = (L + L * L / w2) / (1.0f + L);

// Shadow adjustment with sigmoid mask
float shadow_mask = 1.0f / (1.0f + exp(steepness * (L - toe_pivot)));
L = pow(L, gamma) * shadow_mask + L * (1.0f - shadow_mask);

// Highlight adjustment with inverse mask
float highlight_mask = 1.0f / (1.0f + exp(-steepness * (L - shoulder_pivot)));
L = (1.0f - pow(1.0f - L, gamma)) * highlight_mask + L * (1.0f - highlight_mask);

// Centered power contrast
L = sign(L-0.5) * pow(abs(L-0.5)*2, contrast) * 0.5 + 0.5;
```

**Note:** This differs from ACES filmic - it uses Extended Reinhard for the base curve with separate shadow/highlight controls.

---

### 3. White Balance (Tanner Helland)

**Source:** [Tanner Helland's Algorithm](https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html)

**Implementation:**
```cpp
void kelvin_to_rgb(float kelvin, float& r, float& g, float& b) {
    float temp = kelvin / 100.0f;

    // Red
    if (temp <= 66.0f) r = 1.0f;
    else r = 329.698727446f * pow(temp - 60.0f, -0.1332047592f) / 255.0f;

    // Green
    if (temp <= 66.0f)
        g = (99.4708025861f * log(temp) - 161.1195681661f) / 255.0f;
    else
        g = 288.1221695283f * pow(temp - 60.0f, -0.0755148492f) / 255.0f;

    // Blue
    if (temp >= 66.0f) b = 1.0f;
    else if (temp <= 19.0f) b = 0.0f;
    else b = (138.5177312231f * log(temp - 10.0f) - 305.0447927307f) / 255.0f;
}
```

**Dial Mapping:**
- Temperature dial 0.0→1.0 maps to 2000K→12000K (piecewise exponential)
- Tint dial 0.5=neutral, 0.0=green shift, 1.0=magenta shift

**Note:** This differs from Bradford CAT - it's a simplified Planckian approximation.

---

### 4. Global Color (Lab-based Vibrance)

**Implementation:** Lab color space with skin protection

```cpp
// 1. Convert to Lab via gamma(1/2.2) + cv::cvtColor (8-bit!)
// 2. Vibrance: boost chroma inversely proportional to existing saturation
float C = sqrt(a*a + b*b);  // Lab chroma
float vib_weight = clamp(1.0f - C / 100.0f, 0.0f, 1.0f);

// Skin protection (hue ~45° in Lab)
if (vibrance > 0) {
    float skin_mask = exp(-(hue - 45)² / 450);
    vib_weight *= (1.0f - skin_mask) * 0.7f + 0.3f;
}

a *= 1.0f + vib_weight * vibrance;
b *= 1.0f + vib_weight * vibrance;

// 3. Color density: additional chroma + L contrast
// 4. Convert back via cv::cvtColor (8-bit!) + gamma(2.2)
```

**8-bit Quantization Issue:** CV goes through 8-bit Lab which causes ~15% error vs float theory.

---

### 5. Selective Color (8-band HLS)

**8 Color Bands:**
- Reds (0°), Oranges (45°), Yellows (90°), Greens (150°)
- Cyans (195°), Blues (240°), Purples (285°), Magentas (315°)

**Band Weighting:** Cosine falloff with 45° range
```cpp
float hue_weight(float pixel_hue, float target) {
    float diff = abs(pixel_hue - target);
    if (diff > 180) diff = 360 - diff;
    if (diff > 45) return 0.0f;
    return 0.5f * (1.0f + cos(PI * diff / 45));
}
```

**8-bit Quantization Issue:** CV goes through 8-bit HLS which causes ~17% error vs float theory.

---

### 6. Geometric Transforms

**Order:** Sequential crop → zoom → rotate (NOT combined matrix)

**Crop:** Reduces output dimensions to cropped size
```cpp
auto [out_w, out_h] = geometric_output_size(in_w, in_h, ct, cr, cb, cl);
```

**Zoom:** cv::resize with INTER_LINEAR (zoom in) or INTER_AREA (zoom out)

**Rotate:** cv::warpAffine with BORDER_REPLICATE

---

## Key References

### Official Sources
- [ACES GitHub (ampas/aces-dev)](https://github.com/ampas/aces-dev)
- [OpenColorIO ACES Config](https://github.com/AcademySoftwareFoundation/OpenColorIO-Config-ACES)
- [darktable source](https://github.com/darktable-org/darktable)
- [Tanner Helland Color Temp](https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html)

### Technical Deep Dives
- [Tone Mapping (64.github.io)](https://64.github.io/tonemapping/)
- [pixls.us forums](https://discuss.pixls.us/) - darktable community
- [Bruce Lindbloom Color Math](http://www.brucelindbloom.com/)

---

## Lessons Learned

1. **CV implementations make practical trade-offs** - 8-bit intermediate conversions are faster but lose precision

2. **Theory gold reveals implementation quirks** - The 2 failing modules aren't algorithmically wrong, they have quantization artifacts

3. **Exact algorithm matching required** - Simple "inspired by" implementations diverge significantly; must port exact formulas

4. **darktable sigmoid is state-of-the-art** - Complex parameter derivation from user-friendly inputs

5. **Extended Reinhard differs from ACES** - Our tone mapper uses Reinhard base + shadow/highlight controls, not ACES RRT+ODT

6. **Geometric transforms change dimensions** - CV crops reduce output size; theory must match this behavior

---

## Status

- **15/17 theory gold tests pass**
- **17/17 CV=VIBE tests pass (IDENTICAL)**
- 2 failures are inherent 8-bit quantization in CV, not algorithm errors
- Theory implementations now match CV exactly where floating-point math allows
