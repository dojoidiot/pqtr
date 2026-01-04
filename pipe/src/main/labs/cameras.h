/*
    cameras.h - Camera database (color matrix + style parameters)

    Source:
    - darktable/src/external/adobe_coeff.c (color matrices)
    - rawspeed/data/cameras.xml (black/white levels, filters)
    - darktable/share/darktable/styles/*.dtstyle (camera style parameters)

    Contains per-camera:
    - XYZ_to_CAM color matrix (for color calibration)
    - Default black/white levels (fallback if not in RAW)
    - Bayer pattern
    - Camera style parameters (exposure, filmic, bilat)
*/

#ifndef CAMERAS_H
#define CAMERAS_H

/* Camera style parameters from DT .dtstyle files */
typedef struct {
    float exposure_ev;          /* Total exposure adjustment (default + style) */
    float filmic_grey;          /* Middle grey (typically 0.1845) */
    float filmic_black_ev;      /* Black point in EV relative to grey */
    float filmic_white_ev;      /* White point in EV relative to grey */
    float bilat_detail;         /* Local contrast detail/clarity */
    float bilat_midtone;        /* Midtone sigma */
} CameraStyle;

typedef struct {
    const char* make;           /* "Sony", "Canon", etc. */
    const char* model;          /* "ILCE-7M3", "EOS R5", etc. */
    float xyz_to_cam[9];        /* XYZ->CAM 3x3 matrix (from adobe_coeff.c) */
    int black_level;            /* Default black level */
    int white_level;            /* Default white level */
    unsigned int filters;       /* Bayer pattern (e.g. 0x94949494 = RGGB) */
    CameraStyle style;          /* Pre-tuned style parameters */
} CameraData;

/* Lookup camera by make/model. Returns NULL if not found. */
const CameraData* cameras_lookup(const char* make, const char* model);

/* Compute D65 WB coefficients from xyz_to_cam matrix */
void cameras_compute_d65(const float xyz_to_cam[9], float d65_coeffs[4]);

#endif /* CAMERAS_H */
