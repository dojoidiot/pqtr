// geos.hpp
// Internal: SPSA optimizer for color/tone dials (35 parameters)
// Not a public header - used only within tune module

#pragma once

#include <tune.hpp>
#include "diff.hpp"

namespace tune::internal
{
    // SPSA hyperparameters (from Spall 1992)
    struct SPSAParams
    {
        float a0 = 0.16f;      // Initial learning rate
        float c0 = 0.05f;      // Initial perturbation size
        float alpha = 0.602f;  // Learning rate decay
        float gamma = 0.101f;  // Perturbation decay
        float A = 100.0f;      // Stability constant
    };

    // Run SPSA optimization on color/tone dials
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
