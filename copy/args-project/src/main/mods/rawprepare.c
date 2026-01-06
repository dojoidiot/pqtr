/*
    rawprepare - EXACT COPY of darktable/src/iop/rawprepare.c process()

    Input: uint16 bayer mosaic (from head)
    Output: float32 bayer mosaic (normalized to 0-1)
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================================================
   From rawprepare.c - dt_iop_rawprepare_params_t (lines 46-55)
   ============================================================================ */

typedef struct {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
    uint16_t raw_black_level_separate[4];
    uint16_t raw_white_point;
    int flat_field;  /* 0 = off, 1 = embedded gainmap */
} RawprepareParams;

/* ============================================================================
   From rawprepare.c - dt_iop_rawprepare_data_t (lines 65-82)
   ============================================================================ */

typedef struct {
    int32_t left, top, right, bottom;
    float sub[4];
    float div[4];
} RawprepareData;

/* ============================================================================
   From rawprepare.c - _BL function (lines 314-320)
   Computes Bayer channel index for a given pixel position
   ============================================================================ */

static inline int _BL(int row, int col, int top, int left)
{
    /* return ((((row + roi_out->y + d->top) & 1) << 1) + ((col + roi_out->x + d->left) & 1)); */
    /* Simplified: roi_out->y = 0, roi_out->x = 0 for full image */
    return ((((row + top) & 1) << 1) + ((col + left) & 1));
}

/* ============================================================================
   From rawprepare.c - commit_params (lines 681-741)
   Converts params to processing data
   ============================================================================ */

static void rawprepare_commit_params(const RawprepareParams* p, RawprepareData* d)
{
    d->left = p->left;
    d->top = p->top;
    d->right = p->right;
    d->bottom = p->bottom;

    /* From lines 694-702:
       if(piece->pipe->dsc.filters)
       {
         const float white = (float)p->raw_white_point;
         for(int i = 0; i < 4; i++)
         {
           d->sub[i] = (float)p->raw_black_level_separate[i];
           d->div[i] = (white - d->sub[i]);
         }
       }
    */
    const float white = (float)p->raw_white_point;
    for (int i = 0; i < 4; i++)
    {
        d->sub[i] = (float)p->raw_black_level_separate[i];
        d->div[i] = (white - d->sub[i]);
    }
}

/* ============================================================================
   From rawprepare.c - process() lines 322-358 (raw mosaic u16 path)
   ============================================================================ */

void rawprepare_process(
    const uint16_t* in,
    float* out,
    int width_in,
    int height_in,
    int width_out,
    int height_out,
    const RawprepareData* d)
{
    /* From lines 331-332:
       const int csx = _compute_proper_crop(piece, roi_in, d->left);
       const int csy = _compute_proper_crop(piece, roi_in, d->top);

       _compute_proper_crop at scale=1.0 just returns the value unchanged.
    */
    const int csx = d->left;
    const int csy = d->top;

    /* From lines 336-357:
       if(piece->pipe->dsc.filters && piece->dsc_in.channels == 1
          && piece->dsc_in.datatype == TYPE_UINT16)
       { // raw mosaic
         const uint16_t *const in = (const uint16_t *const)ivoid;
         DT_OMP_FOR_SIMD(collapse(2))
         for(int j = 0; j < roi_out->height; j++)
         {
           for(int i = 0; i < roi_out->width; i++)
           {
             const size_t pin = (size_t)(roi_in->width * (j + csy) + csx) + i;
             const size_t pout = (size_t)j * roi_out->width + i;
             const int id = _BL(roi_out, d, j, i);
             out[pout] = (in[pin] - d->sub[id]) / d->div[id];
           }
         }
         ...
       }
    */
    for (int j = 0; j < height_out; j++)
    {
        for (int i = 0; i < width_out; i++)
        {
            const size_t pin = (size_t)(width_in * (j + csy) + csx) + i;
            const size_t pout = (size_t)j * width_out + i;

            const int id = _BL(j, i, d->top, d->left);
            out[pout] = ((float)in[pin] - d->sub[id]) / d->div[id];
        }
    }
}

/* ============================================================================
   From rawprepare.c - reload_defaults (lines 754-778)
   Default values come from image metadata
   ============================================================================ */

void rawprepare_reset(RawprepareParams* p,
                      int crop_x, int crop_y, int crop_right, int crop_bottom,
                      uint16_t black0, uint16_t black1, uint16_t black2, uint16_t black3,
                      uint16_t white_point)
{
    /* From lines 762-771:
       *d = (dt_iop_rawprepare_params_t){
         .left = image->crop_x,
         .top = image->crop_y,
         .right = image->crop_right,
         .bottom = image->crop_bottom,
         .raw_black_level_separate[0] = image->raw_black_level_separate[0],
         .raw_black_level_separate[1] = image->raw_black_level_separate[1],
         .raw_black_level_separate[2] = image->raw_black_level_separate[2],
         .raw_black_level_separate[3] = image->raw_black_level_separate[3],
         .raw_white_point = image->raw_white_point,
         .flat_field = ... };
    */
    p->left = crop_x;
    p->top = crop_y;
    p->right = crop_right;
    p->bottom = crop_bottom;
    p->raw_black_level_separate[0] = black0;
    p->raw_black_level_separate[1] = black1;
    p->raw_black_level_separate[2] = black2;
    p->raw_black_level_separate[3] = black3;
    p->raw_white_point = white_point;
    p->flat_field = 0;
}

/* Note: No rawprepare_defaults() - all fields are image metadata.
   RawprepareData must be computed from PipeState via rawprepare_commit_params().
   See mods/ARCHITECTURE.md */
