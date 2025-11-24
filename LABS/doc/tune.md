# Tune Tool Specification 

[back](../README.md)

## Purpose

The `tune` tool is a library and a headless command-line program that automatically finds the optimal pipe dial values required to make a source RAW image visually match a target-styled image. It enables automatic style transfer and preset creation.

It works by using the `diff` tool as a feedback mechanism, iteratively adjusting dials to minimize the perceptual loss between the pipe's output and the target image.

## Prerequisites

**Geometric Alignment**: The `pipe` contains modules for both geometric and creative adjustments. However, the `tune` tool is specifically designed to optimize only the **39 creative dials** (color, tone, etc. - 45 total minus 6 geometric).

Therefore, it is the **user's responsibility** to ensure the source and target images are geometrically aligned *before* using this tool. `tune` does not solve for differences in crop, scale, rotation, or perspective.

---

## Core Algorithm: A Two-Stage Approach

The optimization process is designed to be both fast and effective, focusing on the most impactful dials first.

### Stage 1: Sensitivity Analysis

The tool first identifies which of the pipe's 39 creative dials have the most significant visual impact.

1.  The source RAW is processed with all dials at their default neutral values to get a baseline image.
2.  For each dial, it is perturbed by a small amount (e.g., +10%), and the RAW is processed again.
3.  The `diff` tool calculates the perceptual loss between each perturbed result and the target image.
4.  The change in loss (`|loss_delta|`) determines the sensitivity of that dial.
5.  All dials are ranked by their sensitivity, from most impactful to least impactful.

**Output**: A ranked list of dials that contribute most to the visual difference.

### Stage 2: Greedy Optimization

Using the sensitivity analysis, the tool intelligently searches for the best dial values.

1.  Only dials with a sensitivity above a certain threshold (e.g., 5% contribution) are selected for optimization.
2.  The tool proceeds down the ranked list of high-impact dials.
3.  For each dial, it performs a 1D search (using an efficient algorithm like Golden Section Search) to find the optimal value in its `[0.0, 1.0]` range that minimizes the loss score from the `diff` tool.
4.  Once a dial's optimal value is found, it is fixed, and the process continues to the next dial in the list.

**Output**: A final set of dial values that best reproduces the target style.

---

## Command-Line Usage

```bash
# Full optimization, saving the result to a labs-compatible JSON file
./tune reference.arw target.png --output dials.json

# Run with a real-time visualization window (requires a display)
./tune reference.arw target.png --output dials.json --visualize

# Adjust the sensitivity threshold to only optimize more impactful dials
./tune reference.arw target.png --threshold 0.10 --output dials.json
```

---

## Performance & Limitations

### Performance
The optimization is designed to be fast, typically converging in under 10 seconds on a modern GPU.
- **Simple adjustments** (e.g., exposure, saturation): **1-2 seconds**
- **Complex color grades**: **5-8 seconds**

### Limitations
1.  **Non-Unique Solutions**: Different combinations of dials can produce visually similar results. The tool finds **a** valid solution, but not necessarily the *exact* one used to create the target.
2.  **Overlapping Effects**: Some dials have overlapping visual effects (e.g., `vibrance` and `saturation`), which can make isolating the exact original change difficult.
3.  **Irreversible Operations**: The tool cannot reverse-engineer information loss from destructive operations like heavy JPEG compression artifacts or clipped highlights in the target image.

---

## API Interface

The following C++ interface defines the `tune` tool's programmatic functionality.

```cpp
namespace pqtr {

// Describes the state of the optimization for progress callbacks.
struct OptimizationProgress {
    enum Stage {
        SENSITIVITY_ANALYSIS,
        DIAL_OPTIMIZATION,
        COMPLETE
    };

    Stage stage;
    int total_steps;
    int current_step;

    // Stage-specific data
    int dial_index;
    float dial_value;
    float current_loss;
    float* current_dials;  // Array of all current dial values

    // Visual feedback
    cv::UMat current_candidate;
    cv::UMat visual_diff;
};

using ProgressCallback = std::function<void(const OptimizationProgress&)>;

// The final output of a tune operation.
struct TuneResult {
    float dials[39];
    float sensitivities[39];
    float final_loss;
    int num_optimized;
    double computation_time_seconds;
};

class Tune {
public:
    Tune(Pipe& pipe, Diff& diff);

    // Main entry point for the optimization process.
    TuneResult optimize(
        const cv::UMat& source_raw,       // From raws part
        const cv::UMat& target_styled,
        float sensitivity_threshold = 0.05,
        ProgressCallback callback = nullptr
    );

private:
    Pipe& pipe_;
    Diff& diff_;

    // Stage 1: Sensitivity analysis
    void computeSensitivities(
        const cv::UMat& source,
        const cv::UMat& target,
        float* out_sensitivities,
        ProgressCallback callback
    );

    // Stage 2: Greedy optimization
    void optimizeGreedy(
        const cv::UMat& source,
        const cv::UMat& target,
        const float* sensitivities,
        float threshold,
        float* inout_dials,
        ProgressCallback callback
    );

    // Helper for 1D search during greedy optimization
    float optimizeDial1D(
        int dial_index,
        const cv::UMat& source,
        const cv::UMat& target,
        float* current_dials,
        ProgressCallback callback
    );
};

} // namespace pqtr
```
