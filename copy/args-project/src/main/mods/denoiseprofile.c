/*
    denoiseprofile.c - Profiled denoise using wavelets

    Copied from darktable src/iop/denoiseprofile.c
    Simplified to wavelets-only mode with hardcoded Sony profiles.

    Algorithm:
    1. Variance Stabilizing Transform (Anscombe)
    2. Multi-scale wavelet decomposition
    3. Soft thresholding at each scale
    4. Inverse transform
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================================
   Noise Profile Data
   ============================================================================ */

typedef struct {
    const char* name;
    int iso;
    float a[3];  /* Poissonian (shot noise) coefficients per channel */
    float b[3];  /* Gaussian (read noise) coefficients per channel */
} NoiseProfile;

/* Sony ILCE-7M3 noise profiles from darktable noiseprofiles.json */
static const NoiseProfile sony_ilce7m3_profiles[] = {
    {"ILCE-7M3 iso 100", 100, {3.61133e-06f, 1.39193e-06f, 2.02105e-06f}, {4.33706e-09f, 7.01676e-11f, 1.48355e-09f}},
    {"ILCE-7M3 iso 200", 200, {6.02756e-06f, 2.31907e-06f, 3.42744e-06f}, {1.05972e-08f, 3.48672e-09f, 5.73044e-09f}},
    {"ILCE-7M3 iso 400", 400, {1.06914e-05f, 4.09269e-06f, 6.13203e-06f}, {2.42335e-08f, 1.05841e-08f, 1.77707e-08f}},
    {"ILCE-7M3 iso 800", 800, {2.07473e-05f, 7.81336e-06f, 1.19115e-05f}, {3.40775e-08f, 1.35759e-08f, 2.65097e-08f}},
    {"ILCE-7M3 iso 1600", 1600, {3.94336e-05f, 1.48172e-05f, 2.27145e-05f}, {6.80892e-08f, 3.51493e-08f, 5.89197e-08f}},
    {"ILCE-7M3 iso 3200", 3200, {7.71938e-05f, 2.89964e-05f, 4.47198e-05f}, {1.22224e-07f, 7.95450e-08f, 1.24202e-07f}},
    {"ILCE-7M3 iso 6400", 6400, {1.52242e-04f, 5.74746e-05f, 8.87507e-05f}, {2.08717e-07f, 1.72654e-07f, 2.33172e-07f}},
    {"ILCE-7M3 iso 12800", 12800, {3.00050e-04f, 1.12560e-04f, 1.75664e-04f}, {3.21934e-07f, 3.33934e-07f, 3.92406e-07f}},
    {"ILCE-7M3 iso 25600", 25600, {5.89629e-04f, 2.21213e-04f, 3.46155e-04f}, {3.72155e-07f, 4.19684e-07f, 4.40553e-07f}},
    {NULL, 0, {0,0,0}, {0,0,0}}
};

/* Generic fallback profile */
static const NoiseProfile generic_profile = {
    "generic poissonian", 0, {0.0001f, 0.0001f, 0.0001f}, {0.0f, 0.0f, 0.0f}
};

/* Interpolate between two profiles based on ISO */
static void interpolate_profile(const NoiseProfile* p1, const NoiseProfile* p2,
                                 int target_iso, float* a, float* b)
{
    if (p1->iso == p2->iso || target_iso <= p1->iso) {
        for (int c = 0; c < 3; c++) { a[c] = p1->a[c]; b[c] = p1->b[c]; }
        return;
    }
    if (target_iso >= p2->iso) {
        for (int c = 0; c < 3; c++) { a[c] = p2->a[c]; b[c] = p2->b[c]; }
        return;
    }

    /* Linear interpolation in log space */
    float t = (logf(target_iso) - logf(p1->iso)) / (logf(p2->iso) - logf(p1->iso));
    for (int c = 0; c < 3; c++) {
        a[c] = p1->a[c] * powf(p2->a[c] / p1->a[c], t);
        b[c] = p1->b[c] + t * (p2->b[c] - p1->b[c]);
    }
}

/* Get noise profile for camera and ISO */
static void get_noise_profile(const char* maker, const char* model, int iso,
                               float* a, float* b)
{
    const NoiseProfile* profiles = NULL;

    /* Match camera */
    if (maker && model) {
        if (strstr(maker, "SONY") || strstr(maker, "Sony")) {
            if (strstr(model, "ILCE-7M3") || strstr(model, "ILCE-7RM3") ||
                strstr(model, "ILCE-7RM4") || strstr(model, "ILCE-7RM5")) {
                profiles = sony_ilce7m3_profiles;
            }
        }
    }

    if (!profiles) {
        /* Use generic profile */
        for (int c = 0; c < 3; c++) {
            a[c] = generic_profile.a[c];
            b[c] = generic_profile.b[c];
        }
        return;
    }

    /* Find bracketing profiles and interpolate */
    const NoiseProfile* p1 = &profiles[0];
    const NoiseProfile* p2 = &profiles[0];

    for (int i = 0; profiles[i].name != NULL; i++) {
        if (profiles[i].iso <= iso) p1 = &profiles[i];
        if (profiles[i].iso >= iso) { p2 = &profiles[i]; break; }
        p2 = &profiles[i];
    }

    interpolate_profile(p1, p2, iso, a, b);
}

