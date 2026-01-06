#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"
#include "../core/pipe_state.hpp"

namespace copy::modules::temperature {

    struct Params {
        float coeffs[4];
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, core::PipeState& state, const Params& p);

}
