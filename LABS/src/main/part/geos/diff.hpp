// diff.hpp
// Internal: Loss metric computation helpers
// Not a public header - used only within geos module

#pragma once

#include <opencv2/core.hpp>
#include <array>
#include <utility>
#include <algorithm>
#include <numeric>

namespace geos::internal
{
    // Constants
    constexpr int PROXY_SIZE = 512;
    constexpr float CHROMA_WEIGHT_K = 0.1f;
    constexpr int IDX_MU_L = 3;  // Brightness axis in style vector
    constexpr int IDX_MU_C = 4;  // Color axis in style vector

    // Regional analysis grid (4x4 = 16 cells)
    constexpr int GRID_SIZE = 4;
    constexpr int GRID_CELLS = GRID_SIZE * GRID_SIZE;

    // Sampled cells for MIDS phase (corners + center cross)
    // Indices: 0,3,12,15 (corners) + 5,6,9,10 (center quad)
    constexpr std::array<int, 8> SAMPLED_CELLS = {0, 3, 5, 6, 9, 10, 12, 15};

    // Style feature vector (10 dimensions)
    struct StyleFeatures
    {
        std::array<float, 10> v;   // Raw feature vector
        std::array<float, 10> psi; // Normalized (unit hypersphere)
    };

    // Pre-computed target features (global + regional)
    struct TargetFeatures
    {
        StyleFeatures global;
        std::array<StyleFeatures, GRID_CELLS> regions;
        cv::UMat lch;  // Cached LCH proxy for efficiency
    };

    // Loss evaluation mode (progressive strategy)
    enum class LossMode
    {
        GLOBAL_ONLY,    // HUGE phase: fast exploration
        SAMPLED,        // MIDS phase: corners + center (8 cells)
        FULL_REGIONAL   // TINY phase: all 16 cells
    };

    // Resize image to proxy size for efficient processing
    cv::UMat resizeProxy(const cv::UMat& image, int size = PROXY_SIZE);

    // Convert BGR 8-bit to LCH float with chroma-weighted hue
    cv::UMat convertToSafeLCH(const cv::UMat& bgr);

    // Extract style features from LCH image
    StyleFeatures extractStyle(const cv::UMat& lch);

    // Geodesic loss: 1 - |<a|b>|^2
    float geodesicLoss(const StyleFeatures& a, const StyleFeatures& b);

    // Compute dome compass coordinates (r, theta)
    std::pair<float, float> computeDome(const StyleFeatures& target, const StyleFeatures& candidate);

    // Laplacian variance for sharpness measurement
    float laplacianVariance(const cv::UMat& image);

    // ============================================================
    // Regional loss computation (progressive strategy)
    // ============================================================

    // Extract target features (global + all 16 regions) - call once at startup
    TargetFeatures extractTargetFeatures(const cv::UMat& target);

    // Extract features for a single cell (helper)
    StyleFeatures extractCellFeatures(const cv::UMat& lch, int cellIdx);

    // Compute loss with progressive strategy
    // - GLOBAL_ONLY: 1 SVD (fast exploration)
    // - SAMPLED: 1 + 8 SVDs (corners + center)
    // - FULL_REGIONAL: 1 + 16 SVDs (precise convergence)
    float computeProgressiveLoss(
        const cv::UMat& candidate,
        const TargetFeatures& target,
        LossMode mode,
        float globalWeight = 0.3f);

    // Diagnostic: compute full regional analysis for visualization
    struct RegionalAnalysis
    {
        float global;
        std::array<float, GRID_CELLS> local;

        float localMean() const
        {
            return std::accumulate(local.begin(), local.end(), 0.0f) / GRID_CELLS;
        }

        float localMax() const
        {
            return *std::max_element(local.begin(), local.end());
        }

        float localVariance() const
        {
            float mean = localMean();
            float sum = 0.0f;
            for (float v : local)
                sum += (v - mean) * (v - mean);
            return sum / GRID_CELLS;
        }

        float discrepancy() const { return global - localMean(); }

        std::pair<int, int> worstCell() const
        {
            auto it = std::max_element(local.begin(), local.end());
            int idx = std::distance(local.begin(), it);
            return {idx / GRID_SIZE, idx % GRID_SIZE};
        }
    };

    RegionalAnalysis computeRegionalAnalysis(
        const cv::UMat& candidate,
        const TargetFeatures& target);

} // namespace geos::internal
