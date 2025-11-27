// geos.hpp
// Internal: SPSA optimizer for color/tone dials
// Not a public header - used only within tune module
//
// Two modes (selected via Config::geos_mode):
//
// BLOCKWISE (4-phase):
//   Phase 1: Block A (8 dials) - ColorCorrection + ToneMapping
//   Phase 2: Block B (3 dials) - GlobalColor
//   Phase 3: Joint A+B (11 dials) - refinement
//   Phase 4: Block C (24 dials) - SelectiveColour polish
//
// FULL_35D (1-phase):
//   All 35 dials optimized simultaneously

#pragma once

#include <tune.hpp>
#include "diff.hpp"

namespace tune::internal
{
    // Total dials
    constexpr int GEOS_DIAL_COUNT = 35;

    // ============================================================
    // Block definitions (for BLOCKWISE mode)
    // ============================================================

    // Block A: ColorCorrection (3) + ToneMapping (5) = 8 dials
    constexpr int BLOCK_A_START = 0;
    constexpr int BLOCK_A_SIZE = 8;

    // Block B: GlobalColor (3) = 3 dials
    constexpr int BLOCK_B_START = 8;
    constexpr int BLOCK_B_SIZE = 3;

    // Joint A+B = 11 dials
    constexpr int BLOCK_AB_SIZE = 11;

    // Block C: SelectiveColour (24) = 24 dials
    constexpr int BLOCK_C_START = 11;
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
    constexpr PhaseParams BLOCK_8D  = { 0.15f, 0.06f, 0.602f, 0.101f, 30.0f };
    constexpr PhaseParams BLOCK_3D  = { 0.20f, 0.08f, 0.602f, 0.101f, 20.0f };
    constexpr PhaseParams BLOCK_11D = { 0.08f, 0.03f, 0.602f, 0.101f, 50.0f };
    constexpr PhaseParams BLOCK_24D = { 0.05f, 0.02f, 0.602f, 0.101f, 80.0f };  // Selective: careful
    constexpr PhaseParams BLOCK_35D = { 0.03f, 0.015f, 0.602f, 0.101f, 100.0f }; // Full: very careful

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
