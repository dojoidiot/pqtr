#pragma once

#include "types.hpp"
#include <cstring>
#include <cmath>

namespace copy::core {

    struct PipeState {
        int width = 0;
        int height = 0;
        uint32_t filters = 0;   /* Bayer pattern from decoder */

        /* Camera data */
        float adobe_XYZ_to_CAM[4][3] = {{0}};   /* Color matrix from rawspeed/cameras.xml */
        float d65_color_matrix[9] = {0};       /* Embedded 3x3 matrix from DNG (NaN if invalid) */
        float exposure_bias = 0.0f;             /* EV adjustment for this camera model */

        /* Module outputs */
        struct {
            int enabled = 0;
            float coeffs[4] = {0};
        } temperature;

        struct {
            double D65coeffs[4] = {0};
            double as_shot[4] = {0};
            int late_correction = 0;
        } chroma;
    };

    // Helper to initialize the exact Sony values as per the original code's "pipe_state_init_sony"
    inline void pipe_state_init_sony(PipeState& state) {
        state.width = 6048;
        state.height = 4024;
        state.filters = 2492765332;  /* 0x94949494 = RGGB */

        /* temperature output */
        state.temperature.enabled = 1;
        state.temperature.coeffs[0] = 2.511718750f;
        state.temperature.coeffs[1] = 1.000000000f;
        state.temperature.coeffs[2] = 1.457031250f;
        state.temperature.coeffs[3] = 0.000000000f;

        /* chroma state */
        state.chroma.late_correction = 1;
        state.chroma.as_shot[0] = 2.511718750;
        state.chroma.as_shot[1] = 1.000000000;
        state.chroma.as_shot[2] = 1.457031250;
        state.chroma.as_shot[3] = 0.000000000;
        state.chroma.D65coeffs[0] = 2.671514144;
        state.chroma.D65coeffs[1] = 1.000000000;
        state.chroma.D65coeffs[2] = 1.347009702;
        state.chroma.D65coeffs[3] = INFINITY;  /* DT has inf here */
    }

}
