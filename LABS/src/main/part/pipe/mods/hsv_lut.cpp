// hsv_lut.cpp
// HSV-space LUT for per-hue/saturation color corrections
// Matches Adobe DCP HueSatDelta table approach
//
// Input:  (H, S) → lookup grid
// Output: (ΔH, ΔS, ΔV) applied to each pixel
//
// Grid size follows DCP standard:
//   H: 90 bins (4° per bin) or 36 bins (10° per bin, compact)
//   S: 25 bins (DCP standard) or 12 bins (compact)

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

namespace pipe
{
namespace mods
{

// HSV LUT grid dimensions (compact version for fast iteration)
static constexpr int H_BINS = 36;   // 10° per bin
static constexpr int S_BINS = 12;   // 12 saturation levels
static constexpr int HSV_LUT_SIZE = H_BINS * S_BINS * 3;  // 1296 floats

// Apply HSV LUT to image
// Input:  CV_32FC3 linear RGB (0-1)
// Output: CV_32FC3 with HSV deltas applied
// lut:    H_BINS × S_BINS × 3 array of (ΔH, ΔS, ΔV)
//         Layout: [h0s0_dh, h0s0_ds, h0s0_dv, h0s1_dh, ...]
bool hsv_lut_apply(
    const cv::UMat& input,
    cv::UMat& output,
    const float* lut)
{
    if (input.empty() || lut == nullptr)
    {
        std::cerr << "[HsvLut] Error: Invalid input\n";
        return false;
    }

    if (input.type() != CV_32FC3)
    {
        std::cerr << "[HsvLut] Error: Input must be CV_32FC3\n";
        return false;
    }

    try
    {
        // Work on CPU for per-pixel HSV manipulation
        cv::Mat img_cpu;
        input.copyTo(img_cpu);

        // Convert linear RGB to gamma for HSV conversion (HSV expects gamma-encoded)
        cv::Mat gamma_rgb;
        cv::pow(img_cpu, 1.0f / 2.2f, gamma_rgb);
        cv::max(gamma_rgb, 0.0f, gamma_rgb);
        cv::min(gamma_rgb, 1.0f, gamma_rgb);

        // Convert to 8-bit for HSV conversion (OpenCV cvtColor with 32F gives different ranges)
        cv::Mat gamma_8u;
        gamma_rgb.convertTo(gamma_8u, CV_8UC3, 255.0);

        // Convert to HSV (8-bit: H=0-180, S=0-255, V=0-255)
        cv::Mat hsv_8u;
        cv::cvtColor(gamma_8u, hsv_8u, cv::COLOR_BGR2HSV);

        // Convert back to float for manipulation
        cv::Mat hsv;
        hsv_8u.convertTo(hsv, CV_32FC3);

        // Apply LUT with bilinear interpolation
        for (int y = 0; y < hsv.rows; y++)
        {
            float* hsv_ptr = hsv.ptr<float>(y);
            for (int x = 0; x < hsv.cols; x++)
            {
                int idx = x * 3;
                float h = hsv_ptr[idx + 0];  // 0-180 in OpenCV
                float s = hsv_ptr[idx + 1] / 255.0f;  // Convert to 0-1
                float v = hsv_ptr[idx + 2] / 255.0f;  // Convert to 0-1

                // Normalize H to 0-360 then to bin position
                float h_normalized = (h / 180.0f) * 360.0f;  // 0-360
                float h_pos = (h_normalized / 360.0f) * H_BINS;
                float s_pos = s * (S_BINS - 1);  // S is already 0-1 for float

                // Bilinear interpolation indices
                int h0 = static_cast<int>(h_pos) % H_BINS;
                int h1 = (h0 + 1) % H_BINS;  // Wrap around for hue
                int s0 = std::clamp(static_cast<int>(s_pos), 0, S_BINS - 1);
                int s1 = std::min(s0 + 1, S_BINS - 1);

                float h_frac = h_pos - static_cast<int>(h_pos);
                float s_frac = s_pos - s0;

                // LUT lookup - each cell has 3 values (ΔH, ΔS, ΔV)
                auto lookup = [lut](int hi, int si) -> const float* {
                    return &lut[(hi * S_BINS + si) * 3];
                };

                // Get 4 corners
                const float* c00 = lookup(h0, s0);
                const float* c01 = lookup(h0, s1);
                const float* c10 = lookup(h1, s0);
                const float* c11 = lookup(h1, s1);

                // Bilinear interpolation for each delta
                float dh = (1 - h_frac) * (1 - s_frac) * c00[0] +
                           (1 - h_frac) * s_frac * c01[0] +
                           h_frac * (1 - s_frac) * c10[0] +
                           h_frac * s_frac * c11[0];

                float ds = (1 - h_frac) * (1 - s_frac) * c00[1] +
                           (1 - h_frac) * s_frac * c01[1] +
                           h_frac * (1 - s_frac) * c10[1] +
                           h_frac * s_frac * c11[1];

                float dv = (1 - h_frac) * (1 - s_frac) * c00[2] +
                           (1 - h_frac) * s_frac * c01[2] +
                           h_frac * (1 - s_frac) * c10[2] +
                           h_frac * s_frac * c11[2];

                // Apply deltas
                // ΔH is in degrees (-30 to +30), scale for OpenCV's 0-180 range
                float new_h = h + dh * 0.5f;  // dh is degrees, scale to 0-180
                if (new_h < 0) new_h += 180.0f;
                if (new_h >= 180.0f) new_h -= 180.0f;

                float new_s = std::clamp(s + ds, 0.0f, 1.0f);
                float new_v = std::clamp(v + dv, 0.0f, 1.0f);

                // Write back (H stays in 0-180, S/V need to be in 0-255 for conversion)
                hsv_ptr[idx + 0] = new_h;
                hsv_ptr[idx + 1] = new_s * 255.0f;
                hsv_ptr[idx + 2] = new_v * 255.0f;
            }
        }

        // Convert back to 8-bit HSV for cvtColor
        cv::Mat hsv_out_8u;
        hsv.convertTo(hsv_out_8u, CV_8UC3);

        // Convert back to BGR
        cv::Mat result_gamma_8u;
        cv::cvtColor(hsv_out_8u, result_gamma_8u, cv::COLOR_HSV2BGR);

        // Convert to float and then to linear
        cv::Mat result_gamma;
        result_gamma_8u.convertTo(result_gamma, CV_32FC3, 1.0 / 255.0);

        // Convert gamma back to linear
        cv::Mat result_linear;
        cv::pow(result_gamma, 2.2f, result_linear);

        result_linear.copyTo(output);
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HsvLut] Error: " << e.what() << "\n";
        return false;
    }
}

// Estimate HSV LUT from base image to target image
// Samples pixels, bins by (H, S) of base, accumulates delta from target
//
// base:   Our processed RAW (CV_32FC3 or CV_8UC3)
// target: Camera JPEG (CV_32FC3 or CV_8UC3)
// lut:    Output array (must be preallocated with H_BINS * S_BINS * 3 floats)
bool hsv_lut_estimate(
    const cv::UMat& base,
    const cv::UMat& target,
    float* lut)
{
    if (base.empty() || target.empty() || lut == nullptr)
    {
        std::cerr << "[HsvLut] Error: Invalid input for estimation\n";
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

        // Convert both to 8-bit BGR for HSV conversion
        cv::Mat base_cpu, target_cpu;

        if (base.type() == CV_32FC3)
        {
            cv::UMat clamped;
            cv::max(base, 0.0f, clamped);
            cv::min(clamped, 1.0f, clamped);
            cv::UMat gamma;
            cv::pow(clamped, 1.0f / 2.2f, gamma);
            cv::Mat temp;
            gamma.copyTo(temp);
            temp.convertTo(base_cpu, CV_8UC3, 255.0);
        }
        else
        {
            base.copyTo(base_cpu);
        }

        if (target_resized.type() == CV_32FC3)
        {
            cv::UMat clamped;
            cv::max(target_resized, 0.0f, clamped);
            cv::min(clamped, 1.0f, clamped);
            cv::UMat gamma;
            cv::pow(clamped, 1.0f / 2.2f, gamma);
            cv::Mat temp;
            gamma.copyTo(temp);
            temp.convertTo(target_cpu, CV_8UC3, 255.0);
        }
        else
        {
            target_resized.copyTo(target_cpu);
        }

        // Convert to HSV
        cv::Mat base_hsv, target_hsv;
        cv::cvtColor(base_cpu, base_hsv, cv::COLOR_BGR2HSV);
        cv::cvtColor(target_cpu, target_hsv, cv::COLOR_BGR2HSV);

        // Accumulators for each bin: sum of deltas and count
        std::vector<double> sum_dh(H_BINS * S_BINS, 0.0);
        std::vector<double> sum_ds(H_BINS * S_BINS, 0.0);
        std::vector<double> sum_dv(H_BINS * S_BINS, 0.0);
        std::vector<double> counts(H_BINS * S_BINS, 0.0);

        // Sample all pixels and bin them
        for (int y = 0; y < base_hsv.rows; y++)
        {
            const uchar* base_ptr = base_hsv.ptr<uchar>(y);
            const uchar* tgt_ptr = target_hsv.ptr<uchar>(y);

            for (int x = 0; x < base_hsv.cols; x++)
            {
                int idx = x * 3;

                // Base HSV (OpenCV: H=0-179, S=0-255, V=0-255)
                float b_h = base_ptr[idx + 0];
                float b_s = base_ptr[idx + 1] / 255.0f;
                float b_v = base_ptr[idx + 2] / 255.0f;

                // Target HSV
                float t_h = tgt_ptr[idx + 0];
                float t_s = tgt_ptr[idx + 1] / 255.0f;
                float t_v = tgt_ptr[idx + 2] / 255.0f;

                // Skip very dark or very desaturated pixels (noisy data)
                if (b_v < 0.05f || b_s < 0.05f) continue;

                // Compute bin indices from base HSV
                int h_bin = static_cast<int>((b_h / 180.0f) * H_BINS) % H_BINS;
                int s_bin = std::clamp(static_cast<int>(b_s * S_BINS), 0, S_BINS - 1);
                int bin_idx = h_bin * S_BINS + s_bin;

                // Compute deltas (target - base)
                // Hue delta needs wrapping (-90 to +90 in OpenCV's 0-180 scale)
                float dh = t_h - b_h;
                if (dh > 90.0f) dh -= 180.0f;
                if (dh < -90.0f) dh += 180.0f;
                dh *= 2.0f;  // Scale back to full 0-360 degrees

                float ds = t_s - b_s;
                float dv = t_v - b_v;

                // Weight by saturation (colorful pixels are more reliable)
                float weight = b_s;

                sum_dh[bin_idx] += weight * dh;
                sum_ds[bin_idx] += weight * ds;
                sum_dv[bin_idx] += weight * dv;
                counts[bin_idx] += weight;
            }
        }

        // Compute averages and fill LUT
        for (int i = 0; i < H_BINS * S_BINS; i++)
        {
            if (counts[i] > 1.0)
            {
                lut[i * 3 + 0] = static_cast<float>(sum_dh[i] / counts[i]);
                lut[i * 3 + 1] = static_cast<float>(sum_ds[i] / counts[i]);
                lut[i * 3 + 2] = static_cast<float>(sum_dv[i] / counts[i]);
            }
            else
            {
                // No data for this bin - use identity (no change)
                lut[i * 3 + 0] = 0.0f;
                lut[i * 3 + 1] = 0.0f;
                lut[i * 3 + 2] = 0.0f;
            }
        }

        // Smooth the LUT to handle sparse bins
        // Simple averaging with neighbors (respecting hue wrap-around)
        std::vector<float> smoothed(H_BINS * S_BINS * 3);
        for (int h = 0; h < H_BINS; h++)
        {
            for (int s = 0; s < S_BINS; s++)
            {
                int center = h * S_BINS + s;

                // Neighbor indices (with hue wrap-around)
                int h_prev = ((h - 1 + H_BINS) % H_BINS) * S_BINS + s;
                int h_next = ((h + 1) % H_BINS) * S_BINS + s;
                int s_prev = (s > 0) ? h * S_BINS + (s - 1) : center;
                int s_next = (s < S_BINS - 1) ? h * S_BINS + (s + 1) : center;

                for (int c = 0; c < 3; c++)
                {
                    float avg = 0.4f * lut[center * 3 + c] +
                                0.15f * lut[h_prev * 3 + c] +
                                0.15f * lut[h_next * 3 + c] +
                                0.15f * lut[s_prev * 3 + c] +
                                0.15f * lut[s_next * 3 + c];
                    smoothed[center * 3 + c] = avg;
                }
            }
        }

        // Copy smoothed back to output
        for (int i = 0; i < H_BINS * S_BINS * 3; i++)
        {
            lut[i] = smoothed[i];
        }

        // Log stats
        int filled = 0;
        float max_dh = 0, max_ds = 0, max_dv = 0;
        for (int i = 0; i < H_BINS * S_BINS; i++)
        {
            if (std::abs(lut[i * 3 + 0]) > 0.01f ||
                std::abs(lut[i * 3 + 1]) > 0.01f ||
                std::abs(lut[i * 3 + 2]) > 0.01f)
            {
                filled++;
            }
            max_dh = std::max(max_dh, std::abs(lut[i * 3 + 0]));
            max_ds = std::max(max_ds, std::abs(lut[i * 3 + 1]));
            max_dv = std::max(max_dv, std::abs(lut[i * 3 + 2]));
        }

        std::cerr << "[HsvLut] Estimated " << H_BINS << "x" << S_BINS
                  << " LUT, " << filled << "/" << (H_BINS * S_BINS) << " bins filled\n";
        std::cerr << "[HsvLut] Max deltas: ΔH=" << max_dh << "° ΔS=" << max_ds
                  << " ΔV=" << max_dv << "\n";

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HsvLut] Estimation error: " << e.what() << "\n";
        return false;
    }
}

// Initialize identity LUT (no change)
void hsv_lut_identity(float* lut)
{
    for (int i = 0; i < H_BINS * S_BINS * 3; i++)
    {
        lut[i] = 0.0f;
    }
}

// Get LUT dimensions
int hsv_lut_h_bins() { return H_BINS; }
int hsv_lut_s_bins() { return S_BINS; }
int hsv_lut_size() { return HSV_LUT_SIZE; }

} // namespace mods
} // namespace pipe
