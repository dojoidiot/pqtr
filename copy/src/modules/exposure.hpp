#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"

namespace copy::modules::exposure {

    struct Params {
        float exposure;
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const Params& p);

}
