// spsa.hpp
// Internal: SPSA optimizer for color/tone dials
// Not a public header - used only within geos module
//
// Two modes (selected via Config::geos_mode):
//
// BLOCKWISE (4-phase):
//   Phase 1: Block A (10 dials) - ColorCorrection + ToneMapping
//   Phase 2: Block B (3 dials) - GlobalColor
//   Phase 3: Joint A+B (13 dials) - refinement
//   Phase 4: Block C (24 dials) - SelectiveColour polish
//
// FULL_37D (1-phase):
//   All 37 dials optimized simultaneously

#pragma once

#include <geos.hpp>
#include "diff.hpp"

namespace geos::internal
{
    // Total dials: 37 original + 4 split tone + 4 detail = 45
    // Detail dials: sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma
    constexpr int GEOS_DIAL_COUNT = 45;

    // ============================================================
    // Block definitions (for BLOCKWISE mode)
    // ============================================================

    // Block A: ColorCorrection (3) + ToneMapping (7) = 10 dials
    constexpr int BLOCK_A_START = 0;
    constexpr int BLOCK_A_SIZE = 10;

    // Block B: GlobalColor (3) + SplitTone (4) = 7 dials
    constexpr int BLOCK_B_START = 10;
    constexpr int BLOCK_B_SIZE = 7;  // Was 3, now includes 4 split tone dials

    // Joint A+B = 17 dials
    constexpr int BLOCK_AB_SIZE = 17;  // Was 13

    // Block C: SelectiveColour (24) = 24 dials
    constexpr int BLOCK_C_START = 17;  // Was 13
    constexpr int BLOCK_C_SIZE = 24;

    // ============================================================
    // LINEAR_ONLY mode definitions
    // ============================================================
    // Linear dials only (skip ToneMapping indices 3-9):
    //   [0-2]   ColorCorrection: exposure, temperature, tint
    //   [10-12] GlobalColor: vibrance, saturation, colourDensity
    //   [13-16] SplitTone: shadow_temp, shadow_tint, highlight_temp, highlight_tint
    //   [17-40] SelectiveColour: 8 hues × 3 (H/S/L)
    // Total: 34 linear dials

    // Linear Block A: ColorCorrection only (3 dials)
    constexpr int LINEAR_A_START = 0;
    constexpr int LINEAR_A_SIZE = 3;

    // Linear Block B: GlobalColor (3) + SplitTone (4) = 7 dials
    constexpr int LINEAR_B_START = 10;
    constexpr int LINEAR_B_SIZE = 7;

    // Linear Block C: SelectiveColour (24 dials)
    constexpr int LINEAR_C_START = 17;
    constexpr int LINEAR_C_SIZE = 24;

    // ============================================================
    // SCENE_LINEAR mode definitions (two-link architecture)
    // ============================================================
    // Scene-referred dials only (for linear link):
    //   [0] exposure, [1] temperature, [2] tint, [8] black, [9] white
    // Total: 5 non-contiguous dials
    constexpr std::array<int, 5> SCENE_LINEAR_DIALS = {0, 1, 2, 8, 9};
    // BLOCK_5D defined after PhaseParams below

    // ============================================================
    // DISPLAY mode definitions (two-link architecture)
    // ============================================================
    // Display-referred dials only (skip scene-linear dials):
    //   [3-7]   ToneMapping curves: contrast, highlights, shadows, toe, shoulder
    //   [10-16] GlobalColor + SplitTone
    //   [17-40] SelectiveColour
    // Total: 36 dials (excludes 0,1,2,8,9)

    // Display Block A: ToneMapping curves only (5 dials: 3-7)
    constexpr int DISPLAY_A_START = 3;
    constexpr int DISPLAY_A_SIZE = 5;

    // Display Block B: GlobalColor + SplitTone (7 dials: 10-16)
    constexpr int DISPLAY_B_START = 10;
    constexpr int DISPLAY_B_SIZE = 7;

    // Display Block C: SelectiveColour (24 dials: 17-40)
    constexpr int DISPLAY_C_START = 17;
    constexpr int DISPLAY_C_SIZE = 24;

    // ============================================================
    // SPSA hyperparameters
    // ============================================================

    struct PhaseParams
    {
        float a0;     // Initial learning rate
        float c0;     // Initial perturbation size
        float alpha;  // Learning rate decay exponent
        float gamma;  // Perturbation decay exponent
        float A;      // Stability constant
    };

