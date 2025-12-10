// white_balance.cpp - VIBE
// White Balance Module - Adjusts color temperature and tint
// Part of Color Correction module (2 of 3 dials)

#include "mods.h"
#include <iostream>
#include <cmath>
#include <vector>

namespace vibe
{
namespace mods
{
    // Convert color temperature (Kelvin) to RGB multipliers
    // Based on Tanner Helland's algorithm (approximation of Planckian locus)
    static void kelvin_to_rgb(float kelvin, float& r, float& g, float& b)
    {
        float temp = kelvin / 100.0f;

        // Red
        if (temp <= 66.0f)
            r = 1.0f;
        else
        {
            r = temp - 60.0f;
            r = 329.698727446f * std::pow(r, -0.1332047592f);
            r = std::clamp(r / 255.0f, 0.0f, 1.0f);
        }

        // Green
        if (temp <= 66.0f)
        {
            g = temp;
            g = 99.4708025861f * std::log(g) - 161.1195681661f;
            g = std::clamp(g / 255.0f, 0.0f, 1.0f);
        }
        else
        {
            g = temp - 60.0f;
            g = 288.1221695283f * std::pow(g, -0.0755148492f);
            g = std::clamp(g / 255.0f, 0.0f, 1.0f);
        }

        // Blue
        if (temp >= 66.0f)
            b = 1.0f;
        else if (temp <= 19.0f)
            b = 0.0f;
        else
        {
            b = temp - 10.0f;
            b = 138.5177312231f * std::log(b) - 305.0447927307f;
            b = std::clamp(b / 255.0f, 0.0f, 1.0f);
        }
    }

    // Dial: temperature (0.0 - 1.0)
    //   0.0 = 2000K (very warm/orange)
    //   0.5 = 6500K (daylight neutral)
    //   1.0 = 12000K (very cool/blue)
    //
    // Dial: tint (0.0 - 1.0)
    //   0.0 = green shift
    //   0.5 = neutral
    //   1.0 = magenta shift
    bool white_balance(const View& in, View& out, Dial temperature, Dial tint)
    {
        if (in.empty() || in.type() != CV_32FC3)
        {
            std::cerr << "[vibe::white_balance] invalid input\n";
            return false;
        }

        temperature = std::clamp(temperature, 0.0f, 1.0f);
        tint = std::clamp(tint, 0.0f, 1.0f);

        // Convert temperature dial to Kelvin (piecewise exponential)
        float kelvin;
        if (temperature < 0.5f)
        {
            float t = temperature * 2.0f;
            kelvin = 2000.0f * std::pow(6500.0f / 2000.0f, t);
        }
        else
        {
            float t = (temperature - 0.5f) * 2.0f;
            kelvin = 6500.0f * std::pow(12000.0f / 6500.0f, t);
        }

        // Get RGB values for this color temperature
        float tr, tg, tb;
        kelvin_to_rgb(kelvin, tr, tg, tb);

        // Normalize to preserve brightness (green as reference)
        float norm = 1.0f / std::max(0.001f, tg);
        tr *= norm;
        tg = 1.0f;
        tb *= norm;

        // Apply tint (green/magenta shift)
        float tint_shift = (tint - 0.5f) * 0.4f;
        float green_mult = 1.0f - tint_shift;
        float rb_mult = 1.0f + tint_shift * 0.5f;

        // Final multipliers (inverse of target color to correct)
        float r_mult = (1.0f / std::max(0.001f, tr)) * rb_mult;
        float g_mult = (1.0f / std::max(0.001f, tg)) * green_mult;
        float b_mult = (1.0f / std::max(0.001f, tb)) * rb_mult;

        // Normalize to preserve mid-gray
        float avg = (r_mult + g_mult + b_mult) / 3.0f;
        r_mult /= avg;
        g_mult /= avg;
        b_mult /= avg;

        // Split channels, apply multipliers (OpenCV is BGR)
        std::vector<cv::UMat> channels(3);
        cv::split(in, channels);

        cv::multiply(channels[0], b_mult, channels[0]);
        cv::multiply(channels[1], g_mult, channels[1]);
        cv::multiply(channels[2], r_mult, channels[2]);

        cv::merge(channels, out);
        return true;
    }

} // namespace mods
} // namespace vibe
