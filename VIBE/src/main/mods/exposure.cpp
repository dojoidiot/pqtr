// exposure.cpp - VIBE
// Exposure Module - Adjusts overall brightness via EV shift
// Part of Color Correction module (1 of 3 dials)

#include "mods.h"
#include <iostream>
#include <cmath>

namespace vibe
{
namespace mods
{
    // Dial: exposure (0.0 - 1.0)
    //   0.0 = -4 EV (16x darker)
    //   0.5 = 0 EV (neutral)
    //   1.0 = +4 EV (16x brighter)
    bool exposure(const View& in, View& out, Dial dial)
    {
        if (in.empty())
        {
            std::cerr << "[vibe::exposure] empty input\n";
            return false;
        }

        if (in.type() != CV_32FC3)
        {
            std::cerr << "[vibe::exposure] need CV_32FC3\n";
            return false;
        }

        dial = std::clamp(dial, 0.0f, 1.0f);
        float ev = (dial - 0.5f) * 8.0f;
        float mult = std::pow(2.0f, ev);

        cv::multiply(in, mult, out);
        return true;
    }

    // Direct EV value (not a dial) - used by baseline
    bool exposure_ev(const View& in, View& out, float ev)
    {
        if (in.empty() || in.type() != CV_32FC3)
        {
            std::cerr << "[vibe::exposure_ev] invalid input\n";
            return false;
        }

        float mult = std::pow(2.0f, ev);
        cv::multiply(in, mult, out);
        return true;
    }

} // namespace mods
} // namespace vibe
