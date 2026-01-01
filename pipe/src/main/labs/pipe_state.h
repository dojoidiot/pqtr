/*
    PipeState - pipeline state passed through all modules

    Mirrors DT's piece->pipe->dsc and image_storage

    Camera data is populated by pipe_prepare from DT's knowledge base.
    Module outputs are stored by modules for downstream use.
*/

#ifndef PIPE_STATE_H
#define PIPE_STATE_H

#include <stdint.h>

typedef struct {
    int width;
    int height;
    uint32_t filters;   /* Bayer pattern from decoder, adjusted for row order */

    /* ========================================================================
       Camera data - populated by pipe_prepare from cameras.xml
       From image.h lines 310, 343
       ======================================================================== */
    float adobe_XYZ_to_CAM[4][3];   /* Color matrix from rawspeed/cameras.xml */
    float d65_color_matrix[9];       /* Embedded 3x3 matrix from DNG (NaN if invalid) */

    /* ========================================================================
       Module outputs - set by modules for downstream use
       ======================================================================== */

    /* From format.h lines 52-56: set by temperature module */
    struct {
        int enabled;
        float coeffs[4];
    } temperature;

    /* From develop.h lines 147-156: dt_dev_chroma_t - set by temperature module */
    struct {
        double D65coeffs[4];
        double as_shot[4];
        int late_correction;
    } chroma;
} PipeState;

/* Common filter patterns (after row reversal in head decoder) */
#define FILTERS_GBRG 0x49494949  /* Sony ARW after row reversal */
#define FILTERS_RGGB 0x94949494  /* Standard RGGB */

#endif /* PIPE_STATE_H */
