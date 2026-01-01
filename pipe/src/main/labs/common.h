/*
    Common functions shared across modules

    Copied from darktable source:
    - FC() from develop/imageop_math.h
*/
#ifndef LABS_COMMON_H
#define LABS_COMMON_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
   From imageop_math.h - FC (Filter Color) function
   Returns Bayer color index for given row/col
   ============================================================================ */

static inline int FC(const size_t row, const size_t col, const uint32_t filters)
{
    return (filters >> (((row << 1 & 14) + (col & 1)) << 1)) & 3;
}

#endif /* LABS_COMMON_H */
