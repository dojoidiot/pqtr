#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"

namespace copy::modules::colorout {

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const float cmatrix[4][4]);

}
