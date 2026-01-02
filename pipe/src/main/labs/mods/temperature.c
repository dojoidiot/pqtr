/*
    temperature - EXACT COPY of darktable/src/iop/temperature.c process()

    Input: float32 bayer mosaic (from rawprepare)
    Output: float32 bayer mosaic (white balanced)
*/

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "../pipe_state.h"

/* ============================================================================
   Inlined from common.h - FC (Filter Color) for Bayer index
   ============================================================================ */

#ifndef FC_DEFINED
#define FC_DEFINED
static inline int FC(const size_t row, const size_t col, const uint32_t filters)
{
    return (filters >> (((row << 1 & 14) + (col & 1)) << 1)) & 3;
}
#endif

/* ============================================================================
   From temperature.c - dt_iop_temperature_params_t (lines 68-75)
   ============================================================================ */

typedef struct {
    float red;
    float green;
    float blue;
    float various;
    int preset;
} TemperatureParams;

/* ============================================================================
   From temperature.c - dt_iop_temperature_data_t (lines 101-105)
   ============================================================================ */

typedef struct {
    float coeffs[4];
    int preset;
} TemperatureData;

/* ============================================================================
   From temperature.c - commit_params (lines 688-729)
   ============================================================================ */

static void temperature_commit_params(const TemperatureParams* p, TemperatureData* d)
{
    /* From lines 709-713:
       for_four_channels(k)
       {
         d->coeffs[k] = tcoeffs[k];
         ...
       }
    */
    d->coeffs[0] = p->red;
    d->coeffs[1] = p->green;
    d->coeffs[2] = p->blue;
    d->coeffs[3] = p->various;
    d->preset = p->preset;
}

/* ============================================================================
   From temperature.c - process() lines 585-623 (bayer float mosaiced path)
   ============================================================================ */

void temperature_process(
    const float* in,
    float* out,
    PipeState* state,  /* Modified: sets dsc.temperature like DT line 521-524 */
    const TemperatureData* d)
{
    /* From temperature.c lines 521-524: _publish_chroma sets dsc.temperature */
    state->temperature.enabled = 1;
    for (int k = 0; k < 4; k++)
        state->temperature.coeffs[k] = d->coeffs[k];

    /* From lines 585-623:
       else if(filters)
       { // bayer float mosaiced
         const int width = roi_out->width;
         DT_OMP_FOR()
         for(int j = 0; j < roi_out->height; j++)
         {
           ...
           for(; i < width; i++)
           {
             const size_t p = (size_t)j * width + i;
             out[p] = in[p] * d_coeffs[FC(offset_j, i + roi_out->x, filters)];
           }
         }
       }

       Simplified: roi_out->x = 0, roi_out->y = 0 for full image
    */
    const float* d_coeffs = d->coeffs;
    const int width = state->width;
    const int height = state->height;
    const uint32_t filters = state->filters;

    for (int j = 0; j < height; j++)
    {
        const int offset_j = j;  /* roi_out->y = 0 */
        for (int i = 0; i < width; i++)
        {
            const size_t p = (size_t)j * width + i;
            out[p] = in[p] * d_coeffs[FC(offset_j, i, filters)];
        }
    }
}

/* ============================================================================
   Reset to values from XMP or metadata
   ============================================================================ */

void temperature_reset(TemperatureParams* p,
                       float red, float green, float blue, float various,
                       int preset)
{
    p->red = red;
    p->green = green;
    p->blue = blue;
    p->various = various;
    p->preset = preset;
}
