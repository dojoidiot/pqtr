// geos.hpp
// Internal: SPSA optimizer for color/tone dials
// Not a public header - used only within tune module
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

#include <tune.hpp>
#include "diff.hpp"

namespace tune::internal
{
    // Total dials
    constexpr int GEOS_DIAL_COUNT = 37;

    // ============================================================
    // Block definitions (for BLOCKWISE mode)
    // ============================================================

    // Block A: ColorCorrection (3) + ToneMapping (7) = 10 dials
    constexpr int BLOCK_A_START = 0;
    constexpr int BLOCK_A_SIZE = 10;

    // Block B: GlobalColor (3) = 3 dials
    constexpr int BLOCK_B_START = 10;
    constexpr int BLOCK_B_SIZE = 3;

    // Joint A+B = 13 dials
    constexpr int BLOCK_AB_SIZE = 13;

    // Block C: SelectiveColour (24) = 24 dials
    constexpr int BLOCK_C_START = 13;
    constexpr int BLOCK_C_SIZE = 24;

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
    constexpr PhaseParams BLOCK_3D  = { 0.20f, 0.08f, 0.602f, 0.101f, 20.0f };
    constexpr PhaseParams BLOCK_10D = { 0.12f, 0.05f, 0.602f, 0.101f, 35.0f };
    constexpr PhaseParams BLOCK_13D = { 0.07f, 0.025f, 0.602f, 0.101f, 55.0f };
    constexpr PhaseParams BLOCK_24D = { 0.05f, 0.02f, 0.602f, 0.101f, 80.0f };  // Selective: careful
    constexpr PhaseParams BLOCK_37D = { 0.025f, 0.012f, 0.602f, 0.101f, 110.0f }; // Full: very careful

    // ============================================================
    // Iteration split (BLOCKWISE mode)
    // ============================================================
    // Phase 1 (A):    30% - establish base exposure/tone
    // Phase 2 (B):    15% - color saturation
    // Phase 3 (A+B):  30% - joint refinement
    // Phase 4 (C):    25% - selective color polish
    constexpr float PHASE1_RATIO = 0.30f;
    constexpr float PHASE2_RATIO = 0.15f;
    constexpr float PHASE3_RATIO = 0.30f;
    constexpr float PHASE4_RATIO = 0.25f;

    // Convergence threshold (stop if loss below this)
    constexpr float CONVERGE_THRESHOLD = 0.01f;  // 1%

    // ============================================================
    // Optimizer entry point
    // ============================================================

    // Run SPSA optimization (mode selected via config.geos_mode)
    // Returns number of iterations performed
    int optimizeGeos(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress
    );

} // namespace tune::internal
