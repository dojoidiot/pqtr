#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"

namespace copy::modules::colorbalancergb {

    using Colormatrix = float[4][4];

    struct Params {
        float global[4];
        float shadows[4];
        float highlights[4];
        float midtones[4];
        float midtones_Y;
        float chroma_global, chroma[4], vibrance, contrast;
        float saturation_global, saturation[4];
        float brilliance_global, brilliance[4];
        float hue_angle;
        float shadows_weight, highlights_weight, midtones_weight, mask_grey_fulcrum;
        float white_fulcrum, grey_fulcrum;
        float gamut_LUT[512];
        float max_chroma;
        int saturation_formula;
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, 
                 const Colormatrix& input_matrix, const Colormatrix& output_matrix, const Params& p);

}
