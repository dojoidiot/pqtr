#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"

namespace copy::modules::bilat {

    struct Params {
        int mode;
        float sigma_r;
        float sigma_s;
        float detail;
        float midtone;
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const Params& p);

}
