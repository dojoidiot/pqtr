# Diff Tool Specification

[back](../README.md)

## Purpose

The `diff` tool computes loss metrics between images for the `tune` optimizer. It provides two complementary metrics:

- **Spectral loss**: Measures color/tone similarity (content-invariant)
- **Frequency loss**: Measures sharpness similarity

These correspond to the two automated stages of tuning. See [tune.md](./tune.md) for the three roles of tuning.

---

## Workflow

The diff tool is used in the tune R&D workflow to measure the gap between scene-referred output and camera rendering:

```
RAW File
    │
    ▼
┌─────────┐
│  HEAD   │ ──► head.png (camera preview - TARGET)
└────┬────┘
     │
     ▼
┌─────────┐
│  BODY   │ ──► body.png (scene-referred - CANDIDATE)
│ (empty) │
└────┬────┘
     │
     ▼
┌─────────┐
│  DIFF   │ ──► diff.json (loss metrics)
│         │ ──► diff.png  (visual difference)
└─────────┘
```

**Comparison**: `diff(body, head)` measures how far scene-referred output is from camera rendering.

**Goal**: Tune finds edit steps that minimize this loss, so `tail.png` matches `head.png`.

---

## Two Metrics for Two Properties

| Metric | Measures | Used By | Content-Invariant |
|--------|----------|---------|-------------------|
| **Spectral** | Color, tone, mood | SPSA optimizer (35 dials) | Yes |
| **Frequency** | Sharpness, texture | Edge optimizer (4 dials) | Yes |

### Why Two Metrics?

A single metric cannot capture both:
- Spectral features (SVD, statistics) ignore spatial frequency → blind to sharpness
- Frequency features (Laplacian) ignore color distribution → blind to tone

---

## Spectral Loss (Color/Tone)

Measures style similarity using geodesic distance on a feature hypersphere. See [geos.md](./geos.md) for full theory.

### Feature Extraction

1. Resize to 512×512 proxy
2. Convert to LCH with chroma-weighted hue
3. Compute SVD singular values
4. Build style vector:

```
style = [σ₁, σ₂, σ₃, μ_L, μ_C, std_L, std_C, skew_L, cov(L,C), cov(H,C)]
```

5. Normalize to unit hypersphere: `ψ = style / ||style||`

### Loss Function

```
spectral_loss = 1 - |⟨ψ_candidate | ψ_reference⟩|²
```

- Loss = 0: Identical color/tone style
- Loss = 1: Maximally different styles

### Properties

- **Content-invariant**: Works across different scenes
- **Geometry-invariant**: No alignment required
- **Fast**: <5ms per image (512×512 proxy)

---

## Frequency Loss (Sharpness)

Measures sharpness similarity using Laplacian variance.

### Laplacian Variance

The Laplacian operator highlights edges and fine detail:

```cpp
cv::Laplacian(gray, laplacian, CV_64F);
variance = laplacian.var();
```

Higher variance = sharper image (more edges, more detail).

### Loss Function

```
frequency_loss = |var(candidate) - var(reference)| / var(reference)
```

- Loss = 0: Identical sharpness
- Loss > 0: Different sharpness levels

### Properties

- **Content-invariant**: Measures overall sharpness, not specific edges
- **Fast**: <2ms per image
- **Intuitive**: Directly maps to perceived sharpness

---

## Command-Line Usage

```bash
# Compute both metrics
./diff candidate.png reference.png
# Output:
# Spectral Loss: 0.0156 (1.56%)
# Frequency Loss: 0.0234 (2.34%)

# Spectral only
./diff candidate.png reference.png --spectral
# Output: Spectral Loss: 0.0156

# Frequency only
./diff candidate.png reference.png --frequency
# Output: Frequency Loss: 0.0234

# Extract style features (for debugging)
./diff image.png --extract-style
# Output: Style vector: [σ₁=0.82, σ₂=0.31, ...]

# Visual diff (pixel difference, scaled)
./diff candidate.png reference.png --visual-diff output.png --scale 5.0
```

