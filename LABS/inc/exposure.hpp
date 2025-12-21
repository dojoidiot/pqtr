#pragma once

#include <cstdint>

namespace exposure
{
    struct Params
    {
        float correction = 1.0f; // Multiplicative factor
    };

    // Learn the exposure correction required to match the target's brightness.
    Params learn(const float* in_rgb, int width, int height,
                 const uint8_t* ref_rgb8, int ref_width, int ref_height);

    // Apply exposure correction. Can be done in-place.
    void apply(float* rgb, int width, int height, const Params& params);
}
