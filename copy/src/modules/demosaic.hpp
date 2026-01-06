#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"
#include "../core/pipe_state.hpp"

namespace copy::modules::demosaic {

    struct Params {
        int demosaicing_method; // 0=PPG, 5=RCD
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const core::PipeState& state, const Params& p);

}