    // Block-specific parameters (lower dims = can be more aggressive)
    // Tuned for 17³ LUT which gives ~1.4% starting loss (vs 4.2% with 9³)
    // Smaller a0/c0 for finer adjustments at lower loss values
    // Default block-specific parameters (can be overridden via Config)
    constexpr PhaseParams DEFAULT_3D  = { 0.08f, 0.03f, 0.602f, 0.101f, 10.0f };
    constexpr PhaseParams DEFAULT_5D  = { 0.20f, 0.12f, 0.602f, 0.101f, 10.0f };
    constexpr PhaseParams DEFAULT_7D  = { 0.12f, 0.06f, 0.602f, 0.101f, 15.0f };
    constexpr PhaseParams DEFAULT_10D = { 0.15f, 0.08f, 0.602f, 0.101f, 20.0f };
    constexpr PhaseParams DEFAULT_17D = { 0.08f, 0.04f, 0.602f, 0.101f, 40.0f };
    constexpr PhaseParams DEFAULT_24D = { 0.02f, 0.008f, 0.602f, 0.101f, 50.0f };
    constexpr PhaseParams DEFAULT_41D = { 0.15f, 0.10f, 0.602f, 0.101f, 30.0f };

    // Helper: get params from config if set (a0 > 0), else use default
    inline PhaseParams getParams(const geos::PhaseParams& cfg, const PhaseParams& def) {
        if (cfg.a0 > 0.0f) {
            return { cfg.a0, cfg.c0, cfg.alpha, cfg.gamma, cfg.A };
        }
        return def;
    }

    // Backward compatibility aliases (used throughout spsa.cpp)
    #define BLOCK_3D  DEFAULT_3D
    #define BLOCK_5D  DEFAULT_5D
    #define BLOCK_7D  DEFAULT_7D
    #define BLOCK_10D DEFAULT_10D
    #define BLOCK_17D DEFAULT_17D
    #define BLOCK_24D DEFAULT_24D
    #define BLOCK_41D DEFAULT_41D

    // ============================================================
    // Iteration split (BLOCKWISE mode)
    // ============================================================
    // When LUT is active, Phase 4 (SelectiveColour) is skipped because
    // the 3D LUT already captures hue-dependent transforms.
    //
    // Phase 1 (A):    35% - establish base exposure/tone (fast convergence)
    // Phase 2 (B):    10% - color saturation (3 dials, quick)
    // Phase 3 (A+B):  55% - joint refinement (the workhorse)
    // Phase 4 (C):    skip when LUT active, otherwise 25%
    constexpr float PHASE1_RATIO = 0.35f;
    constexpr float PHASE2_RATIO = 0.10f;
    constexpr float PHASE3_RATIO = 0.55f;
    constexpr float PHASE4_RATIO = 0.25f;  // Only used when no LUT

    // Early termination: stop phase if no meaningful improvement for N iterations
    constexpr int STALL_THRESHOLD = 20;  // Reduced from 30

    // Minimum improvement to count as "meaningful" (relative to current loss)
    // If improvement < current_loss * MIN_RELATIVE_IMPROVEMENT, count as stall
    constexpr float MIN_RELATIVE_IMPROVEMENT = 0.01f;  // 1% relative improvement

    // Absolute minimum improvement threshold
    constexpr float MIN_ABSOLUTE_IMPROVEMENT = 0.0001f;  // 0.01% absolute

    // Convergence threshold (stop if loss below this)
    constexpr float CONVERGE_THRESHOLD = 0.005f;  // 0.5% (tighter with LUT)

    // ============================================================
    // Shared dial utilities (used by both SPSA and ACEO)
    // ============================================================

    // Dial values as float array
    using Theta = std::array<float, GEOS_DIAL_COUNT>;

    // ============================================================
    // Jacobian-informed gradient (feedforward from features)
    // ============================================================

    // Jacobian matrix: J[dial][feature] = dfeature/ddial
    using JacobianMatrix = std::array<std::array<float, STYLE_DIM>, GEOS_DIAL_COUNT>;

    // Load Jacobian from etc/jacob.json
    bool loadJacobian(const std::string& path, JacobianMatrix& J);

    // Compute analytic gradient from feature residual
    // gradient[d] = sum_f( J[d][f] * weight[f] * (target[f] - current[f]) )
    void computeJacobianGradient(
        const JacobianMatrix& J,
        const StyleFeatures& target,
        const StyleFeatures& current,
        Theta& gradient);

    // Read current dial values from link into theta
    void readDials(pipe::Body::Link& link, Theta& theta);

    // Write theta values to link dials
    void writeDials(pipe::Body::Link& link, const Theta& theta);

    // Initialize theta to neutral (0.5 for all dials)
    void initNeutral(Theta& theta);

    // Compute loss for current body state (global only)
    float evaluateLoss(
        pipe::Body& body,
        const StyleFeatures& targetStyle);

    // Compute combined loss: spectral + frequency (for holistic optimization)
    // Used by --full mode where edge dials are optimized together with color/tone
    float evaluateCombinedLoss(
        pipe::Body& body,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        float freqWeight = 0.15f);

    // ============================================================
    // Optimizer entry point
    // ============================================================

    // Run SPSA optimization (mode selected via config.geos_mode)
    // Returns number of iterations performed
    // lutEstimated: if true, skip Phase 4 (SelectiveColour) since LUT captures hue transforms
    // targetFeatures: optional regional features for DISPLAY mode refinement
    int optimizeGeos(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress,
        bool lutEstimated = false,
        const TargetFeatures* targetFeatures = nullptr
    );

} // namespace geos::internal
