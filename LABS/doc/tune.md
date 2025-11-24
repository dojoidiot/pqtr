# Tune Tool Specification

[back](../README.md)

## Purpose

The `tune` tool automatically finds optimal pipe dial values to match a target aesthetic. It handles **color/tone** and **sharpness** optimization, while **geometric framing** remains user-controlled.

---

## The Three Roles of Tuning

Image style has three independent dimensions. Each requires different handling:

| Role | Dials | Who/What | Metric | Time |
|------|-------|----------|--------|------|
| **Color/Tone** | 35 | SPSA optimizer | Spectral (geodesic) | ~60s |
| **Sharpness** | 4 | Edge optimizer | Frequency (Laplacian) | ~2s |
| **Geometry** | 6 | User | Visual judgment | - |

**Total: 45 dials = 35 + 4 + 6**

### Role 1: Color/Tone (Automated)

The "vibe" or "mood" of an image - warm/cool, saturated/muted, high-key/low-key, contrast, color harmony.

**Dials (35):**
- Color Correction: exposure, temperature, tint (3)
- Tone Mapping: contrast, highlights, shadows, black point, white point (5)
- Global Color: vibrance, saturation, color density (3)
- Selective Color: 8 colors × hue/saturation/luminance (24)

**Method:** SPSA with spectral loss. Content-invariant - works across different scenes.

### Role 2: Sharpness (Automated)

The texture quality - crisp edges vs soft/dreamy, noise reduction level.

**Dials (4):**
- Detail: sharpen amount, sharpen radius, denoise luminance, denoise chroma

**Method:** Edge optimizer with frequency-based loss. Matches sharpness characteristics.

### Role 3: Geometry (User-Controlled)

The framing and composition - what's in the frame, how it's oriented.

**Dials (6):**
- Geometric: crop top/right/bottom/left, zoom, rotation

**Method:** User judgment. Geometry is a creative/compositional choice, not a transferable style property.

---

## Why This Separation?

### Different Metrics for Different Properties

| Property | What Varies | What Stays Constant | Metric Type |
|----------|-------------|---------------------|-------------|
| Color/Tone | Pixel colors, brightness | Spatial structure | Statistical (SVD) |
| Sharpness | Edge definition, texture | Color distribution | Frequency (Laplacian) |
| Geometry | What's in frame | Everything else | Human judgment |

A single metric cannot optimize all three effectively:
- Spectral loss ignores spatial frequency → blind to sharpness
- Frequency metrics ignore color distribution → blind to tone
- Both require aligned images for geometry → user must handle framing

### Content Invariance

Color/tone and sharpness styles transfer across different scenes. A "golden hour" vibe works on any photo. Geometry does not transfer - each photo has its own optimal framing.

---

## Command-Line Usage

### Full Optimization (Color + Sharpness)

```bash
# Optimize all 39 creative dials
./tune source.ARW reference.png --output style.vibe

# Color/tone only (skip sharpness)
./tune source.ARW reference.png --skip-edge --output style.vibe

# Sharpness only (skip color/tone)
./tune source.ARW reference.png --skip-spsa --output style.vibe
```

### Options

| Option | Description |
|--------|-------------|
| `--output FILE` | Output .vibe file |
| `--skip-spsa` | Skip color/tone optimization |
| `--skip-edge` | Skip sharpness optimization |
| `--spsa-starts N` | Multi-start count for SPSA (default: 5) |
| `--spsa-max-iter N` | Max SPSA iterations (default: 500) |
| `--visualize` | Show real-time progress |

---

## Stage 1: SPSA Color/Tone Optimizer

Optimizes 35 dials using spectral loss. See [geos.md](./geos.md) for theoretical foundation.

### Algorithm

At each iteration:
1. Generate random perturbation Δ ∈ {-1, +1}³⁵
2. Evaluate spectral loss at θ + cΔ and θ - cΔ
3. Estimate gradient from the two measurements
4. Update parameters: θ ← θ - a·ĝ

### Hyperparameters

| Parameter | Value | Purpose |
|-----------|-------|---------|
| a₀ | 0.16 | Initial learning rate |
| c₀ | 0.05 | Initial perturbation |
| α | 0.602 | Learning rate decay |
| γ | 0.101 | Perturbation decay |
| A | 100 | Stability constant |

### Multi-Start Strategy

Runs from 5 random starting points, keeps best result. Avoids local minima.

### Convergence

Stops when:
- Loss < 0.01
- 500 iterations reached
- Loss improvement < 0.001 over 50 iterations

