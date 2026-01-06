#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"

namespace copy::modules::filmicrgb {

    using Colormatrix = float[4][4];
    using AlignedPixel = float[4];

    enum CurveType {
        DT_FILMIC_CURVE_POLY_4 = 0,
        DT_FILMIC_CURVE_POLY_3 = 1,
        DT_FILMIC_CURVE_RATIONAL = 2
    };

    struct Spline {
        AlignedPixel M1, M2, M3, M4, M5;
        float latitude_min, latitude_max;
        float y[5];
        float x[5];
        CurveType type[2];
    };

    struct Params {
        float grey_source;
        float black_source;
        float white_source;
        float dynamic_range;
        float normalize;
        float output_power;
        float contrast;
        float saturation;
        float sigma_toe, sigma_shoulder;
        Spline spline;
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, 
                 const Params& data,
                 const Colormatrix& input_matrix_trans,
                 const Colormatrix& output_matrix,
                 const Colormatrix& output_matrix_trans,
                 const Colormatrix& export_input_matrix_trans,
                 const Colormatrix& export_output_matrix,
                 const Colormatrix& export_output_matrix_trans,
                 float display_black, float display_white,
                 int use_output_profile);

    void autotune(Params& data, const core::ImageBuffer<core::f32>& in);
    void compute_spline(Params& d);

}
