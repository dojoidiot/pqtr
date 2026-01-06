#pragma once

#include "../core/types.hpp"
#include "../core/image_buffer.hpp"

namespace copy::modules::colorin {

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const float cam_to_xyz[3][3]);

}