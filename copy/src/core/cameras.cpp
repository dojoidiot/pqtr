#include "cameras.hpp"
#include <cstring>

namespace copy::core {

#define DEFAULT_STYLE { 0.0f, 0.1845f, -8.0f, 4.0f, 0.1f, 0.5f }
#define SONY_ILCE7M3_STYLE { 1.34f, 0.1845f, -5.0f, 4.0f, 0.1f, 0.5f }

    static const CameraData camera_db[] = {
        /* ------------------------------------------------------------------------
           Sony Alpha cameras
           ------------------------------------------------------------------------ */
        {
            "Sony",
            "ILCE-7M3",
            {
                 0.7374f, -0.2389f, -0.0551f,
                -0.5435f,  1.3162f,  0.2519f,
                -0.1006f,  0.1795f,  0.6552f
            },
            512,
            16383,
            0x94949494,  /* RGGB */
            SONY_ILCE7M3_STYLE
        },
        {
            "Sony",
            "ILCE-7RM3",
            {
                 0.7374f, -0.2389f, -0.0551f,
                -0.5435f,  1.3162f,  0.2519f,
                -0.1006f,  0.1795f,  0.6552f
            },
            512,
            16383,
            0x94949494,
            SONY_ILCE7M3_STYLE
        },
        {
            "Sony",
            "ILCE-9",
            {
                 0.7374f, -0.2389f, -0.0551f,
                -0.5435f,  1.3162f,  0.2519f,
                -0.1006f,  0.1795f,  0.6552f
            },
            512,
            16383,
            0x94949494,
            SONY_ILCE7M3_STYLE
        },
        {
            "Sony",
            "ILCE-7M4",
            {
                 0.7460f, -0.2365f, -0.0546f,
                -0.5765f,  1.3586f,  0.2389f,
                -0.1153f,  0.2064f,  0.6398f
            },
            512,
            16383,
            0x94949494,
            SONY_ILCE7M3_STYLE
        },
        {
            "Sony",
            "ILCE-7RM4",
            {
                 0.7460f, -0.2365f, -0.0546f,
                -0.5765f,  1.3586f,  0.2389f,
                -0.1153f,  0.2064f,  0.6398f
            },
            512,
            16383,
            0x94949494,
            SONY_ILCE7M3_STYLE
        },

        /* ------------------------------------------------------------------------
           Canon cameras (examples)
           ------------------------------------------------------------------------ */
        {
            "Canon",
            "EOS R5",
            {
                 0.8530f, -0.2169f, -0.0671f,
                -0.4665f,  1.2287f,  0.2616f,
                -0.0854f,  0.1481f,  0.6114f
            },
            2048,
            16383,
            0x94949494,
            DEFAULT_STYLE
        },
        {
            "Canon",
            "EOS R6",
            {
                 0.8530f, -0.2169f, -0.0671f,
                -0.4665f,  1.2287f,  0.2616f,
                -0.0854f,  0.1481f,  0.6114f
            },
            2048,
            16383,
            0x94949494,
            DEFAULT_STYLE
        },

        /* Sentinel */
        { nullptr, nullptr, {0}, 0, 0, 0, {0} }
    };

    const CameraData* cameras_lookup(const char* make, const char* model) {
        if (!make || !model) return nullptr;

        for (const CameraData* cam = camera_db; cam->make != nullptr; cam++) {
            if (strcmp(cam->make, make) == 0 && strcmp(cam->model, model) == 0) {
                return cam;
            }
        }
        return nullptr;
    }

    void cameras_compute_d65(const float xyz_to_cam[9], float d65_coeffs[4]) {
        /* sRGB D65 RGB_to_XYZ matrix */
        static const float RGB_to_XYZ[3][3] = {
            { 0.412453f, 0.357580f, 0.180423f },
            { 0.212671f, 0.715160f, 0.072169f },
            { 0.019334f, 0.119193f, 0.950227f }
        };

        /* RGB_to_CAM = XYZ_to_CAM * RGB_to_XYZ */
        float RGB_to_CAM[3][3];
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                RGB_to_CAM[i][j] = 0.0f;
                for (int k = 0; k < 3; k++) {
                    RGB_to_CAM[i][j] += xyz_to_cam[i * 3 + k] * RGB_to_XYZ[k][j];
                }
            }
        }

        /* mul = 1/row_sum, then normalize by green */
        float mul[3];
        for (int i = 0; i < 3; i++) {
            float sum = RGB_to_CAM[i][0] + RGB_to_CAM[i][1] + RGB_to_CAM[i][2];
            mul[i] = (sum != 0.0f) ? 1.0f / sum : 0.0f;
        }

        d65_coeffs[0] = mul[0] / mul[1];  /* R */
        d65_coeffs[1] = 1.0f;             /* G */
        d65_coeffs[2] = mul[2] / mul[1];  /* B */
        d65_coeffs[3] = 1.0f;             /* G2 */
    }

}
