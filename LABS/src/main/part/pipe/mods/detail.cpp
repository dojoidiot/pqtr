// detail.cpp
// Detail + Output Module - Sharpen, Denoise
// Part of Detail module (4 dials)

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

namespace pipe
{
namespace mods
{
    // Apply unsharp mask sharpening (luminance-only)
    // Input:  CV_32FC3 linear RGB
    // Output: CV_32FC3 sharpened linear RGB (colors preserved)
    //
    // Sharpens only the luminance channel in Lab space to avoid
    // color shifts and chromatic aberration amplification.
    static void apply_sharpen(cv::UMat& img, float amount, float radius)
    {
        if (amount < 0.01f) return;

        // Convert to gamma-encoded RGB for Lab conversion
        cv::UMat clamped;
        cv::max(img, 0.0f, clamped);
        cv::min(clamped, 1.0f, clamped);

        cv::UMat gamma_rgb;
        cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

        // Convert to 8-bit BGR for Lab conversion
        cv::UMat rgb8;
        gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);

        cv::UMat bgr8;
        cv::cvtColor(rgb8, bgr8, cv::COLOR_RGB2BGR);

        // Convert to Lab
        cv::UMat lab;
        cv::cvtColor(bgr8, lab, cv::COLOR_BGR2Lab);

        // Split into L, a, b channels
        std::vector<cv::UMat> channels(3);
        cv::split(lab, channels);

        // Apply unsharp mask to L channel only
        cv::UMat L_float;
        channels[0].convertTo(L_float, CV_32F);

        cv::UMat L_blurred;
        int kernel_size = static_cast<int>(radius * 2) * 2 + 1;  // Ensure odd
        kernel_size = std::max(3, std::min(31, kernel_size));
        cv::GaussianBlur(L_float, L_blurred, cv::Size(kernel_size, kernel_size), radius);

        // Unsharp mask: L_sharp = L + amount * (L - L_blurred)
        cv::UMat L_diff;
        cv::subtract(L_float, L_blurred, L_diff);
        cv::scaleAdd(L_diff, amount, L_float, L_float);

        // Clamp L to valid range [0, 255]
        cv::max(L_float, 0.0f, L_float);
        cv::min(L_float, 255.0f, L_float);

        // Convert back to 8-bit
        L_float.convertTo(channels[0], CV_8U);

        // Merge channels (a and b unchanged)
        cv::UMat lab_sharp;
        cv::merge(channels, lab_sharp);

        // Convert back to BGR
        cv::UMat bgr_sharp;
        cv::cvtColor(lab_sharp, bgr_sharp, cv::COLOR_Lab2BGR);

        // Convert to RGB
        cv::UMat rgb8_sharp;
        cv::cvtColor(bgr_sharp, rgb8_sharp, cv::COLOR_BGR2RGB);

        // Convert to float [0,1]
        cv::UMat gamma_out;
        rgb8_sharp.convertTo(gamma_out, CV_32FC3, 1.0/255.0);

        // Remove gamma to get linear RGB
        cv::pow(gamma_out, 2.2f, img);
    }

    // Apply bilateral filter denoise (approximation for speed)
    // Input:  CV_32FC3 linear RGB
    // Output: CV_32FC3 denoised linear RGB
    static void apply_denoise_luminance(cv::UMat& img, float strength)
    {
        if (strength < 1.0f) return;

        // Convert to Lab for luminance-only denoise
        cv::UMat gamma_rgb, lab;

        // Apply gamma for perceptual processing
        cv::UMat clamped;
        cv::max(img, 0.0f, clamped);
        cv::min(clamped, 1.0f, clamped);
        cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

        // Convert to 8-bit for processing
        cv::UMat rgb8;
        gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);

        cv::UMat bgr8;
        cv::cvtColor(rgb8, bgr8, cv::COLOR_RGB2BGR);

        // Use bilateral filter for edge-preserving smoothing
        cv::Mat cpu_bgr;
        bgr8.copyTo(cpu_bgr);

        cv::Mat denoised;
        // Parameters: d=diameter, sigmaColor, sigmaSpace
        int d = static_cast<int>(strength / 10.0f) + 3;
        d = std::min(9, d);  // Limit for performance
        float sigma = strength * 0.5f;
        cv::bilateralFilter(cpu_bgr, denoised, d, sigma, sigma);

        // Convert back
        cv::UMat denoised_umat;
        denoised.copyTo(denoised_umat);

        cv::UMat rgb8_out;
        cv::cvtColor(denoised_umat, rgb8_out, cv::COLOR_BGR2RGB);

        cv::UMat gamma_out;
        rgb8_out.convertTo(gamma_out, CV_32FC3, 1.0/255.0);

