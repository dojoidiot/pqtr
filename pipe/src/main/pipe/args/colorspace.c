/*
 * colorspace.c - RGB/Lab conversion (COPIED from darktable)
 *
 * Uses D50 white point and Rec2020 working space.
 * Lab range: L [0,100], a/b [-128,128]
 */

#include <math.h>
#include <stddef.h>

/* D50 white point */
static const float d50[3] = { 0.9642f, 1.0f, 0.8249f };
static const float d50_inv[3] = { 1.0f/0.9642f, 1.0f, 1.0f/0.8249f };

/* Rec2020 to XYZ (D50) matrix */
static const float rec2020_to_xyz[3][3] = {
    { 0.6370f, 0.1446f, 0.1689f },
    { 0.2627f, 0.6780f, 0.0593f },
    { 0.0000f, 0.0281f, 1.0610f }
};

/* XYZ (D50) to Rec2020 matrix */
static const float xyz_to_rec2020[3][3] = {
    {  1.7167f, -0.3557f, -0.2534f },
    { -0.6667f,  1.6165f,  0.0158f },
    {  0.0176f, -0.0428f,  0.9421f }
};

/* Lab f function */
static inline float lab_f(const float x)
{
    const float epsilon = 216.0f / 24389.0f;
    const float kappa = 24389.0f / 27.0f;
    if (x > epsilon) {
        /* Approximate cbrt using fast initial guess and Halley iteration */
        union { float f; unsigned int i; } u = { x };
        u.i = u.i / 3 + 709921077;
        float a = u.f;
        float a3 = a * a * a;
        return a * (a3 + x + x) / (a3 + a3 + x);
    }
    return (kappa * x + 16.0f) / 116.0f;
}

/* Lab f inverse */
static inline float lab_f_inv(const float x)
{
    const float epsilon = 0.20689655172413796f; /* cbrt(216/24389) */
    const float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? x * x * x : (116.0f * x - 16.0f) / kappa;
}

/* XYZ to Lab */
static inline void xyz_to_lab(const float XYZ[3], float Lab[3])
{
    float f[3];
    for (int i = 0; i < 3; i++)
        f[i] = lab_f(XYZ[i] * d50_inv[i]);

    Lab[0] = 116.0f * f[1] - 16.0f;
    Lab[1] = 500.0f * (f[0] - f[1]);
    Lab[2] = 200.0f * (f[1] - f[2]);
}

/* Lab to XYZ */
static inline void lab_to_xyz(const float Lab[3], float XYZ[3])
{
    float fy = (Lab[0] + 16.0f) / 116.0f;
    float fx = Lab[1] / 500.0f + fy;
    float fz = fy - Lab[2] / 200.0f;

    XYZ[0] = d50[0] * lab_f_inv(fx);
    XYZ[1] = d50[1] * lab_f_inv(fy);
    XYZ[2] = d50[2] * lab_f_inv(fz);
}

/* RGB (Rec2020) to XYZ */
static inline void rgb_to_xyz(const float rgb[3], float XYZ[3])
{
    for (int i = 0; i < 3; i++) {
        XYZ[i] = rec2020_to_xyz[i][0] * rgb[0]
               + rec2020_to_xyz[i][1] * rgb[1]
               + rec2020_to_xyz[i][2] * rgb[2];
    }
}

/* XYZ to RGB (Rec2020) */
static inline void xyz_to_rgb(const float XYZ[3], float rgb[3])
{
    for (int i = 0; i < 3; i++) {
        rgb[i] = xyz_to_rec2020[i][0] * XYZ[0]
               + xyz_to_rec2020[i][1] * XYZ[1]
               + xyz_to_rec2020[i][2] * XYZ[2];
    }
}

/* ============================================================================
   Public API: Process full image
   ============================================================================ */

void colorspace_rgb_to_lab(const float *in, float *out, int width, int height)
{
    size_t npixels = (size_t)width * height;

    for (size_t i = 0; i < npixels; i++) {
        const float *px_in = in + i * 4;
        float *px_out = out + i * 4;

        float rgb[3] = { px_in[0], px_in[1], px_in[2] };
        float XYZ[3], Lab[3];

        rgb_to_xyz(rgb, XYZ);
        xyz_to_lab(XYZ, Lab);

        px_out[0] = Lab[0];  /* L: 0-100 */
        px_out[1] = Lab[1];  /* a: ~-128 to +128 */
        px_out[2] = Lab[2];  /* b: ~-128 to +128 */
        px_out[3] = px_in[3]; /* preserve alpha */
    }
}

void colorspace_lab_to_rgb(const float *in, float *out, int width, int height)
{
    size_t npixels = (size_t)width * height;

    for (size_t i = 0; i < npixels; i++) {
        const float *px_in = in + i * 4;
        float *px_out = out + i * 4;

        float Lab[3] = { px_in[0], px_in[1], px_in[2] };
        float XYZ[3], rgb[3];

        lab_to_xyz(Lab, XYZ);
        xyz_to_rgb(XYZ, rgb);

        px_out[0] = rgb[0];
        px_out[1] = rgb[1];
        px_out[2] = rgb[2];
        px_out[3] = px_in[3]; /* preserve alpha */
    }
}
