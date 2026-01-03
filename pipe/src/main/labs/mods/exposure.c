/*
 * exposure.c - Exposure correction module
 * Copied from darktable src/iop/exposure.c process() lines 541-566
 */

#include <stdint.h>
#include <stddef.h>
#include <math.h>

typedef struct {
    int mode;
    float black;
    float exposure;
    float deflicker_percentile;
    float deflicker_target_level;
    int compensate_exposure_bias;
} ExposureParams;

#define exposure2white(x) exp2f(-(x))

void exposure_process(const float *in, float *out,
                      int width, int height, int ch,
                      const ExposureParams *p)
{
    const float black = p->black;
    const float white = exposure2white(p->exposure);
    const float scale = 1.0f / (white - black);
    const size_t npixels = (size_t)width * height;

    for (size_t k = 0; k < (size_t)ch * npixels; k++)
    {
        out[k] = (in[k] - black) * scale;
    }
}

/* ============================================================================
   Helper: return ExposureParams with defaults (no correction)
   ============================================================================ */

static inline ExposureParams exposure_defaults(void)
{
    ExposureParams p;
    p.mode = 0;                    /* EXPOSURE_MODE_MANUAL */
    p.black = 0.0f;
    p.exposure = 0.0f;             /* 0 EV = no change */
    p.deflicker_percentile = 50.0f;
    p.deflicker_target_level = -4.0f;
    p.compensate_exposure_bias = 0;
    return p;
}
