#pragma once

#include "types.hpp"

namespace copy::core {

    struct CameraStyle {
        float exposure_ev;
        float filmic_grey;
        float filmic_black_ev;
        float filmic_white_ev;
        float bilat_detail;
        float bilat_midtone;
    };

    struct CameraData {
        const char* make;
        const char* model;
        float xyz_to_cam[9];
        int black_level;
        int white_level;
        unsigned int filters;
        CameraStyle style;
    };

    const CameraData* cameras_lookup(const char* make, const char* model);
    void cameras_compute_d65(const float xyz_to_cam[9], float d65_coeffs[4]);

}
