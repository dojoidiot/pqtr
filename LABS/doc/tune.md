# Tune Tool Specification

[back](../README.md)

## Purpose

The `tune` tool automatically finds optimal pipe dial values to match a target aesthetic. It handles **color/tone** and **sharpness** optimization, while **geometric framing** remains user-controlled.

---

## The Three Roles of Tuning

Image style has three independent dimensions. Each requires different handling:

| Role | Dials | Who/What | Metric | Time |
|------|-------|----------|--------|------|
| **Color/Tone** | 41 + LUT | SPSA/ACEO/HYBRID | Weighted L2 (19D) | ~5min |
| **Sharpness** | 4 | Detail optimizer | Frequency (Laplacian) | ~2s |
| **Geometry** | 6 | User | Visual judgment | - |

**Total: 51 dials = 41 + 4 + 6** (plus 17³ LUT for nonlinear color)

### Role 1: Color/Tone (Automated)

The "vibe" or "mood" of an image - warm/cool, saturated/muted, high-key/low-key, contrast, color harmony.

**Dials (41):**
- Color Correction: exposure, temperature, tint (3)
- Tone Mapping: contrast, highlights, shadows, toe pivot, shoulder pivot, white point, black point (7)
- Global Color: vibrance, saturation, color density (3)
- Split Tone: shadow temp/tint, highlight temp/tint (4)
- Selective Color: 8 hues × 3 HSL adjustments (24)
- **17³ 3D LUT** captures nonlinear camera color science

**Method:** 3D LUT estimation + SPSA/ACEO/HYBRID with 19D weighted L2 loss. Content-invariant.

### Role 2: Sharpness (Automated)

The texture quality - crisp edges vs soft/dreamy, noise reduction level.

**Dials (4):**
- Detail: sharpen amount, sharpen radius, denoise luma, denoise chroma

**Method:** Golden section search with Laplacian variance. Luminance-only sharpening preserves color accuracy.

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
│              TUNE ──► edit steps (41 color + 4 detail dials)    │
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

**Example metrics:**
```json
// Baseline (no edit steps):
{ "spectral": 0.024843, "frequency": 0.814570 }  // 2.5% color, 81% sharpness

// After optimization (45 dials):
{ "spectral": 0.000511, "frequency": 0.007 }     // 0.05% color, <1% sharpness
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
# Optimize all 45 style dials (41 color + 4 detail)
# Outputs: tune.json (contains dials + 3D LUT)
./tune source.ARW reference.png --save-area ./output

# With verbose progress logging
./tune source.ARW reference.png --save-area ./output --logs

# Save intermediate images + metadata
./tune source.ARW reference.png --save-area ./output --fine

# Intermediate images to separate directory
./tune source.ARW reference.png --save-area ./output --fine --fine-area ./debug

# Skip LUT estimation (dials only)
./tune source.ARW reference.png --save-area ./output --skip-lut
```

### Options

| Option | Description |
|--------|-------------|
| `--save-area <dir>` | Output directory for tune.json (required) |
| `--threshold <value>` | Stop when spectral loss below (default: 0.005) |
| `--size <pixels>` | Working size (default: 1080) |
| `--mode <mode>` | blockwise, full35d, linear (default: blockwise) |
| `--skip-lut` | Skip 3D LUT estimation |
| `--logs` | Verbose progress (dome.r, edge.ratio) |
| `--fine` | Save intermediate images + meta.json (camera metadata) |
| `--fine-area <dir>` | Directory for --fine outputs (default: --save-area) |

---

## Two-Link Architecture

Tune produces **two separate links** to separate concerns:

```
┌─────────────────────────────────────────────────────────────────┐
│                    TWO-LINK PIPELINE                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  RAW ──► LINK 1: Scene-Linear (5 dials)                         │
│              exposure, temp, tint, black, white                 │
│              No LUT - pure linear operations                    │
│                   │                                             │
│                   ▼                                             │
│          LINK 2: Display-Referred (36 dials + LUT)              │
│              tone curves, color, split tone, selective          │
│              17³ LUT for residual correction                    │
│                   │                                             │
│                   ▼                                             │
│          tail.png                                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Why Two Links?

| Link | Domain | Dials | LUT | Purpose |
|------|--------|-------|-----|---------|
| **Scene-Linear** | Linear light | 5 | No | Exposure, white balance, clipping |
| **Display** | Display-referred | 36 | Yes | Tone mapping, color grading, selective color |

**Benefits:**
- Scene-linear ops are physically meaningful (stops, kelvin)
- Display ops are perceptually meaningful (contrast, saturation)
- LUT only captures display transforms, not linear corrections
- Cleaner separation for debugging and transfer

### Output Format

```json
{
  "links": [
    {
      "name": "linear",
      "modules": { "exposure": 0.5, "temp": 0.5, "tint": 0.5, "black": 0.0, "white": 1.0 }
    },
    {
      "name": "display",
      "modules": { /* 36 dials */ },
      "lut": { "grid": 17, "data": "..." }
    }
  ]
}
```

---

## Stage 1: GeoS Color/Tone Optimizer

Optimizes dials + 17³ LUT using weighted L2 loss in 19D feature space.

**See [geos.md](./geos.md) for full theory and algorithm.**

| Aspect | Value |
|--------|-------|
| **Dials** | 5 scene-linear + 36 display + 17³ LUT |
| **Algorithm** | 3D LUT estimation + SPSA/ACEO/HYBRID |
| **Phases** | Scene-Linear → LUT Estimation → Display (phased step sizes) |
| **Loss** | Weighted L2 in 19D feature space |
| **Time** | ~5 minutes |
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

Optimizes 2 detail dials using frequency-based loss (Laplacian variance).

**See [edge.md](./edge.md) for full theory and algorithm.**

| Aspect | Value |
|--------|-------|
| **Dials** | 2 (sharpen amount, sharpen radius) |
| **Algorithm** | Golden section search (L-channel only) |
| **Loss** | Frequency (Laplacian variance) - content-invariant |
| **Time** | ~2 seconds |

---

## Output: tune.json

Tune outputs a single `tune.json` file containing all optimized settings:

```
<save-area>/tune.json   # 17 color/tone dials + 2 detail dials + 17³ LUT
```

This file is consumed by the `labs` binary via `--tune tune.json`.

### tune.json Format

```json
{
  "version": "1.0",
  "meta": {
    "loss": { "spectral": 0.0005, "frequency": 0.007 },
    "iterations": 342,
    "reference": "sunset_beach.png"
  },
  "link": {
    "name": "tune",
    "modules": {
      "color_correction": {
        "exposure": { "value": 0.65 },
        "white_balance": { "temperature": 0.55, "tint": 0.48 }
      },
      "tone_mapping": {
        "contrast": { "value": 0.72 },
        "highlights": 0.45,
        "shadows": 0.55,
        "toe_pivot": 0.5,
        "shoulder_pivot": 0.5,
        "black_point": 0.35,
        "white_point": 0.85
      },
      "global_color": {
        "vibrance": 0.55,
        "saturation": 0.68,
        "color_density": 0.52
      },
      "split_tone": {
        "shadow_hue": 0.5,
        "shadow_sat": 0.5,
        "highlight_hue": 0.5,
        "highlight_sat": 0.5
      },
      "detail": {
        "sharpen_amount": 0.4,
        "sharpen_radius": 0.5
      }
    }
  },
  "lut": {
    "grid": 17,
    "data": "hex-encoded-uint16-values..."
  }
}
```

### LUT Encoding

The 3D LUT is stored as hex-encoded uint16 values:
- **Grid**: 17³ = 4913 points × 3 channels = 14,739 values
- **Encoding**: Each float [0,1] → uint16 [0,65535] → 4 hex chars
- **Size**: ~59KB hex string (more compact than base64/float at ~78KB)

**Note:** Geometric dials are not included - they are user-controlled per image. Sharpening operates on L-channel only to preserve color accuracy.

---

## Applying Tuned Settings

When applying tuned settings to a new image:

### Command Line (labs binary)

```bash
# Apply tune.json to a RAW file
./labs source.ARW --tune tune.json --output styled.png

# With custom output size
./labs source.ARW --tune tune.json --output styled.png --size 2048
```

### Programmatic Usage

```cpp
// Load tune.json
pipe::Body::Link link = data::link::fromJson(loadFile("tune.json"));

// Build pipeline with tuned link
pipe::Body body;
body.add(link);

// Process RAW through HEAD → BODY → TAIL
pipe::Head head;
cv::UMat linear = head.decode("source.ARW");
cv::UMat display = body.view(linear);
pipe::Tail tail;
tail.save(display, "styled.png", 1080);
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

The 19D style space is projected to a 2D dome compass:

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
// Given feature vectors in R^19
// Compute normalized weighted difference
float weighted_loss = 0.0f;
for (int i = 0; i < 19; i++) {
    float diff = features[i] - target[i];
    weighted_loss += weights[i] * diff * diff;
}
float r = std::sqrt(weighted_loss);

// Project onto semantic axes (μ_L and μ_C from style vector)
float x = (features[3] - target[3]) * weights[3];  // Brightness axis
float y = (features[4] - target[4]) * weights[4];  // Color axis
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
| **Style** (45 dials) | SPSA/ACEO/HYBRID + 19D weighted L2 loss | ~5min |
| **Geometry** (6 dials) | User sets manually | - |

The user's responsibility is simple: **frame your shot**. The tool handles the rest.

**Current limitation**: Pipeline capability gap (42% contrast shortfall). See [base_curve.md](./base_curve.md).

---

## Internal Structure

The tune module is split into focused files:

| File | Purpose |
|------|---------|
| `tune.cpp` | Task class + `make()` factory |
| `diff.cpp` | Loss metrics (spectral, frequency) |
| `geos.cpp` | 3D LUT estimation + SPSA optimizer for color/tone |
| `edge.cpp` | Golden section for sharpness (L-channel only) |
| `data.cpp` | Data ↔ JSON serialization |

See [libs.md](./libs.md) for full source structure.

---

## See Also

- [tldr.md](./tldr.md) - Quick overview
- [geos.md](./geos.md) - GeoS: 19D feature space + SPSA/ACEO/HYBRID algorithm (color/tone)
- [edge.md](./edge.md) - Edge: Frequency loss theory + golden section algorithm (sharpness)
- [diff.md](./diff.md) - Loss metrics redirect
- [data.md](./data.md) - Style sidecar format
- [base_curve.md](./base_curve.md) - Per-camera base curve learning
- [todo.md](./todo.md) - Current status and next steps