### Performance

- ~60 seconds typical
- 200-400 iterations to converge

---

## Stage 2: Edge Sharpness Optimizer

Optimizes 4 detail dials using frequency-based loss. See [diff.md](./diff.md) for metric details.

### Algorithm

Simple greedy search (only 4 dials):
1. For each detail dial in order:
   - Golden section search in [0, 1]
   - Minimize frequency loss
   - Fix at optimal value

### Frequency Loss

Matches Laplacian variance between candidate and reference:

```
edge_loss = |laplacian_var(candidate) - laplacian_var(reference)| / laplacian_var(reference)
```

### Performance

- ~2 seconds
- 4 dials × ~15 evaluations each

---

## Output: .vibe File

The output contains all 39 optimized creative dials:

```json
{
  "meta": {
    "version": "1.0",
    "timestamp": "2024-05-21T10:00:00Z",
    "spsa_loss": 0.0156,
    "edge_loss": 0.0234,
    "spsa_iterations": 342,
    "reference": "sunset_beach.png"
  },
  "dials": {
    "color_correction": {
      "exposure": { "value": 0.65 },
      "white_balance": { "temperature": 0.55, "tint": 0.48 }
    },
    "tone_mapping": {
      "contrast": { "value": 0.72 },
      "curve_adjustment": { "highlights": 0.45, "shadows": 0.55 },
      "clipping_point": { "black": 0.15, "white": 0.85 }
    },
    "global_color": {
      "vibrance": 0.55,
      "saturation": 0.68,
      "color_density": 0.52
    },
    "selective_color": {
      "red": { "hue": 0.52, "saturation": 0.55, "luminance": 0.5 },
      "orange": { "hue": 0.5, "saturation": 0.6, "luminance": 0.5 },
      "yellow": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
      "green": { "hue": 0.5, "saturation": 0.45, "luminance": 0.5 },
      "cyan": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
      "blue": { "hue": 0.48, "saturation": 0.55, "luminance": 0.5 },
      "purple": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
      "magenta": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 }
    },
    "detail": {
      "sharpen": { "amount": 0.4, "radius": 0.5 },
      "denoise": { "luminance": 0.3, "chroma": 0.5 }
    }
  }
}
```

**Note:** Geometric dials are not included - they are user-controlled per image.

---

## Applying a .vibe

When applying a vibe to a new image:

1. **User sets geometry** - Crop, zoom, rotate as desired
2. **Apply vibe dials** - Load 39 dials from .vibe file
3. **Process through pipe** - Output has reference style

```cpp
// Load vibe
json vibe = loadVibe("sunset.vibe");

// User handles geometry
link.geometric().crop().top(user_crop_top);
// ... other geometric dials ...

// Apply color/tone (35 dials)
applyColorTone(link, vibe["dials"]);

// Apply detail (4 dials)
applyDetail(link, vibe["dials"]);

// Process
cv::UMat output = pipe.process();
```

---

## API Interface

```cpp
namespace pqtr {

struct TuneResult {
    float color_dials[35];     // SPSA-optimized
    float detail_dials[4];     // Edge-optimized
    float spsa_loss;           // Final spectral loss
    float edge_loss;           // Final frequency loss
    int spsa_iterations;
    double computation_time;
};

struct TuneConfig {
    bool skip_spsa = false;
    bool skip_edge = false;
    int spsa_max_iterations = 500;
    int spsa_multi_starts = 5;
};

class Tune {
public:
    Tune(Pipe& pipe, Diff& diff);

    TuneResult optimize(
        const cv::UMat& source_raw,
        const cv::UMat& reference,
        const TuneConfig& config = TuneConfig(),
        ProgressCallback callback = nullptr
    );

private:
    // Stage 1: Color/tone
    void optimizeSPSA(/*...*/);

    // Stage 2: Sharpness
    void optimizeEdge(/*...*/);
};

} // namespace pqtr
```

---

## Summary

| What | How | Time |
|------|-----|------|
| **Color/Tone** (35 dials) | SPSA + spectral loss | ~60s |
| **Sharpness** (4 dials) | Greedy + frequency loss | ~2s |
| **Geometry** (6 dials) | User sets manually | - |

The user's responsibility is simple: **frame your shot**. The tool handles the rest.

---

## See Also

- [geos.md](./geos.md) - Spectral loss theory (color/tone)
- [diff.md](./diff.md) - Loss metrics (spectral + frequency)
- [data.md](./data.md) - .vibe file format
- [test.md](./test.md) - Test cases
