#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"

namespace copy::modules::denoiseprofile {

    struct Params {
        float strength;
        float shadows;
        float a[3];
        float b[3];
        int max_scales;
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const Params& p);
    void set_profile(Params& p, const char* maker, const char* model, int iso);

}
