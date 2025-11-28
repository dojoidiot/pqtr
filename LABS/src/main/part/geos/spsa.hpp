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
    // Total dials: 37 original + 4 split tone = 41
    constexpr int GEOS_DIAL_COUNT = 41;

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
    constexpr PhaseParams BLOCK_3D  = { 0.08f, 0.03f, 0.602f, 0.101f, 10.0f };
    constexpr PhaseParams BLOCK_7D  = { 0.06f, 0.025f, 0.602f, 0.101f, 15.0f }; // GlobalColor + SplitTone
    constexpr PhaseParams BLOCK_10D = { 0.05f, 0.02f, 0.602f, 0.101f, 20.0f };
    constexpr PhaseParams BLOCK_17D = { 0.025f, 0.01f, 0.602f, 0.101f, 40.0f }; // Joint A+B (was 13D)
    constexpr PhaseParams BLOCK_24D = { 0.02f, 0.008f, 0.602f, 0.101f, 50.0f };  // Selective: careful
    constexpr PhaseParams BLOCK_41D = { 0.01f, 0.005f, 0.602f, 0.101f, 80.0f }; // Full: very careful

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

    // Early termination: stop phase if no improvement for N iterations
    constexpr int STALL_THRESHOLD = 30;

    // Convergence threshold (stop if loss below this)
    constexpr float CONVERGE_THRESHOLD = 0.005f;  // 0.5% (tighter with LUT)

    // ============================================================
    // Optimizer entry point
    // ============================================================

    // Run SPSA optimization (mode selected via config.geos_mode)
    // Returns number of iterations performed
    // lutEstimated: if true, skip Phase 4 (SelectiveColour) since LUT captures hue transforms
    int optimizeGeos(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress,
        bool lutEstimated = false
    );

} // namespace geos::internal
