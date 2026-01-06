#pragma once

#include "../core/types.hpp"
#include "../core/cameras.hpp"
#include <string>
#include <vector>

namespace copy::core {

    struct PictureProfile {
        int creative_style = 0;
        int picture_profile = 0;
        float saturation = 0.0f;
        float vibrance = 0.0f;
        float contrast = 0.0f;
    };

    struct MetaData {
        int width = 0;
        int height = 0;
        int strip_offset = 0;
        u16 sony_curve[4] = {0};
        int black_level = 512;
        int white_level = 16383;
        float wb_rggb[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float color_matrix[9] = {0};
        uint32_t filters = 0x94949494;
        float exposure_bias = 0.0f;
        float xyz_to_cam[9] = {0};
        float d65_coeffs[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const CameraData* camera = nullptr;
        PictureProfile profile;
        int dro_level = 0;
        float dro_shadow_lift = 1.0f;
        int iso = 100;
        uint32_t preview_offset = 0;
        uint32_t preview_length = 0;
    };

}
