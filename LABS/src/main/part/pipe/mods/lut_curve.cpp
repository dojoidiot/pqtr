// lut_curve.cpp
// LUT-based Per-Channel Curve Module
// Applies 3 separate 1D lookup tables to R, G, B channels
//
// Unlike parametric tone_map, this directly maps input RGB -> output RGB
// using precomputed or estimated curves per channel.

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace pipe
{
namespace mods
{
    // Apply 3 separate 1D LUTs to R, G, B channels
    // Input:  CV_32FC3 linear RGB
    // Output: CV_32FC3 with each channel remapped
    // lut:    Array of 3*lut_size floats: [R0..R31, G0..G31, B0..B31]
    //
    // Values between LUT entries are linearly interpolated.
    bool lut_curve(
        const cv::UMat &input,
        cv::UMat &output,
        const float* lut,
        int lut_size)
    {
        if (input.empty() || lut == nullptr || lut_size < 2)
        {
            std::cerr << "[LutCurve] Error: Invalid input\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[LutCurve] Error: Input must be CV_32FC3\n";
            return false;
        }

        try
        {
            // Build full 256-entry LUTs for each channel by interpolating
            std::vector<float> full_lut_b(256), full_lut_g(256), full_lut_r(256);
            float step = 1.0f / (lut_size - 1);

            const float* lut_r = lut;                    // R: [0..lut_size)
            const float* lut_g = lut + lut_size;         // G: [lut_size..2*lut_size)
            const float* lut_b = lut + 2 * lut_size;     // B: [2*lut_size..3*lut_size)

            for (int i = 0; i < 256; i++)
            {
                float in_val = i / 255.0f;  // Normalized input 0-1
                float pos = in_val / step;  // Position in LUT
                int idx0 = static_cast<int>(pos);
                int idx1 = std::min(idx0 + 1, lut_size - 1);
                float frac = pos - idx0;
                idx0 = std::min(idx0, lut_size - 1);

                // Interpolate each channel's LUT
                full_lut_r[i] = lut_r[idx0] + frac * (lut_r[idx1] - lut_r[idx0]);
                full_lut_g[i] = lut_g[idx0] + frac * (lut_g[idx1] - lut_g[idx0]);
                full_lut_b[i] = lut_b[idx0] + frac * (lut_b[idx1] - lut_b[idx0]);
            }

            // Convert to 8-bit for processing (gamma encode first)
            cv::UMat clamped;
            cv::max(input, 0.0f, clamped);
            cv::min(clamped, 1.0f, clamped);
            cv::UMat gamma;
            cv::pow(clamped, 1.0f/2.2f, gamma);

            cv::UMat input_8u;
            gamma.convertTo(input_8u, CV_8UC3, 255.0);

            // Apply LUTs per channel
            cv::Mat img_cpu;
            input_8u.copyTo(img_cpu);

            for (int y = 0; y < img_cpu.rows; y++)
            {
                uchar* ptr = img_cpu.ptr<uchar>(y);
                for (int x = 0; x < img_cpu.cols; x++)
                {
                    int idx = x * 3;
                    // OpenCV BGR order
                    ptr[idx + 0] = static_cast<uchar>(full_lut_b[ptr[idx + 0]] * 255.0f + 0.5f);
                    ptr[idx + 1] = static_cast<uchar>(full_lut_g[ptr[idx + 1]] * 255.0f + 0.5f);
                    ptr[idx + 2] = static_cast<uchar>(full_lut_r[ptr[idx + 2]] * 255.0f + 0.5f);
                }
            }

            // Convert back to linear float
            cv::UMat result_8u;
            img_cpu.copyTo(result_8u);

            cv::UMat result_float;
            result_8u.convertTo(result_float, CV_32FC3, 1.0/255.0);
            cv::pow(result_float, 2.2f, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[LutCurve] Error: " << e.what() << "\n";
            return false;
        }
    }

    // Estimate per-channel transfer curves from base image to target image
    // Returns 3 LUTs (R, G, B) that map base channels to target channels
    //
    // base:   Our processed RAW
    // target: Camera JPEG (what we want to match)
    // lut:    Output array (must be preallocated with 3*lut_size elements)
    //         Layout: [R0..R31, G0..G31, B0..B31]
    bool estimate_lut(
        const cv::UMat& base,
        const cv::UMat& target,
        float* lut,
        int lut_size)
    {
        if (base.empty() || target.empty() || lut == nullptr || lut_size < 2)
        {
            std::cerr << "[EstimateLut] Error: Invalid input\n";
            return false;
        }

        try
        {
            // Resize target to match base if needed
            cv::UMat target_resized;
            if (base.size() != target.size())
            {
                cv::resize(target, target_resized, base.size());
            }
            else
            {
                target.copyTo(target_resized);
            }

            // Convert both to 8-bit BGR
            cv::UMat base_8u, target_8u;

            if (base.type() == CV_32FC3)
            {
                cv::UMat clamped;
                cv::max(base, 0.0f, clamped);
                cv::min(clamped, 1.0f, clamped);
                cv::UMat gamma;
                cv::pow(clamped, 1.0f/2.2f, gamma);
                gamma.convertTo(base_8u, CV_8UC3, 255.0);
            }
            else
            {
                base.convertTo(base_8u, CV_8UC3);
            }

            if (target_resized.type() == CV_32FC3)
            {
                cv::UMat clamped;
                cv::max(target_resized, 0.0f, clamped);
                cv::min(clamped, 1.0f, clamped);
                cv::UMat gamma;
                cv::pow(clamped, 1.0f/2.2f, gamma);
                gamma.convertTo(target_8u, CV_8UC3, 255.0);
            }
            else
            {
                target_resized.convertTo(target_8u, CV_8UC3);
            }

            cv::Mat base_cpu, target_cpu;
            base_8u.copyTo(base_cpu);
            target_8u.copyTo(target_cpu);

            // Compute average target value for each base value bin, per channel
            // Weight by saturation so colorful pixels have more influence
            float bin_size = 256.0f / lut_size;

            // R, G, B accumulators (weighted sums)
            std::vector<double> sum_r(lut_size, 0.0), sum_g(lut_size, 0.0), sum_b(lut_size, 0.0);
            std::vector<double> weight_r(lut_size, 0.0), weight_g(lut_size, 0.0), weight_b(lut_size, 0.0);

            for (int y = 0; y < base_cpu.rows; y++)
            {
                const uchar* b_ptr = base_cpu.ptr<uchar>(y);
                const uchar* t_ptr = target_cpu.ptr<uchar>(y);
                for (int x = 0; x < base_cpu.cols; x++)
                {
                    int idx = x * 3;
                    // BGR order
                    uchar b_b = b_ptr[idx + 0];
                    uchar b_g = b_ptr[idx + 1];
                    uchar b_r = b_ptr[idx + 2];

                    // Compute saturation weight for this pixel
                    // sat = (max - min) / max, scaled to give colorful pixels more weight
                    int maxc = std::max({b_r, b_g, b_b});
                    int minc = std::min({b_r, b_g, b_b});
                    float sat = (maxc > 10) ? static_cast<float>(maxc - minc) / maxc : 0.0f;

                    // Weight: 1.0 for neutrals, up to 3.0 for saturated colors
                    float w = 1.0f + 2.0f * sat;

                    int bin_b = std::min(lut_size - 1, static_cast<int>(b_b / bin_size));
                    int bin_g = std::min(lut_size - 1, static_cast<int>(b_g / bin_size));
                    int bin_r = std::min(lut_size - 1, static_cast<int>(b_r / bin_size));

                    sum_b[bin_b] += w * t_ptr[idx + 0];
                    sum_g[bin_g] += w * t_ptr[idx + 1];
                    sum_r[bin_r] += w * t_ptr[idx + 2];

                    weight_b[bin_b] += w;
                    weight_g[bin_g] += w;
                    weight_r[bin_r] += w;
                }
            }

            // Pointers into output array
            float* lut_r = lut;
            float* lut_g = lut + lut_size;
            float* lut_b = lut + 2 * lut_size;

            // Compute LUT values (normalized 0-1)
            for (int i = 0; i < lut_size; i++)
            {
                float default_val = (i + 0.5f) * bin_size / 255.0f;  // Identity default

                lut_r[i] = (weight_r[i] > 0.1) ? static_cast<float>(sum_r[i] / weight_r[i]) / 255.0f : default_val;
                lut_g[i] = (weight_g[i] > 0.1) ? static_cast<float>(sum_g[i] / weight_g[i]) / 255.0f : default_val;
                lut_b[i] = (weight_b[i] > 0.1) ? static_cast<float>(sum_b[i] / weight_b[i]) / 255.0f : default_val;
            }

            // Smooth each LUT to avoid discontinuities from sparse bins
            auto smooth_lut = [lut_size](float* lut_ch, const std::vector<double>& weights) {
                std::vector<float> smoothed(lut_size);
                smoothed[0] = lut_ch[0];
                smoothed[lut_size-1] = lut_ch[lut_size-1];
                for (int i = 1; i < lut_size - 1; i++)
                {
                    if (weights[i] < 100.0)
                    {
                        smoothed[i] = 0.5f * (lut_ch[i-1] + lut_ch[i+1]);
                    }
                    else
                    {
                        smoothed[i] = 0.25f * lut_ch[i-1] + 0.5f * lut_ch[i] + 0.25f * lut_ch[i+1];
                    }
                }
                for (int i = 0; i < lut_size; i++)
                {
                    lut_ch[i] = smoothed[i];
                }
            };

            smooth_lut(lut_r, weight_r);
            smooth_lut(lut_g, weight_g);
            smooth_lut(lut_b, weight_b);

            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[EstimateLut] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
