// base_curve.cpp
// Base Curve Module - Applies per-channel tone curves derived by RAWS
// Bridges the gap between flat RAW decode and camera JPEG appearance

#include <opencv2/core.hpp>
#include <iostream>
#include <cmath>

namespace pipe
{
namespace mods
{
    // Apply per-channel base curves (RGB LUTs)
    //
    // Input:  CV_32FC3 scene-linear BGR [0-1]
    // Output: CV_32FC3 with tone curves applied [0-1]
    // curve:  768 floats [B0..B255, G0..G255, R0..R255] in gamma space
    //
    // The curves were estimated in gamma space (sRGB), so we:
    // 1. Convert linear → gamma per channel
    // 2. Apply per-channel curve
    // 3. Convert gamma → linear
    bool base_curve(
        const cv::UMat &input,
        cv::UMat &output,
        const float* curve)
    {
        if (input.empty() || curve == nullptr)
        {
            std::cerr << "[BaseCurve] Error: Invalid input\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[BaseCurve] Error: Input must be CV_32FC3\n";
            return false;
        }

        try
        {
            cv::Mat cpu_in;
            input.copyTo(cpu_in);

            cv::Mat cpu_out = cpu_in.clone();

            for (int y = 0; y < cpu_in.rows; y++)
            {
                const float* in_ptr = cpu_in.ptr<float>(y);
                float* out_ptr = cpu_out.ptr<float>(y);

                for (int x = 0; x < cpu_in.cols; x++)
                {
                    for (int c = 0; c < 3; c++)  // B=0, G=1, R=2
                    {
                        float v = in_ptr[x * 3 + c];

                        // Clamp to [0, 1]
                        v = std::max(0.0f, std::min(1.0f, v));

                        // Linear → gamma (curve was estimated in gamma space)
                        float gamma_v = std::pow(v, 1.0f / 2.2f);

                        // Map to per-channel LUT with interpolation
                        float pos = gamma_v * 255.0f;
                        int idx0 = static_cast<int>(pos);
                        int idx1 = std::min(idx0 + 1, 255);
                        float frac = pos - idx0;

                        // Per-channel curve: curve[c*256 + idx]
                        int base = c * 256;
                        float out_gamma = curve[base + idx0] + frac * (curve[base + idx1] - curve[base + idx0]);

                        // Gamma → linear
                        out_ptr[x * 3 + c] = std::pow(out_gamma, 2.2f);
                    }
                }
            }

            cpu_out.copyTo(output);
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[BaseCurve] Error: " << e.what() << "\n";
            return false;
        }
    }

    // Generate identity curve for all 3 channels (768 values)
    void base_curve_identity(float* curve)
    {
        for (int c = 0; c < 3; c++)
            for (int i = 0; i < 256; i++)
                curve[c * 256 + i] = i / 255.0f;
    }

} // namespace mods
} // namespace pipe
