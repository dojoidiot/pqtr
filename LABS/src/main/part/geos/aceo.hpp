// aceo.hpp
// Internal: ACEO optimizer for all style dials (Full ACEO)
// Not a public header - used only within geos module
//
// ACEO: Adaptive Covariance Evolver Optimiser
// Based on CMA-ES with prior covariance from empirical dial correlations
//
// Key features:
//   - Uses prior correlation matrix from etc/aceo.json (or etc/aceo_full.json)
//   - Samples from multivariate normal using eigenspace projection
//   - Adapts step size (sigma) based on success rate
//   - Population-based (λ offspring per generation)
//   - Online covariance accumulator (Welford's algorithm)
//
// See doc/aceo.md for theory and empirical findings

#pragma once

#include <geos.hpp>
#include "diff.hpp"

namespace geos::internal
{
    // Full ACEO: All 45 style dials (excludes geometric - user composition)
    // Maps to indices in the full 45-dial theta vector
    constexpr int ACEO_DIAL_COUNT = 45;

    // Mapping from ACEO dial index to full theta index
    // All style dials included - geometric excluded (user composition)
    //
    // Index layout in theta:
    //   [0-2]   ColorCorrection: exposure, temperature, tint
    //   [3-9]   ToneMapping: contrast, highlights, shadows, toe, shoulder, black, white
    //   [10-12] GlobalColor: vibrance, saturation, density
    //   [13-16] SplitTone: shadow_temp, shadow_tint, highlight_temp, highlight_tint
    //   [17-40] SelectiveColor: 8 hues × (H/S/L)
    //   [41-44] Detail: sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma
    //
    constexpr std::array<int, ACEO_DIAL_COUNT> ACEO_DIAL_MAP = {
        // ColorCorrection (3)
        0,  // exposure
        1,  // temperature
        2,  // tint
        // ToneMapping (7)
        3,  // contrast
        4,  // highlights
        5,  // shadows
        6,  // toe_pivot
        7,  // shoulder_pivot
        8,  // black
        9,  // white
        // GlobalColor (3)
        10, // vibrance
        11, // saturation
        12, // density
        // SplitTone (4)
        13, // shadow_temp
        14, // shadow_tint
        15, // highlight_temp
        16, // highlight_tint
        // SelectiveColor (24)
        17, 18, 19,  // red H/S/L
        20, 21, 22,  // orange H/S/L
        23, 24, 25,  // yellow H/S/L
        26, 27, 28,  // green H/S/L
        29, 30, 31,  // cyan H/S/L
        32, 33, 34,  // blue H/S/L
        35, 36, 37,  // purple H/S/L
        38, 39, 40,  // magenta H/S/L
        // Detail (4)
        41, // sharpen_amount
        42, // sharpen_radius
        43, // denoise_luma
        44  // denoise_chroma
    };

    // CMA-ES hyperparameters
    struct AceoParams
    {
        int lambda;         // Population size (offspring per generation)
        int mu;             // Number of parents for recombination
        float sigma0;       // Initial step size
        float sigma_min;    // Minimum step size
        float sigma_max;    // Maximum step size
        float c_sigma;      // Step size adaptation rate
        float d_sigma;      // Step size damping
        float c_c;          // Cumulation for covariance
        float c_1;          // Rank-one update weight
        float c_mu;         // Rank-mu update weight
    };

    // Default parameters tuned for 45-dimensional dial optimization
    constexpr AceoParams ACEO_PARAMS_45D = {
        .lambda = 18,        // Population size (slightly larger for 45D)
        .mu = 9,             // Parents = lambda/2
        .sigma0 = 0.20f,     // Initial step size (20% of dial range)
        .sigma_min = 0.005f, // Minimum step size
        .sigma_max = 0.5f,   // Maximum step size
        .c_sigma = 0.15f,    // Step size adaptation (slower for higher D)
        .d_sigma = 1.5f,     // Damping (stable)
        .c_c = 0.08f,        // Cumulation rate (slower for 45D)
        .c_1 = 0.008f,       // Rank-one weight (smaller for 45D)
        .c_mu = 0.025f       // Rank-mu weight (smaller for stability)
    };

    // ACEO optimizer entry point
    // Returns number of generations performed
    int optimizeAceo(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress,
        bool lutEstimated = false,
        const TargetFeatures* targetFeatures = nullptr
    );

    // Load prior correlation matrix from JSON file
    // Returns true if successful, matrix is 45x45 row-major
    // Falls back to identity if file not found (for bootstrapping)
    bool loadPriorCovariance(const std::string& path, std::array<float, ACEO_DIAL_COUNT * ACEO_DIAL_COUNT>& matrix);

} // namespace geos::internal
