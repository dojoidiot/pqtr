#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace copy::modules::autotune {

    float raw_exposure_autotune(const std::string& raw_path, uint32_t preview_offset, uint32_t preview_length, 
                                const uint8_t* pipeline_srgb, int width, int height);

}
