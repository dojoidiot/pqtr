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
    constexpr int STYLE_DIM = 23;  // Feature vector dimension (19 + 4 luminance-dependent color)
    constexpr int IDX_MU_L = 3;    // Brightness axis in style vector
    constexpr int IDX_MU_C = 4;    // Color axis in style vector
    constexpr int IDX_STD_L = 5;   // Contrast (L standard deviation)
    constexpr int IDX_STD_C = 6;   // Chroma spread
    constexpr int IDX_MU_A = 10;   // Lab a* axis (green-magenta)
    constexpr int IDX_MU_B = 11;   // Lab b* axis (blue-yellow)
    // Histogram percentiles for tone curve shape
    constexpr int IDX_L_P10 = 12;  // Black point (crushed shadows)
    constexpr int IDX_L_P25 = 13;  // Shadow region
    constexpr int IDX_L_P75 = 14;  // Highlight region
    constexpr int IDX_L_P90 = 15;  // White point (stretched highlights)
    constexpr int IDX_C_P50 = 16;  // Chroma median (saturation level)
    constexpr int IDX_C_P90 = 17;  // Chroma peak (max saturation)
    constexpr int IDX_C_SHADOW = 18;  // Shadow chroma (mean C where L < L_p25)
    // Luminance-dependent color (for split tone optimization)
    constexpr int IDX_A_SHADOW = 19;    // Mean a* where L < L_p25 (shadow green-magenta)
    constexpr int IDX_B_SHADOW = 20;    // Mean b* where L < L_p25 (shadow blue-yellow)
    constexpr int IDX_A_HIGHLIGHT = 21; // Mean a* where L > L_p75 (highlight green-magenta)
    constexpr int IDX_B_HIGHLIGHT = 22; // Mean b* where L > L_p75 (highlight blue-yellow)

    // Regional analysis grid (4x4 = 16 cells)
    constexpr int GRID_SIZE = 4;
    constexpr int GRID_CELLS = GRID_SIZE * GRID_SIZE;

    // Sampled cells for MIDS phase (corners + center cross)
    // Indices: 0,3,12,15 (corners) + 5,6,9,10 (center quad)
    constexpr std::array<int, 8> SAMPLED_CELLS = {0, 3, 5, 6, 9, 10, 12, 15};

    // Feature weights for weighted L2 loss (TRAINED on 11 images)
    // Higher weight = more important for matching
    constexpr std::array<float, STYLE_DIM> FEATURE_WEIGHTS = {
        0.8f, 0.8f, 0.7f,   // [0-2]  sigma1, sigma2, sigma3 (shape)
        1.6f,               // [3]    mu_L (brightness)
        2.7f,               // [4]    mu_C (saturation)
        5.0f,               // [5]    std_L (contrast - critical!)
        3.7f,               // [6]    std_C (chroma spread)
        5.0f,               // [7]    skew_L (tone curve asymmetry - critical!)
        0.5f, 0.5f,         // [8-9]  cov_LC, cov_HC (correlations)
        4.8f, 5.0f,         // [10-11] mu_a, mu_b (color cast)
        5.0f,               // [12]   L_p10 (black point - critical!)
        5.0f,               // [13]   L_p25 (shadow region - critical!)
        5.0f,               // [14]   L_p75 (highlight region - critical!)
        5.0f,               // [15]   L_p90 (white point - critical!)
        5.0f,               // [16]   C_p50 (chroma median - critical!)
        3.1f,               // [17]   C_p90 (chroma peak)
        5.0f,               // [18]   C_shadow (shadow chroma - critical!)
        3.0f, 3.0f,         // [19-20] a_shadow, b_shadow (shadow color)
        3.0f, 3.0f          // [21-22] a_highlight, b_highlight (highlight color)
    };

    // Style feature vector (23 dimensions)
    // [0-2]   SVD singular values (sigma1, sigma2, sigma3)
    // [3-4]   LCH means (mu_L, mu_C)
    // [5-6]   LCH stds (std_L, std_C)
    // [7]     Luminance skewness (skew_L)
    // [8-9]   Covariances (cov_LC, cov_HC)
    // [10-11] Lab a/b means (mu_a, mu_b) - global color cast
    // [12-15] Luminance percentiles (L_p10, L_p25, L_p75, L_p90) - tone curve
    // [16-17] Chroma percentiles (C_p50, C_p90) - saturation level
    // [18]    Shadow chroma (mean C where L < L_p25) - preserve color in shadows
    // [19-20] Shadow color (a_shadow, b_shadow) - for split tone optimization
    // [21-22] Highlight color (a_highlight, b_highlight) - for split tone optimization
    struct StyleFeatures
    {
        std::array<float, STYLE_DIM> v;   // Raw feature vector
        std::array<float, STYLE_DIM> psi; // Normalized (for backward compat, but not used in loss)
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

    // Extract style features from LCH image (legacy - doesn't capture color cast)
    StyleFeatures extractStyle(const cv::UMat& lch);

    // Extract style features directly from BGR image (preferred - captures Lab a/b for color cast)
    StyleFeatures extractStyleFromBGR(const cv::UMat& bgr);

    // Geodesic loss: 1 - |<a|b>|^2
    float geodesicLoss(const StyleFeatures& a, const StyleFeatures& b);

    // Diagnostic: per-feature error analysis
    extern const char* FEATURE_NAMES[STYLE_DIM];
    std::array<float, STYLE_DIM> perFeatureError(const StyleFeatures& target, const StyleFeatures& candidate);
    void printFeatureAnalysis(const StyleFeatures& target, const StyleFeatures& candidate);

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

    // ============================================================
    // Axis Contrast Preservation (Hypothesis 3)
    // ============================================================

    // Opponent axis contrast: measures how much both poles of each axis are present
    // High contrast = both poles saturated (R and C both present)
    // Low contrast = one-sided or desaturated
    struct AxisContrast
    {
        float r_c;  // Red vs Cyan axis
        float g_m;  // Green vs Magenta axis
        float b_y;  // Blue vs Yellow axis

        float total() const { return r_c + g_m + b_y; }
    };

    // Measure axis contrast in a BGR image
    AxisContrast measureAxisContrast(const cv::UMat& bgr);

    // Axis contrast loss: penalize collapsing axes that exist in target
    // Returns 0 if no axes to preserve, positive if contrast is lost
    float axisContrastLoss(const AxisContrast& target, const AxisContrast& candidate);

} // namespace geos::internal
