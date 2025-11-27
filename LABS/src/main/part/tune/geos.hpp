// geos.hpp
// Internal: Block-wise SPSA optimizer for color/tone dials
// Not a public header - used only within tune module
//
// Block Strategy:
//   Phase 1: Block A (8 dials) - ColorCorrection + ToneMapping
//   Phase 2: Block B (3 dials) - GlobalColor
//   Phase 3: Joint A+B (11 dials) - final polish
//   (SelectiveColour 24 dials skipped - likely noise)

#pragma once

#include <tune.hpp>
#include "diff.hpp"

namespace tune::internal
{
    // Total dials (for storage, though we only optimize 11)
    constexpr int GEOS_DIAL_COUNT = 35;

    // Block definitions
    // Block A: ColorCorrection (3) + ToneMapping (5) = 8 dials
    constexpr int BLOCK_A_START = 0;
    constexpr int BLOCK_A_SIZE = 8;

    // Block B: GlobalColor (3) = 3 dials
    constexpr int BLOCK_B_START = 8;
    constexpr int BLOCK_B_SIZE = 3;

    // Joint A+B = 11 dials
    constexpr int BLOCK_AB_SIZE = 11;

    // SPSA hyperparameters - tuned for lower dimensions
    struct PhaseParams
    {
        float a0;     // Initial learning rate
        float c0;     // Initial perturbation size
        float alpha;  // Learning rate decay exponent
        float gamma;  // Perturbation decay exponent
        float A;      // Stability constant
    };

    // Block-specific parameters (lower dims = can be more aggressive)
    // 8D block: moderate exploration
    constexpr PhaseParams BLOCK_8D = { 0.15f, 0.06f, 0.602f, 0.101f, 30.0f };
    // 3D block: can explore more aggressively
    constexpr PhaseParams BLOCK_3D = { 0.20f, 0.08f, 0.602f, 0.101f, 20.0f };
    // 11D joint: careful refinement
    constexpr PhaseParams BLOCK_11D = { 0.08f, 0.03f, 0.602f, 0.101f, 50.0f };

    // Iterations per phase (configurable via config.geos_max_iter)
    // Split: 40% Block A, 20% Block B, 40% Joint
    constexpr float PHASE1_RATIO = 0.40f;
    constexpr float PHASE2_RATIO = 0.20f;
    constexpr float PHASE3_RATIO = 0.40f;

    // Convergence threshold (stop if loss below this)
    constexpr float CONVERGE_THRESHOLD = 0.01f;  // 1%

    // Run block-wise SPSA optimization
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