### Options

| Option | Description |
|--------|-------------|
| `--spectral` | Output spectral loss only |
| `--frequency` | Output frequency loss only |
| `--extract-style` | Show style feature vector |
| `--visual-diff FILE` | Output visual difference image |
| `--scale N` | Amplification for visual diff (default: 5.0) |

---

## Visual Difference Output

For debugging, generates an amplified pixel difference image:

1. Convert both images to float
2. Compute absolute difference per pixel
3. Amplify by scale factor (default: 5×)
4. Output as viewable image

Note: Visual diff shows pixel differences, not the metrics used for optimization.

---

## API Interface

### diff.hpp (Core API)

PIMPL design enables caching of base image features for efficient repeated comparisons during tune optimization.

```cpp
namespace diff {

    // GPU-accelerated image matrix (BGR 8-bit).
    // Reference-counted internally by OpenCV - shallow copy shares data.
    // Returned Views are read-only references; do not modify.
    using View = cv::UMat;

    // Combined metrics result (value type)
    struct Data {
        float spectral = 0.0f;   // [0, 1] geodesic distance (0 = identical color/tone)
        float frequency = 0.0f;  // [0, ∞) relative variance difference (0 = identical sharpness)
    };

    // Task holds cached base image features for efficient repeated comparisons.
    // Ownership: Hold<Task> owns the task; returned Views share underlying data.
    // Created via diff::make(base), destroyed via RAII.
    class Task {
    public:
        virtual ~Task() = default;
        virtual View base() = 0;                             // access cached base (read-only)
        virtual Data diff(View test) = 0;                    // compute metrics
        virtual View view(View test, float scale = 5.0f) = 0; // compute diff image
    };

    // Factory: create Task with base image (features cached for reuse)
    pqtr::Hold<Task> make(View base);

} // namespace diff
```

### Ownership

| Type | Ownership | Notes |
|------|-----------|-------|
| `Hold<Task>` | Caller owns | RAII cleanup when Hold goes out of scope |
| `View` | Shared | OpenCV reference-counted; shallow copy shares GPU buffer |
| `Data` | Caller owns | Value type (two floats); safe to copy |

**Contract**: Returned `View` objects are read-only references to internal data. Do not modify.

### data.hpp (Serialization)

Serialization is handled by the data layer (separation of concerns):

```cpp
namespace data::diff {

    std::string toJson(const ::diff::Data& d);
    ::diff::Data fromJson(const std::string& json);

    bool save(const ::diff::Data& d, const std::string& path);
    ::diff::Data load(const std::string& path);

} // namespace data::diff
```

### Usage Example

```cpp
#include <diff.hpp>
#include <data.hpp>

// head = target (camera preview), body = candidate (scene-referred)
diff::View head, body;
// ... load images ...

// Create diff task with head as base (features cached)
pqtr::Hold<diff::Task> task = diff::make(head);

// Access base image (read-only reference)
diff::View base = task->base();

// Compute metrics (base features reused for each call)
diff::Data m = task->diff(body);
std::cout << "Spectral: " << m.spectral << std::endl;   // e.g., 0.0248
std::cout << "Frequency: " << m.frequency << std::endl; // e.g., 0.8146

// Get diff image for visualization
diff::View diffImg = task->view(body);
cv::imwrite("diff.png", diffImg);

// Save/load metrics (via data layer)
data::diff::save(m, "diff.json");
diff::Data loaded = data::diff::load("diff.json");
```

### JSON Format

```json
{
  "spectral": 0.024843,
  "frequency": 0.814570
}
```

---

## Performance

| Metric | Time | Notes |
|--------|------|-------|
| Spectral loss | <5ms | 512×512 proxy, SVD |
| Frequency loss | <2ms | Single Laplacian + variance |
| Visual diff | <10ms | Full resolution |

---

## See Also

- [geos.md](./geos.md) - Spectral loss theory
- [tune.md](./tune.md) - How metrics are used in optimization
- [test.md](./test.md) - Test cases for both metrics
