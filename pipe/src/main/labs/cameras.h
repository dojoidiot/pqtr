/*
    cameras.h - Camera color matrix database

    Source: Mirrors DT's adobe_coeff.c + rawspeed cameras.xml

    Contains per-camera:
    - XYZ_to_CAM color matrix (for color calibration)
    - Default black/white levels (fallback if not in RAW)
    - Bayer pattern
*/

#ifndef CAMERAS_H
#define CAMERAS_H

typedef struct {
    const char* make;           /* "Sony", "Canon", etc. */
    const char* model;          /* "ILCE-7M3", "EOS R5", etc. */
    float xyz_to_cam[9];        /* XYZ->CAM 3x3 matrix (from adobe_coeff.c) */
    int black_level;            /* Default black level */
    int white_level;            /* Default white level */
    unsigned int filters;       /* Bayer pattern (e.g. 0x94949494 = RGGB) */
} CameraData;

/* Lookup camera by make/model. Returns NULL if not found. */
const CameraData* cameras_lookup(const char* make, const char* model);

/* Compute D65 WB coefficients from xyz_to_cam matrix */
void cameras_compute_d65(const float xyz_to_cam[9], float d65_coeffs[4]);

#endif /* CAMERAS_H */
