#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"
#include "../core/metadata.hpp"

namespace copy::modules::rawprepare {

    struct Params {
        int left;
        int top;
        int right;
        int bottom;
        core::u16 raw_black_level_separate[4];
        core::u16 raw_white_point;
    };

    void process(const core::ImageBuffer<core::u16>& in, core::ImageBuffer<core::f32>& out, const Params& p);

}
