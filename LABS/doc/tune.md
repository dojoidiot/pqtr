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

## End-to-End Workflow

The complete tune workflow transforms scene-referred RAW into camera-matched output:

```
┌─────────────────────────────────────────────────────────────────┐
│                         TUNE WORKFLOW                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  RAW File ──► HEAD ──► head.png (camera preview = TARGET)       │
│                │                                                │
│                ▼                                                │
│              BODY (empty) ──► body.png (scene-referred)         │
│                │                                                │
│                ▼                                                │
│              DIFF ──► metrics (spectral + frequency loss)       │
│                │                                                │
│                ▼                                                │
│              TUNE ──► edit steps (35 color + 4 detail dials)    │
│                │                                                │
│                ▼                                                │
│              BODY (with edit steps) ──► TAIL ──► tail.png       │
│                                                                 │
│  Goal: tail.png matches head.png (loss → 0)                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Test Harness

The `tune` test validates this workflow:

```bash
make -f Makefile.tune test
```

**Outputs** (`tmp/var/tune/`):
- `head.png` - Camera preview (target)
- `body.png` - Scene-referred (candidate, no edit steps)
- `tail.png` - Final output (should match head after tuning)
- `diff.png` - Visual difference (head vs body)
- `diff.json` - Loss metrics baseline

**Example metrics** (no edit steps):
```json
{
  "spectral": 0.024843,    // 2.5% color/tone gap
  "frequency": 0.814570    // 81% sharpness gap (expected - no sharpening)
}
```

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
| **Phases** | HUGE → MIDS → TINY (coarse-to-fine) |
| **Loss** | Spectral (geodesic) - content-invariant |
| **Time** | ~60 seconds |
| **Multi-start** | 5 random initializations |

### Coarse-to-Fine Phases

GEOS proceeds through three phases with progressively smaller step sizes:

| Phase | Perturbation | Learning Rate | Purpose |
|-------|--------------|---------------|---------|
| **HUGE** | 0.15 | 0.50 | Explore parameter space, find basin |
| **MIDS** | 0.05 | 0.16 | Refine within basin |
| **TINY** | 0.01 | 0.05 | Precise convergence |

**Transitions:**
- HUGE → MIDS: When loss stops improving (stall detected)
- MIDS → TINY: When loss < 2%

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

The tune module provides a unified PIMPL interface for both loss measurement and optimization.
This consolidates what was previously separate "diff" functionality into a single coherent API.

### tune.hpp

```cpp
namespace tune {

    using View = cv::UMat;  // GPU-accelerated image (BGR 8-bit)

    // Loss metrics (spectral + frequency)
    struct Data {
        float spectral = 0.0f;   // [0,1] geodesic distance (0 = identical color/tone)
        float frequency = 0.0f;  // [0,∞) relative variance diff (0 = identical sharpness)
    };

    // Optimization result
    struct Result {
        Data loss;              // Final loss values
        int geos_iterations;    // SPSA iterations used
        int edge_evaluations;   // Golden section evaluations
    };

    // Optimization configuration
    struct Config {
        bool skip_geos = false;
        bool skip_edge = false;
        int geos_max_iter = 500;
        int geos_multi_starts = 5;
        float geos_threshold = 0.01f;
        float edge_tolerance = 0.01f;
    };

    // Progress feedback for GUI visualization
    struct Progress {
        enum class Stage { GEOS, EDGE } stage;

        // GEOS coarse-to-fine phases (only meaningful when stage == GEOS)
        enum class Phase { HUGE, MIDS, TINY } phase;

        int iteration;
        int max_iterations;

        Data loss;  // Current loss values

        // GEOS: 2D dome compass (style space projection)
        struct Dome {
            float r;      // [0,1] radial distance from target (0 = converged)
            float theta;  // [0,2π] semantic direction of error
            float x() const { return r * std::cos(theta); }
            float y() const { return r * std::sin(theta); }
        } dome;

        // EDGE: 1D sharpness slider
        struct Edge {
            float ratio;  // var_cand / var_ref: 1.0 = matched
        } edge;
    };

    // Progress callback - return false to abort optimization
    using Callback = std::function<bool(const Progress&)>;

    // Task holds cached target features for efficient repeated comparisons.
    // PIMPL design: created via tune::make(), destroyed via RAII.
    class Task {
    public:
        virtual ~Task() = default;

        // Access cached target image (read-only reference)
        virtual View target() = 0;

        // Compute loss metrics between candidate and cached target
        virtual Data diff(View candidate) = 0;

        // Compute visual diff image (amplified pixel difference)
        virtual View view(View candidate, float scale = 5.0f) = 0;

        // Run optimization - modifies link dials in-place
        virtual Result run(pipe::Body& body, pipe::Body::Link& link,
                          const Config& config = Config(),
                          Callback progress = nullptr) = 0;
    };

