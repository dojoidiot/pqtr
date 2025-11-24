# Diff Tool Specification 

[back](../README.md)

## Purpose

The `diff` tool is a library and a headless command-line program designed to compute and report the perceptual difference between two images. It is a core component of the `tune` optimization process, providing the loss metric that the optimizer seeks to minimize.

## Functionality

1.  **Perceptual Loss**: Computes a single floating-point value representing the weighted perceptual difference between a `candidate` image and a `target` image.
2.  **Visual Diff**: Generates a visual difference image that highlights where the two images differ.
3.  **Standalone Operation**: Can be used from the command line independently of the `tune` optimizer.
4.  **Dual-Mode Operation**: Provides a fast, single loss score for optimization loops (default), and an optional, detailed grid-based report for human analysis.

---

## Command-Line Usage

The `diff` tool can be used directly from the command line.

**Compute loss only (Optimizer Mode)**:
```bash
./diff candidate.png target.png
# Output: Loss: 0.0234 (2.34%)
```

**Generate a visual diff image**:
```bash
./diff candidate.png target.png --visual-diff diff.png --scale 5.0
```

**Generate a grid-based report for user analysis**:
```bash
./diff candidate.png target.png --grid-report 4x4
# Output:
# Loss: 0.0234 (2.34%)
#
# Grid Loss Report (4x4):
# [ 0.01, 0.02, 0.15, 0.12 ]
# [ 0.01, 0.02, 0.14, 0.11 ]
# [ 0.03, 0.03, 0.08, 0.05 ]
# [ 0.04, 0.04, 0.06, 0.04 ]
#
# Analysis: High deviation detected in top-right quadrant.
```

---

## Grid-Based Anomaly Detection

For human analysis, the `diff` tool can provide a grid-based report to localize differences and spot structural anomalies.

### Process
1.  The tool first computes the full-resolution perceptual difference map, just as it does for the global score.
2.  It overlays a virtual grid (e.g., 4x4, 8x8, specified by the user) onto this difference map.
3.  It then calculates the average loss score within each individual cell of the grid.
4.  The resulting matrix of scores is printed as a "heat map," providing an intuitive summary of where the `candidate` and `target` images differ most.

This allows a user to quickly identify if a difference is global or confined to a specific region of the image.

---

## Core Logic

### Perceptual Loss Function

The core of the `diff` tool is its loss function, which combines three different metrics to create a score that aligns with human perception. **All color comparisons are performed in perceptually uniform color spaces** to ensure that the `tune` optimizer converges on visually accurate results rather than mathematically "close" but perceptually poor matches.

**Formula**:
```
loss = 0.5 * (1.0 - SSIM) + 0.3 * color_diff_lch + 0.2 * lum_diff
```

**Component Metrics**:

*   **1. SSIM (Structural Similarity) - 50% weight**
    *   Measures perceptual structural differences, capturing changes in texture, detail, and overall structure. An SSIM value of 1.0 means the images are identical.
    *   SSIM accounts for luminance, contrast, and structure in a way that correlates with human perception.

*   **2. Color Difference (CIELAB LCh space) - 30% weight**
    *   Calculates the average color difference in the CIELAB LCh color space, which is designed to be perceptually uniform.
    *   **Why CIELAB?** Simple RGB pixel differences don't correlate with human perception. A large numerical change in dark blue might be imperceptible, while a small numerical change in a skin tone could be jarring. CIELAB ensures differences are weighted appropriately.
    *   Implementation uses OpenCV's `cv::cvtColor(img, COLOR_BGR2Lab)` followed by deltaE calculations.

*   **3. Luminance Difference - 20% weight**
    *   Measures the average difference in brightness in a perceptually linear space.

### Visual Difference Image

To help visualize the differences, the tool can generate a "visual diff" image.

**Process**:
1.  Both `candidate` and `target` images are converted to a floating-point format.
2.  The absolute difference between them is calculated pixel by pixel.
3.  The resulting differences are amplified by a scaling factor (e.g., 5x) to make subtle changes visible.
4.  The result is converted back to a displayable 8-bit image.

---

## API Interface

The following C++ interface defines the `diff` tool's programmatic functionality. The core `compute` method remains focused on delivering a single loss score for performance-critical optimization loops. The grid analysis is intended as a feature of the command-line tool's reporting.

```cpp
namespace pqtr {

struct DiffMetrics {
    float ssim;           // [0, 1] where 1 = identical
    float color_diff;     // [0, 1] color difference in LCh
    float lum_diff;       // [0, 1] luminance difference
    float total_loss;     // Weighted combination
};

class Diff {
public:
    // Compute all metrics for a single global score
    DiffMetrics compute(
        const cv::UMat& candidate,
        const cv::UMat& target
    );

    // Generate visual diff image
    cv::UMat visualDiff(
        const cv::UMat& candidate,
        const cv::UMat& target,
        float scale = 5.0  // Amplification
    );

private:
    float computeSSIM(const cv::UMat& img1, const cv::UMat& img2);
    float computeColorDiffLCh(const cv::UMat& img1, const cv::UMat& img2);
    float computeLuminanceDiff(const cv::UMat& img1, const cv::UMat& img2);
};

} // namespace pqtr
```

**Dependencies**:
- OpenCV (core and quality modules for UMat and SSIM)
