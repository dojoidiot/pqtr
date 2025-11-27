// tone_map.cpp
// Tone Mapping Module - Filmic HDR to SDR compression
// Part of Tone Mapping module (5 dials)

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

namespace pipe
{
namespace mods
{
    // Apply filmic tone mapping for HDR → SDR compression
    // Input:  CV_32FC3 scene-linear sRGB
    // Output: CV_32FC3 tone-mapped linear (before gamma)
    //
    // 5 Dials (all 0.0-1.0, ALL NEUTRAL AT 0.5):
    //   contrast:    0.5-2.0 global contrast (0.5 = 1.0, neutral)
    //   highlights:  -1.0 to +1.0 shoulder adjustment (0.5 = 0, neutral)
    //   shadows:     -1.0 to +1.0 toe adjustment (0.5 = 0, neutral)
    //   white_point: Reinhard compression (0.5 = bypass/neutral, <0.5 = aggressive, >0.5 = mild)
    //   black_point: Shadow lift/crush (0.5 = 0/neutral, <0.5 = crush, >0.5 = lift)
    //
    // At dial 0.5 for all parameters, tone_map is a pass-through (no effect).
    //
    // Algorithm:
    //   1. Apply black point adjustment (lift or crush)
    //   2. Apply extended Reinhard with white point (bypassed at 0.5)
    //   3. Apply shadows curve (toe)
    //   4. Apply highlights curve (shoulder)
    //   5. Apply contrast
    bool tone_map(
        const cv::UMat &input,
        cv::UMat &output,
        float contrast_dial,
        float highlights_dial,
        float shadows_dial,
        float white_point_dial,
        float black_point_dial)
    {
        if (input.empty())
        {
            std::cerr << "[ToneMap] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[ToneMap] Error: Input must be CV_32FC3\n";
            return false;
        }

        // Clamp dials to valid range
        contrast_dial = std::max(0.0f, std::min(1.0f, contrast_dial));
        highlights_dial = std::max(0.0f, std::min(1.0f, highlights_dial));
        shadows_dial = std::max(0.0f, std::min(1.0f, shadows_dial));
        white_point_dial = std::max(0.0f, std::min(1.0f, white_point_dial));
        black_point_dial = std::max(0.0f, std::min(1.0f, black_point_dial));

        // Convert dials to working values
        // All dials are neutral at 0.5 (no effect on image)

        // Contrast: exponential 0.5-2.0 (dial 0.5 = 1.0)
        float contrast = 0.5f * std::exp(contrast_dial * 1.386f);

        // Highlights/Shadows: linear -1 to +1 (dial 0.5 = 0)
        float highlights = (highlights_dial - 0.5f) * 2.0f;
        float shadows = (shadows_dial - 0.5f) * 2.0f;

        // White point: controls Reinhard compression strength
        // dial 0.0 → W=2.0 (aggressive compression)
        // dial 0.5 → bypass Reinhard (neutral, pass-through)
        // dial 1.0 → W=4.0 (mild compression for HDR recovery)
        bool bypass_reinhard = (white_point_dial > 0.45f && white_point_dial < 0.55f);
        float white_point = 2.0f + white_point_dial * 4.0f;  // 2.0 to 6.0

        // Black point: lifts shadows (dial 0.5 = 0, neutral)
        // dial 0.0 → -0.05 (crush blacks)
        // dial 0.5 → 0 (neutral)
        // dial 1.0 → +0.05 (lift blacks)
        float black_point = (black_point_dial - 0.5f) * 0.1f;

        try
        {
            cv::UMat working;

            // Step 1: Black point adjustment (lift or crush shadows)
            // Positive: lift blacks, Negative: crush blacks, Zero: neutral
            if (std::abs(black_point) > 0.001f)
            {
                if (black_point > 0)
                {
                    // Lift: subtract black point, rescale to maintain white
                    cv::subtract(input, black_point, working);
                    cv::max(working, 0.0f, working);
                    float scale = 1.0f / (1.0f - black_point);
                    cv::multiply(working, scale, working);
                }
                else
                {
                    // Crush: add headroom at bottom, compress range
                    float abs_bp = std::abs(black_point);
                    float scale = 1.0f - abs_bp;
                    cv::multiply(input, scale, working);
                    cv::add(working, abs_bp, working);
                }
            }
            else
            {
                input.copyTo(working);
            }

            // Step 2: Extended Reinhard tone compression (bypass at neutral)
            // L_out = L * (1 + L/W²) / (1 + L)
            if (!bypass_reinhard)
            {
                float w2 = white_point * white_point;

                cv::UMat L_squared;
                cv::multiply(working, working, L_squared);

                cv::UMat term2;
                cv::divide(L_squared, w2, term2);

                cv::UMat numerator;
                cv::add(working, term2, numerator);

                cv::UMat denominator;
                cv::add(working, 1.0f, denominator);

                cv::divide(numerator, denominator, working);
            }
            // else: bypass Reinhard, keep working as-is

            // Step 3: Shadow curve adjustment (toe)
            // Lift dark values when shadows > 0, crush when < 0
            if (std::abs(shadows) > 0.01f)
            {
                // Soft toe curve: blend linear with power curve
                cv::UMat shadow_mask;
                cv::threshold(working, shadow_mask, 0.5f, 1.0f, cv::THRESH_BINARY_INV);
                shadow_mask.convertTo(shadow_mask, CV_32FC3);

                // Power adjustment for shadows region
                float shadow_gamma = 1.0f - shadows * 0.5f;  // 0.5-1.5 range
                shadow_gamma = std::max(0.3f, std::min(2.0f, shadow_gamma));

                cv::UMat shadow_adjusted;
                cv::UMat lifted;
                cv::add(working, 0.001f, lifted);
                cv::pow(lifted, shadow_gamma, shadow_adjusted);

                // Blend based on shadow mask
                cv::UMat inv_mask;
                cv::subtract(1.0f, shadow_mask, inv_mask);

                cv::UMat shadow_part, keep_part;
                cv::multiply(shadow_adjusted, shadow_mask, shadow_part);
                cv::multiply(working, inv_mask, keep_part);
                cv::add(shadow_part, keep_part, working);
            }

            // Step 4: Highlight curve adjustment (shoulder)
            // Compress highlights when < 0, expand when > 0
            if (std::abs(highlights) > 0.01f)
            {
                // Soft shoulder curve
                cv::UMat highlight_mask;
                cv::threshold(working, highlight_mask, 0.5f, 1.0f, cv::THRESH_BINARY);
                highlight_mask.convertTo(highlight_mask, CV_32FC3);

                // Inverse power for highlights (expand/compress upper values)
                float highlight_gamma = 1.0f + highlights * 0.5f;  // 0.5-1.5 range
                highlight_gamma = std::max(0.3f, std::min(2.0f, highlight_gamma));

                cv::UMat inv_working;
                cv::subtract(1.0f, working, inv_working);
                cv::max(inv_working, 0.001f, inv_working);

                cv::UMat highlight_adjusted;
                cv::pow(inv_working, highlight_gamma, highlight_adjusted);
                cv::subtract(1.0f, highlight_adjusted, highlight_adjusted);

                // Blend based on highlight mask
                cv::UMat inv_mask;
                cv::subtract(1.0f, highlight_mask, inv_mask);

                cv::UMat highlight_part, keep_part;
                cv::multiply(highlight_adjusted, highlight_mask, highlight_part);
                cv::multiply(working, inv_mask, keep_part);
                cv::add(highlight_part, keep_part, working);
            }

            // Step 5: Global contrast adjustment
            if (std::abs(contrast - 1.0f) > 0.01f)
            {
                // S-curve contrast around midpoint (0.5)
                // Shift to center, apply power, shift back
                cv::subtract(working, 0.5f, working);

                cv::UMat sign_mask;
                cv::compare(working, 0.0f, sign_mask, cv::CMP_GE);
                sign_mask.convertTo(sign_mask, CV_32FC3, 1.0/255.0);

                cv::UMat abs_working;
                cv::absdiff(working, 0.0f, abs_working);
                cv::multiply(abs_working, 2.0f, abs_working);  // Scale to 0-1

                cv::UMat powered;
                cv::add(abs_working, 0.001f, abs_working);
                cv::pow(abs_working, contrast, powered);
                cv::multiply(powered, 0.5f, powered);  // Scale back

                // Restore sign
                cv::UMat pos_part, neg_part;
                cv::multiply(powered, sign_mask, pos_part);

                cv::UMat inv_sign;
                cv::subtract(1.0f, sign_mask, inv_sign);
                cv::multiply(powered, inv_sign, neg_part);
                cv::multiply(neg_part, -1.0f, neg_part);

                cv::add(pos_part, neg_part, working);
                cv::add(working, 0.5f, working);
            }

            // Clamp final output to [0, 1]
            cv::max(working, 0.0f, working);
            cv::min(working, 1.0f, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[ToneMap] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
