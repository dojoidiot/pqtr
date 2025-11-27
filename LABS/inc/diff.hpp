// diff.hpp
// Public API for computing perceptual difference between images
//
// Two metrics for tune optimization:
// - Spectral loss: Color/tone similarity (geodesic distance on hypersphere)
// - Frequency loss: Sharpness similarity (Laplacian variance ratio)
//
// PIMPL design enables caching of base image features for efficient
// repeated comparisons during tune optimization.
//
// User apps include only this header and link against labs.a.
// For serialization, see data.hpp.

#pragma once

#include <hold.hpp>
#include <opencv2/core.hpp>

namespace diff
{

    // GPU-accelerated image matrix (BGR 8-bit).
    // Reference-counted internally by OpenCV - shallow copy shares data.
    // Returned Views are read-only references; do not modify.
    using View = cv::UMat;

    // Combined metrics result (value type)
    struct Data
    {
        float spectral = 0.0f;  // [0, 1] geodesic distance (0 = identical color/tone)
        float frequency = 0.0f; // [0, ∞) relative variance difference (0 = identical sharpness)
    };

    // Task holds cached base image features for efficient repeated comparisons.
    // Ownership: Hold<Task> owns the task; returned Views share underlying data.
    // Created via diff::make(base), destroyed via RAII.
    class Task
    {
    public:
        virtual ~Task() = default;

        // Access cached base image (read-only reference)
        virtual View base() = 0;

        // Compute metrics between test and cached base
        virtual Data diff(View test) = 0;

        // Compute visual diff image (amplified pixel difference)
        virtual View view(View test, float scale = 5.0f) = 0;
    };

    // Factory: create Task with base image (features cached for reuse)
    pqtr::Hold<Task> make(View base);

} // namespace diff
