// diff.hpp
// Internal: Loss metric computation helpers
// Not a public header - used only within tune module

#pragma once

#include <opencv2/core.hpp>
#include <array>
#include <utility>

namespace tune::internal
{
    // Constants
    constexpr int PROXY_SIZE = 512;
    constexpr float CHROMA_WEIGHT_K = 0.1f;
    constexpr int IDX_MU_L = 3;  // Brightness axis in style vector
    constexpr int IDX_MU_C = 4;  // Color axis in style vector

    // Style feature vector (10 dimensions)
    struct StyleFeatures
    {
        std::array<float, 10> v;   // Raw feature vector
        std::array<float, 10> psi; // Normalized (unit hypersphere)
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

} // namespace tune::internal
