// tune.hpp
// Public API for automatic style matching optimization
//
// Unified module providing:
// - Loss measurement (spectral + frequency metrics)
// - GEOS optimizer (35 color/tone dials via SPSA)
// - Edge optimizer (4 detail dials via golden section)
//
// PIMPL design caches target image features for efficient
// repeated comparisons during optimization.
//
// User apps include only this header and link against labs.a.
// For serialization, see data.hpp.

#pragma once

#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/core.hpp>
#include <functional>
#include <cmath>

namespace tune
{

    // GPU-accelerated image matrix (BGR 8-bit).
    // Reference-counted internally by OpenCV - shallow copy shares data.
    // Returned Views are read-only references; do not modify.
    using View = cv::UMat;

    // ============================================================
    // Data - Loss metrics (value type)
    // ============================================================

    struct Data
    {
        float spectral = 0.0f;  // [0, 1] geodesic distance (0 = identical color/tone)
        float frequency = 0.0f; // [0, ∞) relative variance difference (0 = identical sharpness)
    };

    // ============================================================
    // Result - Optimization outcome
    // ============================================================

    struct Result
    {
        Data loss;             // Final loss values
        int geos_iterations;   // SPSA iterations used
        int edge_evaluations;  // Golden section evaluations
    };

    // ============================================================
    // Config - Optimization parameters
    // ============================================================

    struct Config
    {
        bool skip_geos = false;        // Skip color/tone optimization
        bool skip_edge = false;        // Skip sharpness optimization
        int geos_max_iter = 500;       // Max SPSA iterations
        int geos_multi_starts = 5;     // Number of random initializations
        float geos_threshold = 0.01f;  // Stop when spectral loss below this
        float edge_tolerance = 0.01f;  // Golden section convergence tolerance
    };

    // ============================================================
    // Progress - Feedback for GUI visualization
    // ============================================================

    struct Progress
    {
        enum class Stage { GEOS, EDGE } stage;

        // GEOS coarse-to-fine phases (only meaningful when stage == GEOS)
        // HUGE: Large perturbations, explore parameter space
        // MIDS: Medium perturbations, refine within basin
        // TINY: Small perturbations, precise convergence
        enum class Phase { HUGE, MIDS, TINY } phase;

        int iteration;
        int max_iterations;

        Data loss;  // Current loss values

        // GEOS: 2D dome compass (style space projection)
        // Target at north pole, candidate position shows error
        struct Dome
        {
            float r;      // [0,1] radial distance from target (0 = converged)
            float theta;  // [0,2π] semantic direction of error

            float x() const { return r * std::cos(theta); }
            float y() const { return r * std::sin(theta); }
        } dome;

        // EDGE: 1D sharpness slider
        struct Edge
        {
            float ratio;  // var_cand / var_ref: 1.0 = matched, >1 = sharper, <1 = softer
        } edge;
    };

    // Progress callback - return false to abort optimization
    using Callback = std::function<bool(const Progress&)>;

    // ============================================================
    // Task - Cached target features + optimization
    // ============================================================

    // Task holds cached target image features for efficient repeated comparisons.
    // Ownership: Hold<Task> owns the task; returned Views share underlying data.
    // Created via tune::make(target), destroyed via RAII.
    class Task
    {
    public:
        virtual ~Task() = default;

        // Access cached target image (read-only reference)
        virtual View target() = 0;

        // Compute loss metrics between candidate and cached target
        virtual Data diff(View candidate) = 0;

        // Compute visual diff image (amplified pixel difference)
        virtual View view(View candidate, float scale = 5.0f) = 0;

        // Run optimization - modifies link dials in-place
        // Returns result with final loss and iteration counts
        virtual Result run(pipe::Body& body, pipe::Body::Link& link,
                          const Config& config = Config(),
                          Callback progress = nullptr) = 0;
    };

    // Factory: create Task with target image (features cached for reuse)
    pqtr::Hold<Task> make(View target);

} // namespace tune