/* ============================================================================
   Denoise Parameters
   ============================================================================ */

#define DENOISE_BANDS 5
#define MAX_SCALES 7

typedef struct {
    float strength;        /* Overall denoise strength (default 1.0) */
    float shadows;         /* Shadow preservation (default 1.0) */
    float a[3];            /* Noise profile a coefficients */
    float b[3];            /* Noise profile b coefficients */
    int max_scales;        /* Number of wavelet scales */
} DenoiseProfileData;

static void denoiseprofile_reset(DenoiseProfileData* d)
{
    d->strength = 1.0f;
    d->shadows = 1.0f;
    d->max_scales = 5;
    for (int c = 0; c < 3; c++) {
        d->a[c] = 0.0001f;
        d->b[c] = 0.0f;
    }
}

static void denoiseprofile_set_profile(DenoiseProfileData* d,
                                        const char* maker, const char* model, int iso)
{
    get_noise_profile(maker, model, iso, d->a, d->b);
    printf("denoiseprofile: ISO %d -> a=[%.2e, %.2e, %.2e] b=[%.2e, %.2e, %.2e]\n",
           iso, d->a[0], d->a[1], d->a[2], d->b[0], d->b[1], d->b[2]);
}

/* ============================================================================
   Variance Stabilizing Transform (Generalized Anscombe)
   ============================================================================ */

static inline float fast_sqrtf(float x) { return sqrtf(x); }

/* Forward transform: makes noise variance constant */
static void precondition(const float* in, float* out,
                          int width, int height,
                          const float* a, const float* b)
{
    /* sigma2 = (b/a)^2 + 3/8 */
    float sigma2[4];
    for (int c = 0; c < 3; c++) {
        float ratio = (a[c] > 1e-10f) ? (b[c] / a[c]) : 0.0f;
        sigma2[c] = ratio * ratio + 3.0f / 8.0f;
    }
    sigma2[3] = 0.0f;

    size_t npixels = (size_t)width * height;

    #pragma omp parallel for
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float val = in[i * 4 + c];
            float d = fmaxf(0.0f, val / a[c] + sigma2[c]);
            out[i * 4 + c] = 2.0f * fast_sqrtf(d);
        }
        out[i * 4 + 3] = 0.0f;
    }
}

/* Inverse transform */
static void backtransform(float* buf, int width, int height,
                           const float* a, const float* b)
{
    float sigma2[3];
    for (int c = 0; c < 3; c++) {
        float ratio = (a[c] > 1e-10f) ? (b[c] / a[c]) : 0.0f;
        sigma2[c] = ratio * ratio + 1.0f / 8.0f;
    }

    const float sqrt_3_2 = sqrtf(3.0f / 2.0f);
    size_t npixels = (size_t)width * height;

    #pragma omp parallel for
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float x = buf[i * 4 + c];
            float x2 = x * x;

            if (x < 0.5f) {
                buf[i * 4 + c] = 0.0f;
            } else {
                /* Closed form approximation to unbiased inverse */
                float val = 0.25f * x2
                          + 0.25f * sqrt_3_2 / x
                          - 11.0f / 8.0f / x2
                          + 5.0f / 8.0f * sqrt_3_2 / (x * x2)
                          - sigma2[c];
                buf[i * 4 + c] = a[c] * fmaxf(0.0f, val);
            }
        }
    }
}

/* ============================================================================
   Wavelet Decomposition (Edge-Aware)
   ============================================================================ */

/* 5x5 binomial filter for wavelet decomposition */
static const float wavelet_filter[25] = {
    1.0f/256, 4.0f/256, 6.0f/256, 4.0f/256, 1.0f/256,
    4.0f/256, 16.0f/256, 24.0f/256, 16.0f/256, 4.0f/256,
    6.0f/256, 24.0f/256, 36.0f/256, 24.0f/256, 6.0f/256,
    4.0f/256, 16.0f/256, 24.0f/256, 16.0f/256, 4.0f/256,
    1.0f/256, 4.0f/256, 6.0f/256, 4.0f/256, 1.0f/256
};

/* Clamp index to image bounds */
static inline int clamp_idx(int x, int max) {
    if (x < 0) return 0;
    if (x >= max) return max - 1;
    return x;
}

