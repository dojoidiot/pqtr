// geos.hpp
// Public API for automatic style matching optimization
//
// Unified module providing:
// - Loss measurement (spectral + frequency metrics)
// - GEOS optimizer (45 style dials via SPSA or ACEO)
// - Edge optimizer (4 detail dials via golden section)
//
// SPSA explores full dial space and builds covariance.
// ACEO uses prior covariance for efficient eigenspace search.
// Together they form a complementary optimization pair.
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

namespace geos
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
        int geos_iterations;   // Optimizer iterations (SPSA or ACEO)
        int edge_evaluations;  // Golden section evaluations
    };

    // ============================================================
    // Config - Optimization parameters
    // ============================================================

    // GEOS optimization strategy
    enum class Mode
    {
        BLOCKWISE,     // 4-phase: A(10) → B(7) → AB(17) → C(24) selective
        FULL_35D,      // Single-phase: all 45 dials simultaneously
        LINEAR_ONLY,   // Linear ops only: skip ToneMapping (dials 3-9)
        SCENE_LINEAR,  // Scene-referred: exposure(0), temp(1), tint(2), black(8), white(9) only
        DISPLAY,       // Display-referred: skip scene-linear dials, optimize rest + LUT
        STAGED         // Stage-aware: VIEW (6 tone dials) then POPS (39 color dials)
    };

    // Optimizer algorithm selection
    enum class Optimizer
    {
        SPSA,   // Simultaneous Perturbation Stochastic Approximation (default)
        ACEO,   // Adaptive Covariance Evolver Optimiser (CMA-ES with prior)
        HYBRID  // ACEO for direction (pop), then SPSA for polish (photographic quality)
    };

    // SPSA phase parameters (learnable)
    struct PhaseParams
    {
        float a0 = 0.0f;     // Initial learning rate (0 = use default)
        float c0 = 0.0f;     // Initial perturbation size (0 = use default)
        float alpha = 0.602f;  // Learning rate decay exponent
        float gamma = 0.101f;  // Perturbation decay exponent
        float A = 20.0f;       // Stability constant
    };

    struct Config
    {
        bool skip_geos = false;        // Skip color/tone optimization
        bool skip_edge = false;        // Skip sharpness optimization
        bool skip_lut = false;         // Skip LUT curve estimation (for true linear-only)
        bool skip_regional = false;    // Skip regional refinement (faster, simpler)
        int geos_max_iter = 200;       // Max optimizer iterations (with early-stop)
        int geos_multi_starts = 5;     // Number of random initializations
        float geos_threshold = 0.005f; // Stop when spectral loss below this (0.5%)
        float edge_tolerance = 0.01f;  // Golden section convergence tolerance
        Mode geos_mode = Mode::BLOCKWISE;  // Optimization strategy
        Optimizer optimizer = Optimizer::SPSA;  // Algorithm selection (SPSA or ACEO)

        // Covariance options (SPSA builds, ACEO uses)
        std::string aceo_with_cov;     // Path to prior covariance (blend with accumulated)
        std::string aceo_save_cov;     // Path to save accumulated covariance after run

        // Trained phase params (optional - 0 values use compiled defaults)
        // Load from etc/prms.json for trained values
        PhaseParams prms_10d;  // Block A: ColorCorrection + ToneMapping
        PhaseParams prms_17d;  // Joint A+B refinement
        PhaseParams prms_7d;   // Block B: GlobalColor + SplitTone
        PhaseParams prms_24d;  // Block C: SelectiveColour
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
    // Created via geos::make(target), destroyed via RAII.
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

} // namespace geos
