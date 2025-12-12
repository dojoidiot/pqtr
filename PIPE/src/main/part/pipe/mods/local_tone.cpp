// local_tone.cpp
// Local Tone Mapping Module (Iridix-style)
//
// Implements a simplified version of Apical's Iridix algorithm based on
// patent US7302110B2 (Chesnokov, "Image enhancement methods").
//
// The algorithm applies different tone curves to different parts of the image
// based on local luminance context, mimicking human retinal adaptation.
//
// Key features:
// - Multi-scale Gaussian pyramid decomposition
// - Per-pixel adaptive strength based on local luminance
// - Asymmetric processing (stronger in shadows, weaker in highlights)
// - Preserves local contrast while compressing global dynamic range

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace pipe
{
namespace mods
{
    // Compute local mean using Gaussian pyramid (efficient multi-scale)
    static void compute_local_mean(
        const cv::Mat& lum,
        cv::Mat& local_mean,
        int window_size)
    {
        // Use Gaussian blur as approximation to local mean
        // Window size determines spatial locality
        int ksize = window_size | 1;  // Ensure odd
        cv::GaussianBlur(lum, local_mean, cv::Size(ksize, ksize), 0);
    }

    // Asymmetric weighting function from Iridix patent
    // F(I) = [log(I+Δ) - log(Δ)] / [log(1+Δ) - log(Δ)]
    // This compresses shadows more than highlights
    static float asymmetric_weight(float intensity, float delta)
    {
        float num = std::log(intensity + delta) - std::log(delta);
        float den = std::log(1.0f + delta) - std::log(delta);
        return num / den;
    }

    // Transform strength function - stronger for shadows
    // α(I) = 0.5 - 0.5·tanh(4·F(I) - 2)
    static float transform_strength(float intensity, float delta)
    {
        float f = asymmetric_weight(intensity, delta);
        return 0.5f - 0.5f * std::tanh(4.0f * f - 2.0f);
    }

    // Local tone mapping using simplified Iridix algorithm
    // Input:  CV_32FC3 (gamma-encoded, 0-1 range)
    // Output: CV_32FC3 (locally tone-mapped, 0-1 range)
    //
    // Parameters:
    //   strength: Overall effect strength (0-1, default 0.5)
    //   delta: Asymmetry parameter (0.001-0.1, default 0.02)
    //   window_scale: Window size as fraction of image (0.01-0.5, default 0.1)
    bool local_tone(
        const cv::UMat& input,
        cv::UMat& output,
        float strength,
        float delta,
        float window_scale)
    {
        if (input.empty())
        {
            std::cerr << "[LocalTone] Error: Invalid input\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[LocalTone] Error: Input must be CV_32FC3\n";
            return false;
        }

        try
        {
            cv::Mat input_cpu;
            input.copyTo(input_cpu);

            // Extract luminance (simple average for speed)
            cv::Mat lum(input_cpu.size(), CV_32FC1);
            for (int y = 0; y < input_cpu.rows; y++)
            {
                const float* src = input_cpu.ptr<float>(y);
                float* dst = lum.ptr<float>(y);
                for (int x = 0; x < input_cpu.cols; x++)
                {
                    // BGR order
                    float b = src[x * 3 + 0];
                    float g = src[x * 3 + 1];
                    float r = src[x * 3 + 2];
                    // Rec.709 luminance weights
                    dst[x] = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                }
            }

            // Compute local mean at multiple scales
            int base_window = static_cast<int>(std::min(input_cpu.rows, input_cpu.cols) * window_scale);
            base_window = std::max(5, base_window);

            cv::Mat local_mean;
            compute_local_mean(lum, local_mean, base_window);

            // Apply local tone mapping
            cv::Mat output_cpu(input_cpu.size(), CV_32FC3);

            for (int y = 0; y < input_cpu.rows; y++)
            {
                const float* src = input_cpu.ptr<float>(y);
                const float* lm_ptr = local_mean.ptr<float>(y);
                const float* lum_ptr = lum.ptr<float>(y);
                float* dst = output_cpu.ptr<float>(y);

                for (int x = 0; x < input_cpu.cols; x++)
                {
                    float local_lum = std::max(0.001f, lm_ptr[x]);
                    float pixel_lum = std::max(0.001f, lum_ptr[x]);

                    // Compute adaptation level based on local context
                    // Dark areas get lifted, bright areas slightly compressed
                    float alpha = transform_strength(local_lum, delta);

                    // Target luminance: blend between original and lifted
                    // In shadows: target approaches 0.5 (mid-gray)
                    // In highlights: target stays close to original
                    float lift_target = 0.18f + (1.0f - 0.18f) * asymmetric_weight(local_lum, delta);
                    float target_lum = local_lum + strength * alpha * (lift_target - local_lum);

                    // Scale factor to apply to RGB
                    float scale = target_lum / local_lum;

                    // Reduce scale in highlights to prevent blowout
                    if (pixel_lum > 0.7f)
                    {
                        float highlight_suppress = 1.0f - (pixel_lum - 0.7f) / 0.3f;
                        highlight_suppress = std::max(0.0f, highlight_suppress);
                        scale = 1.0f + (scale - 1.0f) * highlight_suppress;
                    }

                    // Apply scale to RGB, maintaining color ratios
                    float b = src[x * 3 + 0];
                    float g = src[x * 3 + 1];
                    float r = src[x * 3 + 2];

                    // Soft clamp to prevent excessive boost
                    scale = std::min(scale, 2.0f);

                    dst[x * 3 + 0] = std::max(0.0f, std::min(1.0f, b * scale));
                    dst[x * 3 + 1] = std::max(0.0f, std::min(1.0f, g * scale));
                    dst[x * 3 + 2] = std::max(0.0f, std::min(1.0f, r * scale));
                }
            }

            output_cpu.copyTo(output);
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[LocalTone] Error: " << e.what() << "\n";
            return false;
        }
    }

    // Estimate local tone mapping parameters from base→target pair
    // Uses histogram comparison to find optimal strength/delta
    bool estimate_local_tone(
        const cv::UMat& base,
        const cv::UMat& target,
        float& strength,
        float& delta,
        float& window_scale)
    {
        if (base.empty() || target.empty())
        {
            std::cerr << "[EstimateLocalTone] Error: Invalid input\n";
            return false;
        }

        try
        {
            cv::Mat base_cpu, target_cpu;
            base.copyTo(base_cpu);
            target.copyTo(target_cpu);

            // Resize if needed
            if (base_cpu.size() != target_cpu.size())
            {
                cv::resize(base_cpu, base_cpu, target_cpu.size(), 0, 0, cv::INTER_AREA);
            }

            // Convert target to float if needed
            cv::Mat target_f;
            if (target_cpu.type() == CV_8UC3)
            {
                target_cpu.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);
            }
            else
            {
                target_f = target_cpu;
            }

            // Apply gamma to base if it's linear
            cv::Mat base_gamma;
            cv::max(base_cpu, 0.0f, base_gamma);
            cv::min(base_gamma, 1.0f, base_gamma);
            cv::pow(base_gamma, 1.0f / 2.2f, base_gamma);

            // Extract luminance from both
            auto extract_lum = [](const cv::Mat& img) -> cv::Mat {
                cv::Mat lum(img.size(), CV_32FC1);
                for (int y = 0; y < img.rows; y++)
                {
                    const float* src = img.ptr<float>(y);
                    float* dst = lum.ptr<float>(y);
                    for (int x = 0; x < img.cols; x++)
                    {
                        dst[x] = 0.2126f * src[x*3+2] + 0.7152f * src[x*3+1] + 0.0722f * src[x*3+0];
                    }
                }
                return lum;
            };

            cv::Mat base_lum = extract_lum(base_gamma);
            cv::Mat target_lum = extract_lum(target_f);

            // Analyze shadow regions (where DRO has most effect)
            float base_shadow_mean = 0, target_shadow_mean = 0;
            int shadow_count = 0;

            for (int y = 0; y < base_lum.rows; y++)
            {
                const float* bl = base_lum.ptr<float>(y);
                const float* tl = target_lum.ptr<float>(y);
                for (int x = 0; x < base_lum.cols; x++)
                {
                    if (bl[x] < 0.3f)  // Shadow region
                    {
                        base_shadow_mean += bl[x];
                        target_shadow_mean += tl[x];
                        shadow_count++;
                    }
                }
            }

            if (shadow_count > 0)
            {
                base_shadow_mean /= shadow_count;
                target_shadow_mean /= shadow_count;
            }

            // Estimate strength based on shadow lift
            float shadow_lift_ratio = (shadow_count > 0 && base_shadow_mean > 0.01f) ?
                (target_shadow_mean / base_shadow_mean) : 1.0f;

            // Map lift ratio to strength (1.0 = no lift, 2.0 = heavy lift)
            strength = std::min(1.0f, std::max(0.0f, (shadow_lift_ratio - 1.0f)));

            // Default delta and window_scale (could be tuned per-scene)
            delta = 0.02f;
            window_scale = 0.1f;

            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[EstimateLocalTone] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
