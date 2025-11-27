// split_tone.cpp
// Split Toning Module - Shadow/Highlight Color Grading
// Applies different color casts to shadows vs highlights
//
// 4 dials: shadow_temp, shadow_tint, highlight_temp, highlight_tint
// Each pair shifts color temperature (warm/cool) and tint (green/magenta)
// in the respective luminance region

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

namespace pipe
{
namespace mods
{
    // Apply split toning (shadow/highlight color grading)
    // Input:  CV_32FC3 scene-linear RGB [0,1]
    // Output: CV_32FC3 adjusted linear RGB [0,1]
    //
    // 4 Dials (all 0.0-1.0, default 0.5):
    //   shadow_temp:    Shadow color temperature (0.5 = neutral, 0 = cool/blue, 1 = warm/yellow)
    //   shadow_tint:    Shadow tint (0.5 = neutral, 0 = green, 1 = magenta)
    //   highlight_temp: Highlight color temperature (same scale)
    //   highlight_tint: Highlight tint (same scale)
    //
    // Algorithm:
    //   1. Compute luminance from linear RGB
    //   2. Create soft shadow/highlight masks based on luminance
    //   3. Apply temperature (blue↔yellow) and tint (green↔magenta) shifts
    //   4. Blend using masks to affect only shadows/highlights
    //
    bool split_tone(
        const cv::UMat &input,
        cv::UMat &output,
        float shadow_temp,
        float shadow_tint,
        float highlight_temp,
        float highlight_tint)
    {
        if (input.empty())
        {
            std::cerr << "[SplitTone] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[SplitTone] Error: Input must be CV_32FC3\n";
            return false;
        }

        // Clamp dials to valid range
        shadow_temp = std::max(0.0f, std::min(1.0f, shadow_temp));
        shadow_tint = std::max(0.0f, std::min(1.0f, shadow_tint));
        highlight_temp = std::max(0.0f, std::min(1.0f, highlight_temp));
        highlight_tint = std::max(0.0f, std::min(1.0f, highlight_tint));

        // Check if all dials are neutral (0.5) - skip processing
        const float eps = 0.01f;
        bool all_neutral = (std::abs(shadow_temp - 0.5f) < eps) &&
                           (std::abs(shadow_tint - 0.5f) < eps) &&
                           (std::abs(highlight_temp - 0.5f) < eps) &&
                           (std::abs(highlight_tint - 0.5f) < eps);

        if (all_neutral)
        {
            input.copyTo(output);
            return true;
        }

        try
        {
            // Convert dials to shift amounts [-0.15, +0.15]
            // This range gives noticeable but not extreme color shifts
            const float max_shift = 0.15f;
            float shadow_temp_shift = (shadow_temp - 0.5f) * 2.0f * max_shift;      // + = warm (add R, sub B)
            float shadow_tint_shift = (shadow_tint - 0.5f) * 2.0f * max_shift;      // + = magenta (add R, sub G)
            float highlight_temp_shift = (highlight_temp - 0.5f) * 2.0f * max_shift;
            float highlight_tint_shift = (highlight_tint - 0.5f) * 2.0f * max_shift;

            // Split into RGB channels (BGR order in OpenCV)
            std::vector<cv::UMat> channels(3);
            cv::split(input, channels);
            cv::UMat& B = channels[0];
            cv::UMat& G = channels[1];
            cv::UMat& R = channels[2];

            // Compute luminance (Rec. 709 coefficients)
            cv::UMat lum;
            cv::UMat tmp1, tmp2;
            cv::multiply(R, 0.2126f, lum);
            cv::multiply(G, 0.7152f, tmp1);
            cv::add(lum, tmp1, lum);
            cv::multiply(B, 0.0722f, tmp2);
            cv::add(lum, tmp2, lum);

            // Create shadow mask: peaks at L=0.15, falls to 0 at L=0.5
            // shadow_mask = max(0, 1 - (L / 0.3))^2  for soft falloff
            cv::UMat shadow_mask;
            cv::divide(lum, 0.3f, shadow_mask);
            cv::subtract(1.0f, shadow_mask, shadow_mask);
            cv::max(shadow_mask, 0.0f, shadow_mask);
            cv::multiply(shadow_mask, shadow_mask, shadow_mask);  // Square for soft falloff

            // Create highlight mask: peaks at L=0.85, falls to 0 at L=0.5
            // highlight_mask = max(0, (L - 0.5) / 0.5)^2
            cv::UMat highlight_mask;
            cv::subtract(lum, 0.5f, highlight_mask);
            cv::divide(highlight_mask, 0.5f, highlight_mask);
            cv::max(highlight_mask, 0.0f, highlight_mask);
            cv::multiply(highlight_mask, highlight_mask, highlight_mask);  // Square for soft falloff

            // Apply shadow color shifts (masked)
            if (std::abs(shadow_temp_shift) > 0.001f || std::abs(shadow_tint_shift) > 0.001f)
            {
                cv::UMat shift_R, shift_G, shift_B;

                // Temperature: warm adds R, subtracts B
                // Tint: magenta adds R, subtracts G
                float r_shift = shadow_temp_shift * 0.5f + shadow_tint_shift * 0.5f;
                float g_shift = -shadow_tint_shift;
                float b_shift = -shadow_temp_shift;

                cv::UMat masked_r, masked_g, masked_b;
                cv::multiply(shadow_mask, r_shift, masked_r);
                cv::multiply(shadow_mask, g_shift, masked_g);
                cv::multiply(shadow_mask, b_shift, masked_b);

                cv::add(R, masked_r, R);
                cv::add(G, masked_g, G);
                cv::add(B, masked_b, B);
            }

            // Apply highlight color shifts (masked)
            if (std::abs(highlight_temp_shift) > 0.001f || std::abs(highlight_tint_shift) > 0.001f)
            {
                float r_shift = highlight_temp_shift * 0.5f + highlight_tint_shift * 0.5f;
                float g_shift = -highlight_tint_shift;
                float b_shift = -highlight_temp_shift;

                cv::UMat masked_r, masked_g, masked_b;
                cv::multiply(highlight_mask, r_shift, masked_r);
                cv::multiply(highlight_mask, g_shift, masked_g);
                cv::multiply(highlight_mask, b_shift, masked_b);

                cv::add(R, masked_r, R);
                cv::add(G, masked_g, G);
                cv::add(B, masked_b, B);
            }

            // Clamp channels
            cv::max(R, 0.0f, R);
            cv::min(R, 1.0f, R);
            cv::max(G, 0.0f, G);
            cv::min(G, 1.0f, G);
            cv::max(B, 0.0f, B);
            cv::min(B, 1.0f, B);

            // Merge back
            cv::merge(channels, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[SplitTone] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