    // Factory: create Task with target image (features cached for reuse)
    pqtr::Hold<Task> make(View target);

} // namespace tune
```

### Ownership

| Type | Ownership | Notes |
|------|-----------|-------|
| `Hold<Task>` | Caller owns | RAII cleanup when Hold goes out of scope |
| `View` | Shared | OpenCV reference-counted; shallow copy shares GPU buffer |
| `Data` | Caller owns | Value type (two floats); safe to copy |
| `Progress` | Transient | Valid only during callback; do not store references |

### Usage Example

```cpp
#include <tune.hpp>
#include <pipe.hpp>

// Load target (camera preview) and create tune task
cv::UMat target = loadImage("head.png");
pqtr::Hold<tune::Task> task = tune::make(target);

// Measure baseline loss
cv::UMat candidate = body.view();
tune::Data baseline = task->diff(candidate);
std::cout << "Baseline spectral: " << baseline.spectral << std::endl;

// Optimize with progress callback
pipe::Body::Link& link = body.add("style");
tune::Result result = task->run(body, link, tune::Config(),
    [](const tune::Progress& p) {
        if (p.stage == tune::Progress::Stage::GEOS) {
            std::cout << "GEOS " << p.iteration << "/" << p.max_iterations
                      << " r=" << p.dome.r << std::endl;
        }
        return true;  // Continue optimization
    });

// Link now has optimized dial values
body.tail().save("output.png", 1080);
```

---

## Progress Visualization

The tune API provides rich feedback for GUI visualization during optimization.

### Dome Compass (GEOS Stage)

The 10D style hypersphere is projected to a 2D dome compass:

```
        N (target)
        ·
       /|\
      / | \
     /  |  \
    ·───┼───·
     \  |  /
      \ | /
       \|/
        · (candidate moving toward N)
```

**Geometry:**
- Target style vector ψ_ref is rotated to "north pole"
- Candidate ψ_cand position shows distance and direction of style error
- As optimization converges, the dot moves toward center

**Computing dome coordinates:**
```cpp
// Given unit vectors ψ_ref and ψ_cand in R^10
float dot = inner_product(psi_ref, psi_cand);
float r = std::sqrt(1.0f - dot * dot);  // = √(spectral_loss)

// Residual in tangent plane
float residual[10];
for (int i = 0; i < 10; i++)
    residual[i] = psi_cand[i] - dot * psi_ref[i];

// Project onto semantic axes (μ_L and μ_C from style vector)
float x = residual[3];  // Brightness axis
float y = residual[4];  // Color axis
float theta = std::atan2(y, x);
```

**Semantic meaning of theta:**
- 0, π: Brightness error (too bright / too dark)
- π/2, 3π/2: Color error (too saturated / too muted)

### Edge Slider (EDGE Stage)

Sharpness is 1D - shown as a slider:

```
soft ◄──────────┼──────────► sharp
                │
                ▼ (moving toward 1.0)
              target
```

**Computing edge.ratio:**
```cpp
edge.ratio = var_candidate / var_reference;
```

- `ratio < 1.0` → too soft (needs sharpening)
- `ratio = 1.0` → matched
- `ratio > 1.0` → too sharp (needs denoising)

### GUI Integration

```cpp
tune::Result result = task->run(body, link, config,
    [&gui](const tune::Progress& p) {
        if (p.stage == tune::Progress::Stage::GEOS) {
            gui.updateDomeCompass(p.dome.x(), p.dome.y());

            // Show current phase
            const char* phaseNames[] = {"HUGE", "MIDS", "TINY"};
            std::string phase = phaseNames[static_cast<int>(p.phase)];
            gui.setStatusText("Color/Tone [" + phase + "]: " +
                std::to_string(int(p.dome.r * 100)) + "% remaining");
        } else {
            gui.updateSharpnessSlider(p.edge.ratio);
            gui.setStatusText("Sharpness: " + formatRatio(p.edge.ratio));
        }
        gui.setProgress(p.iteration, p.max_iterations);
        return !gui.cancelRequested();  // Return false to abort
    });
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

## Internal Structure

The tune module is split into focused files:

| File | Purpose |
|------|---------|
| `tune.cpp` | Task class + `make()` factory |
| `diff.cpp` | Loss metrics (spectral, frequency) |
| `geos.cpp` | SPSA optimizer for color/tone (stub) |
| `edge.cpp` | Golden section for sharpness (stub) |
| `data.cpp` | Data ↔ JSON serialization |

See [libs.md](./libs.md) for full source structure.

---

## See Also

- [geos.md](./geos.md) - GeoS: Spectral loss theory + SPSA algorithm (color/tone)
- [edge.md](./edge.md) - Edge: Frequency loss theory + greedy algorithm (sharpness)
- [diff.md](./diff.md) - Loss metrics redirect
- [data.md](./data.md) - Style sidecar format
- [test.md](./test.md) - Test cases
