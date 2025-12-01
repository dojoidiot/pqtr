// global_color.cpp
// Global Color Module - Vibrance, Saturation, Color Density
// Part of Global Color module (3 dials)

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

namespace pipe
{
namespace mods
{
    // Convert linear RGB to Lab
    // Input: CV_32FC3 linear RGB [0,1]
    // Output: CV_32FC3 Lab (L: 0-100, a: -128 to 128, b: -128 to 128)
    static void rgb_to_lab(const cv::UMat& rgb, cv::UMat& lab)
    {
        // Apply sRGB gamma for cv::cvtColor (expects gamma-encoded input)
        cv::UMat gamma_rgb;
        cv::UMat clamped;
        cv::max(rgb, 0.0f, clamped);
        cv::min(clamped, 1.0f, clamped);
        cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

        // Convert to 8-bit for cvtColor
        cv::UMat rgb8;
        gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);

        // Convert BGR to Lab (OpenCV uses BGR order)
        cv::UMat bgr8;
        cv::cvtColor(rgb8, bgr8, cv::COLOR_RGB2BGR);
        cv::UMat lab8;
        cv::cvtColor(bgr8, lab8, cv::COLOR_BGR2Lab);

        // Convert back to float
        lab8.convertTo(lab, CV_32FC3);

        // Scale Lab to standard ranges
        // OpenCV Lab: L [0,255], a [0,255], b [0,255] (centered at 128)
        // Standard Lab: L [0,100], a [-128,128], b [-128,128]
        std::vector<cv::UMat> channels(3);
        cv::split(lab, channels);
        cv::multiply(channels[0], 100.0f/255.0f, channels[0]);  // L: 0-100
        cv::subtract(channels[1], 128.0f, channels[1]);          // a: -128 to 128
        cv::subtract(channels[2], 128.0f, channels[2]);          // b: -128 to 128
        cv::merge(channels, lab);
    }

    // Convert Lab to linear RGB
    // Input: CV_32FC3 Lab (L: 0-100, a: -128 to 128, b: -128 to 128)
    // Output: CV_32FC3 linear RGB [0,1]
    static void lab_to_rgb(const cv::UMat& lab, cv::UMat& rgb)
    {
        // Scale Lab back to OpenCV ranges
        std::vector<cv::UMat> channels(3);
        cv::split(lab, channels);
        cv::multiply(channels[0], 255.0f/100.0f, channels[0]);  // L: 0-255
        cv::add(channels[1], 128.0f, channels[1]);               // a: 0-255
        cv::add(channels[2], 128.0f, channels[2]);               // b: 0-255

        // Clamp to valid range
        for (auto& ch : channels) {
            cv::max(ch, 0.0f, ch);
            cv::min(ch, 255.0f, ch);
        }

        cv::UMat lab_scaled;
        cv::merge(channels, lab_scaled);

        // Convert to 8-bit for cvtColor
        cv::UMat lab8;
        lab_scaled.convertTo(lab8, CV_8UC3);

        // Convert Lab to BGR
        cv::UMat bgr8;
        cv::cvtColor(lab8, bgr8, cv::COLOR_Lab2BGR);
        cv::UMat rgb8;
        cv::cvtColor(bgr8, rgb8, cv::COLOR_BGR2RGB);

        // Convert to float and remove gamma
        cv::UMat gamma_rgb;
        rgb8.convertTo(gamma_rgb, CV_32FC3, 1.0/255.0);
        cv::pow(gamma_rgb, 2.2f, rgb);
    }

