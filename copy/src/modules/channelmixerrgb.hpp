#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"

namespace copy::modules::channelmixerrgb {

    using Colormatrix = float[4][4];
    using AlignedPixel = float[4];

    struct Params {
        int adaptation;
        AlignedPixel illuminant;
        Colormatrix MIX;
        AlignedPixel saturation;
        AlignedPixel lightness;
        AlignedPixel grey;
        float p;
        float gamut;
        int clip;
        int apply_grey;
        int version;
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, 
                 const Colormatrix& RGB_to_XYZ, const Colormatrix& XYZ_to_RGB, const Params& p);

}
