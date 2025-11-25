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
# Outputs: style.geos.json + style.edge.json
./tune source.ARW reference.png --output style

# Color/tone only (skip sharpness)
# Outputs: style.geos.json only
./tune source.ARW reference.png --skip-edge --output style

# Sharpness only (skip color/tone)
# Outputs: style.edge.json only
./tune source.ARW reference.png --skip-geos --output style
```

### Options

| Option | Description |
|--------|-------------|
| `--output NAME` | Output base name (creates NAME.geos.json, NAME.edge.json) |
| `--skip-geos` | Skip color/tone optimization |
| `--skip-edge` | Skip sharpness optimization |
| `--geos-starts N` | Multi-start count for GeoS SPSA (default: 5) |
| `--geos-max-iter N` | Max GeoS iterations (default: 500) |
| `--visualize` | Show real-time progress |

---

## Stage 1: GeoS Color/Tone Optimizer

Optimizes 35 dials using spectral loss (geodesic distance on hypersphere).

**See [geos.md](./geos.md) for full theory and algorithm.**

| Aspect | Value |
|--------|-------|
| **Dials** | 35 (color correction, tone mapping, global color, selective color) |
| **Algorithm** | SPSA (Simultaneous Perturbation Stochastic Approximation) |
| **Loss** | Spectral (geodesic) - content-invariant |
| **Time** | ~60 seconds |
| **Multi-start** | 5 random initializations |

---

## Stage 2: Edge Sharpness Optimizer

Optimizes 4 detail dials using frequency-based loss (Laplacian variance).

**See [edge.md](./edge.md) for full theory and algorithm.**

| Aspect | Value |
|--------|-------|
| **Dials** | 4 (sharpen amount/radius, denoise luma/chroma) |
| **Algorithm** | Greedy (golden section search per dial) |
| **Loss** | Frequency (Laplacian variance) - content-invariant |
| **Time** | ~2 seconds |

---

## Output: Sidecar Files

Tune outputs two sidecar files, each in pipe Link format:

```
<name>.geos.json   # 35 color/tone dials
<name>.edge.json   # 4 detail dials
```

These are standard pipe Links - they can be added directly to any pipe.json.

### style.geos.json

```json
{
  "name": "geos",
  "meta": {
    "loss": 0.0156,
    "iterations": 342,
    "reference": "sunset_beach.png"
  },
  "modules": {
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
    }
  }
}
```

### style.edge.json

```json
{
  "name": "edge",
  "meta": {
    "loss": 0.0234,
    "reference": "sunset_beach.png"
  },
  "modules": {
    "detail": {
      "sharpen": { "amount": 0.4, "radius": 0.5 },
      "denoise": { "luminance": 0.3, "chroma": 0.5 }
    }
  }
}
```

**Note:** Geometric dials are not included - they are user-controlled per image.

---

## Applying Style Sidecars

When applying a tuned style to a new image:

1. **User sets geometry** - Crop, zoom, rotate as desired
2. **Add geos Link** - Load `.geos.json` as a pipe Link
3. **Add edge Link** - Load `.edge.json` as a pipe Link
4. **Process through pipe** - Output has reference style

```cpp
// Load sidecars as Links
json geos = loadJson("sunset.geos.json");
json edge = loadJson("sunset.edge.json");

// Build pipe.json
json pipe;
pipe["version"] = "1.0";
pipe["decoder"] = "sony_arw2";
pipe["links"] = json::array();

// User geometry Link (optional)
json geom;
geom["name"] = "framing";
geom["modules"]["geometric"] = { /* user values */ };
pipe["links"].push_back(geom);

// Add tuned Links
pipe["links"].push_back(geos);
pipe["links"].push_back(edge);

// Process
cv::UMat output = processPipe(rawFile, pipe);
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

- [geos.md](./geos.md) - GeoS: Spectral loss theory + SPSA algorithm (color/tone)
- [edge.md](./edge.md) - Edge: Frequency loss theory + greedy algorithm (sharpness)
- [diff.md](./diff.md) - Loss metrics implementation
- [data.md](./data.md) - Style sidecar format
- [test.md](./test.md) - Test cases
