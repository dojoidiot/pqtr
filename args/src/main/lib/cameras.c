/*
    cameras.c - Camera color matrix database

    Data copied from:
    - darktable/src/external/adobe_coeff.c (color matrices)
    - rawspeed/data/cameras.xml (black/white levels, filters)

    This is DT's "expert system" for camera-specific data.
    Add camera entries as needed from adobe_coeff.c.
*/

#include "cameras.h"
#include <string.h>
#include <stddef.h>

/* ============================================================================
   Camera database

   Matrix values from adobe_coeff.c (already scaled to float)
   Black/white levels from rawspeed cameras.xml or typical camera defaults
   ============================================================================ */

/* Default style for cameras without a specific DT style */
#define DEFAULT_STYLE { 0.0f, 0.1845f, -8.0f, 4.0f, 0.1f, 0.5f }

/* Sony ILCE-7M3 style from darktable_Sony_ILCE-7M3.dtstyle
   exposure: 1.34 EV (auto-tuned from JPEG brightness matching)
   filmic: grey=0.1845, black=-5.0, white=4.0 */
#define SONY_ILCE7M3_STYLE { 1.34f, 0.1845f, -5.0f, 4.0f, 0.1f, 0.5f }

static const CameraData camera_db[] = {
    /* ------------------------------------------------------------------------
       Sony Alpha cameras
       ------------------------------------------------------------------------ */
    {
        .make = "Sony",
        .model = "ILCE-7M3",
        .xyz_to_cam = {
             0.7374f, -0.2389f, -0.0551f,
            -0.5435f,  1.3162f,  0.2519f,
            -0.1006f,  0.1795f,  0.6552f
        },
        .black_level = 512,
        .white_level = 16383,
        .filters = 0x94949494,  /* RGGB */
        .style = SONY_ILCE7M3_STYLE
    },
    {
        .make = "Sony",
        .model = "ILCE-7RM3",
        .xyz_to_cam = {
             0.7374f, -0.2389f, -0.0551f,
            -0.5435f,  1.3162f,  0.2519f,
            -0.1006f,  0.1795f,  0.6552f
        },
        .black_level = 512,
        .white_level = 16383,
        .filters = 0x94949494,
        .style = SONY_ILCE7M3_STYLE  /* Same style as 7M3 */
    },
    {
        .make = "Sony",
        .model = "ILCE-9",
        .xyz_to_cam = {
             0.7374f, -0.2389f, -0.0551f,
            -0.5435f,  1.3162f,  0.2519f,
            -0.1006f,  0.1795f,  0.6552f
        },
        .black_level = 512,
        .white_level = 16383,
        .filters = 0x94949494,
        .style = SONY_ILCE7M3_STYLE
    },
    {
        .make = "Sony",
        .model = "ILCE-7M4",
        .xyz_to_cam = {
             0.7460f, -0.2365f, -0.0546f,
            -0.5765f,  1.3586f,  0.2389f,
            -0.1153f,  0.2064f,  0.6398f
        },
        .black_level = 512,
        .white_level = 16383,
        .filters = 0x94949494,
        .style = SONY_ILCE7M3_STYLE
    },
    {
        .make = "Sony",
        .model = "ILCE-7RM4",
        .xyz_to_cam = {
             0.7460f, -0.2365f, -0.0546f,
            -0.5765f,  1.3586f,  0.2389f,
            -0.1153f,  0.2064f,  0.6398f
        },
        .black_level = 512,
        .white_level = 16383,
        .filters = 0x94949494,
        .style = SONY_ILCE7M3_STYLE
    },

    /* ------------------------------------------------------------------------
       Canon cameras (examples - add more as needed)
       ------------------------------------------------------------------------ */
    {
        .make = "Canon",
        .model = "EOS R5",
        .xyz_to_cam = {
             0.8530f, -0.2169f, -0.0671f,
            -0.4665f,  1.2287f,  0.2616f,
            -0.0854f,  0.1481f,  0.6114f
        },
        .black_level = 2048,
        .white_level = 16383,
        .filters = 0x94949494,
        .style = DEFAULT_STYLE
    },
    {
        .make = "Canon",
        .model = "EOS R6",
        .xyz_to_cam = {
             0.8530f, -0.2169f, -0.0671f,
            -0.4665f,  1.2287f,  0.2616f,
            -0.0854f,  0.1481f,  0.6114f
        },
        .black_level = 2048,
        .white_level = 16383,
        .filters = 0x94949494,
        .style = DEFAULT_STYLE
    },

    /* Sentinel - must be last */
    { NULL, NULL, {0}, 0, 0, 0, {0} }
};

/* ============================================================================
   cameras_lookup - find camera by make/model
   ============================================================================ */

const CameraData* cameras_lookup(const char* make, const char* model)
{
    if (!make || !model) return NULL;

    for (const CameraData* cam = camera_db; cam->make != NULL; cam++) {
        if (strcmp(cam->make, make) == 0 && strcmp(cam->model, model) == 0) {
            return cam;
        }
    }
    return NULL;  /* Unknown camera */
}

/* ============================================================================
   cameras_compute_d65 - compute D65 WB coefficients from color matrix

   From DT colorspaces.c:dt_colorspaces_conversion_matrices_rgb
   ============================================================================ */

void cameras_compute_d65(const float xyz_to_cam[9], float d65_coeffs[4])
{
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