    // Apply global color adjustments
    // Input:  CV_32FC3 scene-linear sRGB
    // Output: CV_32FC3 adjusted linear RGB
    //
    // 3 Dials (all 0.0-1.0, default 0.5):
    //   vibrance:      Smart saturation with skin protection (0.5 = 0 neutral)
    //   saturation:    Global saturation multiplier (0.5 = 1.0 neutral)
    //   color_density: Color volume/intensity (0.5 = 1.0 neutral)
    //
    // Algorithm:
    //   1. Convert RGB to Lab
    //   2. Compute chroma C = sqrt(a² + b²) and hue h = atan2(b, a)
    //   3. Apply vibrance (saturation boost weighted by inverse chroma)
    //   4. Apply saturation (uniform chroma multiplier)
    //   5. Apply color density (chroma and L contrast boost)
    //   6. Convert Lab back to RGB
    bool global_color(
        const cv::UMat &input,
        cv::UMat &output,
        float vibrance_dial,
        float saturation_dial,
        float color_density_dial)
    {
        if (input.empty())
        {
            std::cerr << "[GlobalColor] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[GlobalColor] Error: Input must be CV_32FC3\n";
            return false;
        }

        // Clamp dials to valid range
        vibrance_dial = std::max(0.0f, std::min(1.0f, vibrance_dial));
        saturation_dial = std::max(0.0f, std::min(1.0f, saturation_dial));
        color_density_dial = std::max(0.0f, std::min(1.0f, color_density_dial));

        // Convert dials to working values
        // Vibrance: -1 to +1 (dial 0.5 = 0)
        float vibrance = (vibrance_dial - 0.5f) * 2.0f;

        // Saturation: 0 to 2 (dial 0.5 = 1.0)
        float saturation = saturation_dial * 2.0f;

        // Color density: 0.5 to 1.5 (dial 0.5 = 1.0)
        float color_density = 0.5f + color_density_dial;

        // NEUTRAL CHECK: If all dials are at 0.5, skip processing entirely
        // This avoids RGB→Lab→RGB quantization errors
        bool is_neutral = (std::abs(vibrance) <= 0.01f) &&
                          (std::abs(saturation - 1.0f) <= 0.01f) &&
                          (std::abs(color_density - 1.0f) <= 0.01f);
        if (is_neutral)
        {
            input.copyTo(output);
            return true;
        }

        try
        {
            // Convert to Lab color space
            cv::UMat lab;
            rgb_to_lab(input, lab);

            // Split into L, a, b channels
            std::vector<cv::UMat> channels(3);
            cv::split(lab, channels);
            cv::UMat& L = channels[0];
            cv::UMat& a = channels[1];
            cv::UMat& b = channels[2];

            // Compute chroma C = sqrt(a² + b²)
            cv::UMat a2, b2, C;
            cv::multiply(a, a, a2);
            cv::multiply(b, b, b2);
            cv::add(a2, b2, C);
            cv::sqrt(C, C);

            // Step 1: Apply vibrance (smart saturation)
            // Boost less saturated colors more, protect skin tones
            if (std::abs(vibrance) > 0.01f)
            {
                // Compute hue for skin tone detection
                cv::UMat hue;
                cv::phase(a, b, hue, true);  // hue in degrees

                // Skin tone mask: orange range 15°-75° with falloff
                cv::UMat skin_center, skin_mask;
                cv::subtract(hue, 45.0f, skin_center);  // center at 45°
                cv::multiply(skin_center, skin_center, skin_center);
                cv::divide(skin_center, -450.0f, skin_mask);  // Gaussian falloff
                cv::exp(skin_mask, skin_mask);

                // Vibrance weight: boost low-chroma pixels more
                // weight = 1 - (C / max_chroma), clamped
                cv::UMat vib_weight;
                cv::divide(C, 100.0f, vib_weight);  // normalize chroma
                cv::subtract(1.0f, vib_weight, vib_weight);
                cv::max(vib_weight, 0.0f, vib_weight);
                cv::min(vib_weight, 1.0f, vib_weight);

                // Reduce vibrance effect on skin tones
                if (vibrance > 0) {
                    cv::UMat skin_protection;
                    cv::subtract(1.0f, skin_mask, skin_protection);
                    cv::multiply(skin_protection, 0.7f, skin_protection);
                    cv::add(skin_protection, 0.3f, skin_protection);
                    cv::multiply(vib_weight, skin_protection, vib_weight);
                }

                // Apply vibrance: C_new = C * (1 + vibrance * weight)
                cv::UMat vib_boost;
                cv::multiply(vib_weight, vibrance, vib_boost);
                cv::add(vib_boost, 1.0f, vib_boost);

                // Scale a and b by the boost
                cv::multiply(a, vib_boost, a);
                cv::multiply(b, vib_boost, b);
            }

            // Step 2: Apply saturation (uniform multiplier)
            if (std::abs(saturation - 1.0f) > 0.01f)
            {
                cv::multiply(a, saturation, a);
                cv::multiply(b, saturation, b);
            }

            // Step 3: Apply color density (overall intensity)
            if (std::abs(color_density - 1.0f) > 0.01f)
            {
                // Boost chroma
                cv::multiply(a, color_density, a);
                cv::multiply(b, color_density, b);

                // Subtle L contrast boost around midpoint
                float l_contrast = 1.0f + (color_density - 1.0f) * 0.3f;
                cv::subtract(L, 50.0f, L);
                cv::multiply(L, l_contrast, L);
                cv::add(L, 50.0f, L);
            }

            // Clamp L to valid range
            cv::max(L, 0.0f, L);
            cv::min(L, 100.0f, L);

            // Merge channels back
            cv::merge(channels, lab);

            // Convert back to RGB
            lab_to_rgb(lab, output);

            // Clamp output
            cv::max(output, 0.0f, output);
            cv::min(output, 1.0f, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[GlobalColor] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