/* Single scale wavelet decomposition */
static void wavelet_decompose(const float* in, float* coarse, float* detail,
                               int width, int height, int scale)
{
    int mult = 1 << scale;  /* Filter spacing: 1, 2, 4, 8... */

    #pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum[4] = {0, 0, 0, 0};
            float wgt = 0.0f;

            /* Apply 5x5 filter with spacing 'mult' */
            int fi = 0;
            for (int fy = -2; fy <= 2; fy++) {
                for (int fx = -2; fx <= 2; fx++) {
                    int ny = clamp_idx(y + fy * mult, height);
                    int nx = clamp_idx(x + fx * mult, width);

                    float w = wavelet_filter[fi++];
                    wgt += w;

                    const float* px = in + (ny * width + nx) * 4;
                    for (int c = 0; c < 3; c++) {
                        sum[c] += w * px[c];
                    }
                }
            }

            /* Normalize and compute detail */
            size_t idx = (y * width + x) * 4;
            for (int c = 0; c < 3; c++) {
                float low = sum[c] / wgt;
                coarse[idx + c] = low;
                detail[idx + c] = in[idx + c] - low;
            }
            coarse[idx + 3] = 0.0f;
            detail[idx + 3] = 0.0f;
        }
    }
}

/* Soft thresholding of detail coefficients */
static void threshold_detail(float* detail, const float* threshold,
                              int width, int height)
{
    size_t npixels = (size_t)width * height;

    #pragma omp parallel for
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float d = detail[i * 4 + c];
            float t = threshold[c];

            /* Soft threshold: sign(d) * max(|d| - t, 0) */
            if (d > t) {
                detail[i * 4 + c] = d - t;
            } else if (d < -t) {
                detail[i * 4 + c] = d + t;
            } else {
                detail[i * 4 + c] = 0.0f;
            }
        }
    }
}

/* ============================================================================
   Main Denoise Process
   ============================================================================ */

void denoiseprofile_process(const float* in, float* out,
                             int width, int height,
                             const DenoiseProfileData* d)
{
    size_t npixels = (size_t)width * height;
    size_t bufsize = npixels * 4 * sizeof(float);

    /* Allocate buffers */
    float* precond = (float*)malloc(bufsize);
    float* buf1 = (float*)malloc(bufsize);
    float* buf2 = (float*)malloc(bufsize);
    float* detail = (float*)malloc(bufsize);

    if (!precond || !buf1 || !buf2 || !detail) {
        fprintf(stderr, "denoiseprofile: allocation failed\n");
        memcpy(out, in, bufsize);
        free(precond); free(buf1); free(buf2); free(detail);
        return;
    }

    /* Scale noise coefficients by strength */
    float a_scaled[3], b_scaled[3];
    for (int c = 0; c < 3; c++) {
        a_scaled[c] = d->a[c] * d->strength;
        b_scaled[c] = d->b[c] * d->strength;
    }

    /* 1. Variance stabilizing transform */
    precondition(in, precond, width, height, a_scaled, b_scaled);

    /* 2. Multi-scale wavelet decomposition and thresholding */
    memcpy(buf1, precond, bufsize);
    memset(out, 0, bufsize);  /* Accumulate thresholded details */

    int max_scale = d->max_scales;

    /* Limit scale based on image size */
    int min_dim = (width < height) ? width : height;
    while ((1 << max_scale) * 4 > min_dim && max_scale > 1) {
        max_scale--;
    }

    for (int scale = 0; scale < max_scale; scale++) {
        /* Decompose: buf1 -> buf2 (coarse) + detail */
        wavelet_decompose(buf1, buf2, detail, width, height, scale);

        /* Compute threshold for this scale
           threshold = sigma_band * strength
           sigma_band increases with scale (noise is spread across scales) */
        float sigma_band = 1.0f;
        for (int s = 0; s < scale; s++) {
            sigma_band *= 0.5f;  /* Each scale halves effective noise */
        }

        float threshold[3];
        for (int c = 0; c < 3; c++) {
            /* Base threshold from noise model, scaled by band */
            threshold[c] = sigma_band * d->strength * 1.5f;  /* Tuned factor */
        }

        /* Apply soft thresholding */
        threshold_detail(detail, threshold, width, height);

        /* Accumulate thresholded detail into output */
        #pragma omp parallel for
        for (size_t i = 0; i < npixels * 4; i++) {
            out[i] += detail[i];
        }

        /* Swap buffers for next scale */
        float* tmp = buf1;
        buf1 = buf2;
        buf2 = tmp;
    }

    /* Add final residue (lowest frequency) */
    #pragma omp parallel for
    for (size_t i = 0; i < npixels * 4; i++) {
        out[i] += buf1[i];
    }

    /* 3. Inverse variance stabilizing transform */
    backtransform(out, width, height, a_scaled, b_scaled);

    /* Cleanup */
    free(precond);
    free(buf1);
    free(buf2);
    free(detail);
}
