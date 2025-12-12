// tone_map.cpp
// Tone Mapping Module - Filmic HDR to SDR compression
// Part of Tone Mapping module (7 dials)
//
// LUMINANCE-PRESERVING MODE:
// All tone adjustments are applied to luminance only. RGB channels are
// scaled proportionally to preserve hue. This prevents color shifts that
// occur when per-channel curves have different gains.
//
// Algorithm:
//   1. Compute luminance: L = 0.2126*R + 0.7152*G + 0.0722*B
//   2. Apply tone curve to L → L_new
//   3. Compute scale = L_new / L
//   4. Apply: R_out = R * scale (preserves R:G:B ratios)

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

namespace pipe
{
namespace mods
{
    // Apply filmic tone mapping for HDR → SDR compression
    // Input:  CV_32FC3 scene-linear sRGB (BGR order in OpenCV)
    // Output: CV_32FC3 tone-mapped linear (before gamma)
    //
    // 7 Dials (all 0.0-1.0, ALL NEUTRAL AT 0.5):
    //   contrast:       0.5-2.0 global contrast (0.5 = 1.0, neutral)
    //   highlights:     -1.0 to +1.0 shoulder adjustment (0.5 = 0, neutral)
    //   shadows:        -1.0 to +1.0 toe adjustment (0.5 = 0, neutral)
    //   toe_pivot:      where shadow region ends (0.5 = 0.3 luminance)
    //   shoulder_pivot: where highlight region begins (0.5 = 0.7 luminance)
    //   white_point:    Reinhard compression (0.5 = bypass/neutral)
    //   black_point:    Shadow lift/crush (0.5 = 0/neutral)
    //
    // At dial 0.5 for all parameters, tone_map is a pass-through (no effect).
    //
    // LUMINANCE-PRESERVING APPROACH:
    //   1. Extract luminance from input RGB
    //   2. Apply all tone operations (black point, Reinhard, shadows, highlights, contrast) to L
    //   3. Compute scale = L_new / L
    //   4. Scale each RGB channel by the same factor → preserves R:G:B ratios (hue)
    bool tone_map(
        const cv::UMat &input,
        cv::UMat &output,
        float contrast_dial,
        float highlights_dial,
        float shadows_dial,
        float toe_pivot_dial,
        float shoulder_pivot_dial,
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
        toe_pivot_dial = std::max(0.0f, std::min(1.0f, toe_pivot_dial));
        shoulder_pivot_dial = std::max(0.0f, std::min(1.0f, shoulder_pivot_dial));
        white_point_dial = std::max(0.0f, std::min(1.0f, white_point_dial));
        black_point_dial = std::max(0.0f, std::min(1.0f, black_point_dial));

        // Convert dials to working values
        // All dials are neutral at 0.5 (no effect on image)

        // Contrast: exponential 0.5-3.0 (dial 0.5 = 1.0)
        // Expanded upper range for stronger contrast boost
        float contrast = 0.5f * std::exp(contrast_dial * 1.792f);

        // Highlights/Shadows: linear -1 to +1 (dial 0.5 = 0)
        float highlights = (highlights_dial - 0.5f) * 2.0f;
        float shadows = (shadows_dial - 0.5f) * 2.0f;

        // Pivot points: dial 0.5 = default positions (0.3 for toe, 0.7 for shoulder)
        // Range: toe 0.1-0.5, shoulder 0.5-0.9
        float toe_pivot = 0.1f + toe_pivot_dial * 0.4f;        // 0.1 to 0.5
        float shoulder_pivot = 0.5f + shoulder_pivot_dial * 0.4f;  // 0.5 to 0.9

        // Smooth mask steepness (how fast transition happens)
        const float mask_steepness = 12.0f;

        // White point: controls Reinhard compression strength
        // dial 0.0 → W=2.0 (aggressive compression)
        // dial 0.5 → bypass Reinhard (neutral, pass-through)
        // dial 1.0 → W=4.0 (mild compression for HDR recovery)
        bool bypass_reinhard = (white_point_dial > 0.45f && white_point_dial < 0.55f);
        float white_point = 2.0f + white_point_dial * 4.0f;  // 2.0 to 6.0

        // Black point: lifts/crushes shadows (dial 0.5 = 0, neutral)
        // dial 0.0 → -0.25 (crush blacks aggressively)
        // dial 0.5 → 0 (neutral)
        // dial 1.0 → +0.25 (lift blacks/fade)
        // Expanded range to ±0.25 for deeper black crush capability
        float black_point = (black_point_dial - 0.5f) * 0.5f;

        try
        {
            // ================================================================
            // LUMINANCE-PRESERVING TONE MAPPING
            // ================================================================
            //
            // Extract luminance, apply tone curve to L only, then scale RGB
            // proportionally. This preserves hue (R:G:B ratios).

            // Step 1: Extract luminance from BGR input
            // L = 0.0722*B + 0.7152*G + 0.2126*R (Rec. 709 coefficients, BGR order)
            std::vector<cv::UMat> channels(3);
            cv::split(input, channels);

            cv::UMat L;
            cv::addWeighted(channels[0], 0.0722f, channels[1], 0.7152f, 0.0f, L);  // B + G
            cv::addWeighted(L, 1.0f, channels[2], 0.2126f, 0.0f, L);                // + R

            // Store original luminance for later ratio computation
            cv::UMat L_orig;
            L.copyTo(L_orig);
            cv::max(L_orig, 0.0001f, L_orig);  // Avoid division by zero

            // Step 2: Apply black point adjustment to luminance
            if (std::abs(black_point) > 0.001f)
            {
                if (black_point > 0)
                {
                    // Lift: subtract black point, rescale to maintain white
                    cv::subtract(L, black_point, L);
                    cv::max(L, 0.0f, L);
                    float scale = 1.0f / (1.0f - black_point);
                    cv::multiply(L, scale, L);
                }
                else
                {
                    // Crush: add headroom at bottom, compress range
                    float abs_bp = std::abs(black_point);
                    float scale = 1.0f - abs_bp;
                    cv::multiply(L, scale, L);
                    cv::add(L, abs_bp, L);
                }
            }

            // Step 3: Extended Reinhard tone compression (bypass at neutral)
            // L_out = L * (1 + L/W²) / (1 + L)
            if (!bypass_reinhard)
            {
                float w2 = white_point * white_point;

                cv::UMat L_squared;
                cv::multiply(L, L, L_squared);

                cv::UMat term2;
                cv::divide(L_squared, w2, term2);

                cv::UMat numerator;
                cv::add(L, term2, numerator);

                cv::UMat denominator;
                cv::add(L, 1.0f, denominator);

                cv::divide(numerator, denominator, L);
            }

            // Step 4: Shadow curve adjustment (toe) with smooth mask
            if (std::abs(shadows) > 0.01f)
            {
                cv::UMat shifted;
                cv::subtract(L, toe_pivot, shifted);
                cv::multiply(shifted, mask_steepness, shifted);

                cv::UMat exp_val;
                cv::exp(shifted, exp_val);
                cv::UMat shadow_mask;
                cv::add(exp_val, 1.0f, shadow_mask);
                cv::divide(1.0f, shadow_mask, shadow_mask);

                float shadow_gamma = 1.0f - shadows * 0.5f;
                shadow_gamma = std::max(0.3f, std::min(2.0f, shadow_gamma));

                cv::UMat lifted;
                cv::add(L, 0.001f, lifted);
                cv::UMat shadow_adjusted;
                cv::pow(lifted, shadow_gamma, shadow_adjusted);

                cv::UMat inv_mask;
                cv::subtract(1.0f, shadow_mask, inv_mask);

                cv::UMat shadow_part, keep_part;
                cv::multiply(shadow_adjusted, shadow_mask, shadow_part);
                cv::multiply(L, inv_mask, keep_part);
                cv::add(shadow_part, keep_part, L);
            }

            // Step 5: Highlight curve adjustment (shoulder) with smooth mask
            if (std::abs(highlights) > 0.01f)
            {
                cv::UMat shifted;
                cv::subtract(L, shoulder_pivot, shifted);
                cv::multiply(shifted, -mask_steepness, shifted);

                cv::UMat exp_val;
                cv::exp(shifted, exp_val);
                cv::UMat highlight_mask;
                cv::add(exp_val, 1.0f, highlight_mask);
                cv::divide(1.0f, highlight_mask, highlight_mask);

                float highlight_gamma = 1.0f + highlights * 0.5f;
                highlight_gamma = std::max(0.3f, std::min(2.0f, highlight_gamma));

                cv::UMat inv_L;
                cv::subtract(1.0f, L, inv_L);
                cv::max(inv_L, 0.001f, inv_L);

                cv::UMat highlight_adjusted;
                cv::pow(inv_L, highlight_gamma, highlight_adjusted);
                cv::subtract(1.0f, highlight_adjusted, highlight_adjusted);

                cv::UMat inv_mask;
                cv::subtract(1.0f, highlight_mask, inv_mask);

                cv::UMat highlight_part, keep_part;
                cv::multiply(highlight_adjusted, highlight_mask, highlight_part);
                cv::multiply(L, inv_mask, keep_part);
                cv::add(highlight_part, keep_part, L);
            }

            // Step 6: Global contrast adjustment
            if (std::abs(contrast - 1.0f) > 0.01f)
            {
                cv::subtract(L, 0.5f, L);

                cv::UMat sign_mask;
                cv::compare(L, 0.0f, sign_mask, cv::CMP_GE);
                sign_mask.convertTo(sign_mask, CV_32FC1, 1.0/255.0);

                cv::UMat abs_L;
                cv::absdiff(L, 0.0f, abs_L);
                cv::multiply(abs_L, 2.0f, abs_L);

                cv::UMat powered;
                cv::add(abs_L, 0.001f, abs_L);
                cv::pow(abs_L, contrast, powered);
                cv::multiply(powered, 0.5f, powered);

                cv::UMat pos_part, neg_part;
                cv::multiply(powered, sign_mask, pos_part);

                cv::UMat inv_sign;
                cv::subtract(1.0f, sign_mask, inv_sign);
                cv::multiply(powered, inv_sign, neg_part);
                cv::multiply(neg_part, -1.0f, neg_part);

                cv::add(pos_part, neg_part, L);
                cv::add(L, 0.5f, L);
            }

            // Clamp L to [0, 1]
            cv::max(L, 0.0f, L);
            cv::min(L, 1.0f, L);

            // ================================================================
            // Step 7: Compute scale factor and apply to RGB channels
            // ================================================================
            //
            // scale = L_new / L_orig
            // This preserves R:G:B ratios, keeping hue constant

            cv::UMat scale;
            cv::divide(L, L_orig, scale);

            // Apply scale to each channel (preserves hue)
            cv::multiply(channels[0], scale, channels[0]);  // B
            cv::multiply(channels[1], scale, channels[1]);  // G
            cv::multiply(channels[2], scale, channels[2]);  // R

            // Merge back and clamp
            cv::UMat result;
            cv::merge(channels, result);

            cv::max(result, 0.0f, result);
            cv::min(result, 1.0f, output);

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
