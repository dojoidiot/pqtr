#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"
#include "../core/pipe_state.hpp"

namespace copy::modules::highlights {

    struct Params {
        float clip;
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const core::PipeState& state, const Params& p);

}
