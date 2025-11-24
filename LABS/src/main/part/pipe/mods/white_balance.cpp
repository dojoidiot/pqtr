// white_balance.cpp
// White Balance Module - Adjusts color temperature and tint
// Part of Color Correction module (2 of 3 dials)

#include <opencv2/core.hpp>
#include <iostream>
#include <cmath>

namespace pipe
{
namespace mods
{
    // Convert color temperature (Kelvin) to RGB multipliers
    // Based on Tanner Helland's algorithm (approximation of Planckian locus)
    static void kelvin_to_rgb(float kelvin, float &r, float &g, float &b)
    {
        float temp = kelvin / 100.0f;

        // Red
        if (temp <= 66.0f)
        {
            r = 1.0f;
        }
        else
        {
            r = temp - 60.0f;
            r = 329.698727446f * std::pow(r, -0.1332047592f);
            r = std::max(0.0f, std::min(1.0f, r / 255.0f));
        }

        // Green
        if (temp <= 66.0f)
        {
            g = temp;
            g = 99.4708025861f * std::log(g) - 161.1195681661f;
            g = std::max(0.0f, std::min(1.0f, g / 255.0f));
        }
        else
        {
            g = temp - 60.0f;
            g = 288.1221695283f * std::pow(g, -0.0755148492f);
            g = std::max(0.0f, std::min(1.0f, g / 255.0f));
        }

        // Blue
        if (temp >= 66.0f)
        {
            b = 1.0f;
        }
        else if (temp <= 19.0f)
        {
            b = 0.0f;
        }
        else
        {
            b = temp - 10.0f;
            b = 138.5177312231f * std::log(b) - 305.0447927307f;
            b = std::max(0.0f, std::min(1.0f, b / 255.0f));
        }
    }

    // Apply white balance adjustment
    // Input:  CV_32FC3 scene-linear sRGB
    // Output: CV_32FC3 white-balanced linear sRGB
    //
    // Dial: temperature (0.0 - 1.0)
    //   0.0 = 2000K (very warm/orange)
    //   0.5 = 6500K (daylight neutral)
    //   1.0 = 12000K (very cool/blue)
    //
    // Dial: tint (0.0 - 1.0)
    //   0.0 = -100 (green shift)
    //   0.5 = 0 (neutral)
    //   1.0 = +100 (magenta shift)
    //
    // The algorithm:
    // 1. Convert dial to target color temperature
    // 2. Calculate RGB multipliers for that temperature
    // 3. Normalize so middle channel stays at 1.0
    // 4. Apply tint as green/magenta shift
    // 5. Multiply image by inverse of target color
    bool white_balance(
        const cv::UMat &input,
        cv::UMat &output,
        float temperature,
        float tint)
    {
        if (input.empty())
        {
            std::cerr << "[WhiteBalance] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[WhiteBalance] Error: Input must be CV_32FC3\n";
            return false;
        }

        // Clamp dials to valid range
        temperature = std::max(0.0f, std::min(1.0f, temperature));
        tint = std::max(0.0f, std::min(1.0f, tint));

        // Convert temperature dial to Kelvin
        // 0.0 → 2000K, 0.5 → 6500K, 1.0 → 12000K
        // Use exponential mapping for perceptual linearity
        float kelvin = 2000.0f * std::pow(6.0f, temperature);

        // Get RGB values for this color temperature
        float tr, tg, tb;
        kelvin_to_rgb(kelvin, tr, tg, tb);

        // Normalize so we preserve overall brightness
        // Use green as reference (middle of spectrum)
        float norm = 1.0f / std::max(0.001f, tg);
        tr *= norm;
        tg = 1.0f;
        tb *= norm;

        // Apply tint (green/magenta shift)
        // tint < 0.5 = more green, tint > 0.5 = more magenta
        float tint_shift = (tint - 0.5f) * 0.4f; // ±0.2 max shift
        float green_mult = 1.0f - tint_shift;
        float rb_mult = 1.0f + tint_shift * 0.5f; // Split between R and B

        // Final multipliers (inverse of target color to correct)
        // If scene is too warm, we apply cool correction (boost blue, reduce red)
        float r_mult = (1.0f / std::max(0.001f, tr)) * rb_mult;
        float g_mult = (1.0f / std::max(0.001f, tg)) * green_mult;
        float b_mult = (1.0f / std::max(0.001f, tb)) * rb_mult;

        // Normalize to preserve mid-gray
        float avg = (r_mult + g_mult + b_mult) / 3.0f;
        r_mult /= avg;
        g_mult /= avg;
        b_mult /= avg;

        try
        {
            // Split channels
            std::vector<cv::UMat> channels(3);
            cv::split(input, channels);

            // Apply multipliers (OpenCV is BGR)
            cv::multiply(channels[0], b_mult, channels[0]);
            cv::multiply(channels[1], g_mult, channels[1]);
            cv::multiply(channels[2], r_mult, channels[2]);

            // Merge back
            cv::merge(channels, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[WhiteBalance] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
