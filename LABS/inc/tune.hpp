#pragma once

#include "lute.hpp"

namespace tune
{
    // Learn the 3D color LUT from a TONE-corrected output and a reference JPEG.
    // This function will populate the LUT-related parts of the CameraLut struct.
    void learn(const float* tone_rgb, int width, int height,
               const uint8_t* ref_rgb8, int ref_width, int ref_height,
               lute::CameraLut& profile);

    // Apply the learned 3D color LUT to the full-resolution image.
    // The `in` and `out` pointers can be the same for in-place modification.
    void apply(const float* in_rgb, float* out_rgb, int width, int height,
               const lute::CameraLut& profile);
}