        // Remove gamma
        cv::pow(gamma_out, 2.2f, img);
    }

    // Apply chroma denoise
    static void apply_denoise_chroma(cv::UMat& img, float strength)
    {
        if (strength < 1.0f) return;

        // Convert to Lab
        cv::UMat gamma_rgb;
        cv::UMat clamped;
        cv::max(img, 0.0f, clamped);
        cv::min(clamped, 1.0f, clamped);
        cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

        cv::UMat rgb8;
        gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);

        cv::UMat bgr8;
        cv::cvtColor(rgb8, bgr8, cv::COLOR_RGB2BGR);

        cv::UMat lab;
        cv::cvtColor(bgr8, lab, cv::COLOR_BGR2Lab);

        // Split channels
        std::vector<cv::UMat> channels(3);
        cv::split(lab, channels);

        // Blur only a and b channels (chroma)
        int kernel_size = static_cast<int>(strength / 10.0f) * 2 + 3;
        kernel_size = std::min(15, kernel_size);
        if (kernel_size % 2 == 0) kernel_size++;

        cv::GaussianBlur(channels[1], channels[1], cv::Size(kernel_size, kernel_size), 0);
        cv::GaussianBlur(channels[2], channels[2], cv::Size(kernel_size, kernel_size), 0);

        // Merge and convert back
        cv::UMat lab_denoised;
        cv::merge(channels, lab_denoised);

        cv::UMat bgr_out;
        cv::cvtColor(lab_denoised, bgr_out, cv::COLOR_Lab2BGR);

        cv::UMat rgb8_out;
        cv::cvtColor(bgr_out, rgb8_out, cv::COLOR_BGR2RGB);

        cv::UMat gamma_out;
        rgb8_out.convertTo(gamma_out, CV_32FC3, 1.0/255.0);

        cv::pow(gamma_out, 2.2f, img);
    }

    // Apply detail adjustments (sharpen + denoise)
    // Input:  CV_32FC3 scene-linear sRGB
    // Output: CV_32FC3 adjusted linear RGB
    //
    // 4 Dials (all 0.0-1.0):
    //   sharpen_amount: Sharpening strength (0.0 = none, 0.6 default, 1.0 = 2.0x)
    //   sharpen_radius: Sharpening radius (0.0 = 0.5px, 0.4 default = 1.5px, 1.0 = 3px)
    //   denoise_luma:   Luminance noise reduction (0.0 = none, 0.3 default, 1.0 = 100)
    //   denoise_chroma: Chroma noise reduction (0.0 = none, 0.5 default, 1.0 = 100)
    //
    // Processing order: Sharpen → Denoise (luminance) → Denoise (chroma)
    bool detail(
        const cv::UMat &input,
        cv::UMat &output,
        float sharpen_amount_dial,
        float sharpen_radius_dial,
        float denoise_luma_dial,
        float denoise_chroma_dial)
    {
        if (input.empty())
        {
            std::cerr << "[Detail] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[Detail] Error: Input must be CV_32FC3\n";
            return false;
        }

        // Clamp dials to valid range
        sharpen_amount_dial = std::max(0.0f, std::min(1.0f, sharpen_amount_dial));
        sharpen_radius_dial = std::max(0.0f, std::min(1.0f, sharpen_radius_dial));
        denoise_luma_dial = std::max(0.0f, std::min(1.0f, denoise_luma_dial));
        denoise_chroma_dial = std::max(0.0f, std::min(1.0f, denoise_chroma_dial));

        // Convert dials to working values
        // Sharpen amount: 0.0-1.0 → 0.0 to 2.0
        float sharpen_amount = sharpen_amount_dial * 2.0f;

        // Sharpen radius: 0.0-1.0 → 0.5 to 3.0 pixels
        float sharpen_radius = 0.5f + sharpen_radius_dial * 2.5f;

        // Denoise: 0.0-1.0 → 0 to 100 (exponential for natural response)
        float denoise_luma = 100.0f * denoise_luma_dial * denoise_luma_dial;
        float denoise_chroma = 100.0f * denoise_chroma_dial * denoise_chroma_dial;

        // Check if any processing is needed
        bool needs_sharpen = (sharpen_amount > 0.01f);
        bool needs_denoise_luma = (denoise_luma > 1.0f);
        bool needs_denoise_chroma = (denoise_chroma > 1.0f);

        if (!needs_sharpen && !needs_denoise_luma && !needs_denoise_chroma)
        {
            input.copyTo(output);
            return true;
        }

        try
        {
            input.copyTo(output);

            // Step 1: Apply sharpening
            if (needs_sharpen)
            {
                apply_sharpen(output, sharpen_amount, sharpen_radius);
            }

            // Step 2: Apply luminance denoise
            if (needs_denoise_luma)
            {
                apply_denoise_luminance(output, denoise_luma);
            }

            // Step 3: Apply chroma denoise
            if (needs_denoise_chroma)
            {
                apply_denoise_chroma(output, denoise_chroma);
            }

            // Final clamp
            cv::max(output, 0.0f, output);
            cv::min(output, 1.0f, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Detail] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
