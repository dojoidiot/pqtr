/*
    autotune.c - JPEG matching autotune

    Analyzes on-camera JPEG to derive optimal pipeline parameters.

    Main function: jpeg_exposure_match()
    - Compares pipeline output brightness to JPEG brightness
    - Returns EV adjustment to match JPEG
*/

#include "autotune.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* stb_image for JPEG loading */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include "stb_image.h"

/* ============================================================================
   sRGB linearization
   ============================================================================ */

static float srgb_to_linear(float v)
{
    /* v is 0-1 sRGB */
    if (v <= 0.04045f)
        return v / 12.92f;
    else
        return powf((v + 0.055f) / 1.055f, 2.4f);
}

/* ============================================================================
   JPEG Filmic Matching
   ============================================================================ */

/*
    Analyze JPEG to derive filmic parameters.

    Method (similar to DT's filmicrgb autotune):
    1. Convert JPEG sRGB to linear light
    2. Find min/max luminance (black/white points)
    3. Calculate grey point from mean
    4. Derive black_EV, white_EV, exposure adjustment
*/
FilmicAutotuneResult jpeg_filmic_autotune(const char* jpeg_path)
{
    FilmicAutotuneResult result = {0};
    result.valid = 0;
    result.grey = 0.1845f;  /* DT default */

    /* Load JPEG */
    int width, height, channels;
    unsigned char* data = stbi_load(jpeg_path, &width, &height, &channels, 3);
    if (!data) {
        fprintf(stderr, "autotune: cannot load JPEG %s\n", jpeg_path);
        return result;
    }

    printf("autotune: loaded JPEG %s (%dx%d)\n", jpeg_path, width, height);

    /* Analyze in linear light */
    double sum_luma = 0.0;
    float min_luma = 1e10f;
    float max_luma = -1e10f;
    long count = (long)width * height;

    /* Histogram for percentile calculation */
    int histogram[256] = {0};

    for (long i = 0; i < count; i++) {
        unsigned char* px = &data[i * 3];

        /* sRGB 0-255 to 0-1 */
        float r = px[0] / 255.0f;
        float g = px[1] / 255.0f;
        float b = px[2] / 255.0f;

        /* Linearize */
        float lin_r = srgb_to_linear(r);
        float lin_g = srgb_to_linear(g);
        float lin_b = srgb_to_linear(b);

        /* Rec.709 luminance */
        float luma = 0.2126f * lin_r + 0.7152f * lin_g + 0.0722f * lin_b;

        if (luma > 1e-6f) {  /* Avoid log of zero */
            sum_luma += luma;
            if (luma < min_luma) min_luma = luma;
            if (luma > max_luma) max_luma = luma;
        }

        /* Build histogram of sRGB luminance for percentile */
        int srgb_luma = (int)(0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2] + 0.5f);
        if (srgb_luma > 255) srgb_luma = 255;
        histogram[srgb_luma]++;
    }

    stbi_image_free(data);

    /* Find 1st and 99th percentile to avoid outliers */
    long p1_target = count / 100;
    long p99_target = count * 99 / 100;
    long cumsum = 0;
    int p1_srgb = 0, p99_srgb = 255;

    for (int i = 0; i < 256; i++) {
        cumsum += histogram[i];
        if (cumsum >= p1_target && p1_srgb == 0) {
            p1_srgb = i;
        }
        if (cumsum >= p99_target) {
            p99_srgb = i;
            break;
        }
    }

    /* Convert percentiles to linear */
    float p1_linear = srgb_to_linear(p1_srgb / 255.0f);
    float p99_linear = srgb_to_linear(p99_srgb / 255.0f);

    /* Mean luminance (geometric mean approximation via log) */
    float mean_luma = (float)(sum_luma / count);

    /* Calculate grey point - where middle grey should map */
    /* In JPEG, middle grey (18%) should appear around sRGB 118 (46%) */
    result.grey = mean_luma;
    if (result.grey < 0.01f) result.grey = 0.01f;
    if (result.grey > 0.5f) result.grey = 0.5f;

    /* Calculate EV range
       black_EV = log2(black / grey)
       white_EV = log2(white / grey)
    */
    if (p1_linear > 1e-6f && result.grey > 1e-6f) {
        result.black_ev = log2f(p1_linear / result.grey);
    } else {
        result.black_ev = -8.0f;  /* DT default */
    }

    if (p99_linear > 1e-6f && result.grey > 1e-6f) {
        result.white_ev = log2f(p99_linear / result.grey);
    } else {
        result.white_ev = 4.0f;  /* DT default */
    }

    /* Clamp to reasonable range (DT uses -16 to -1 for black, 1 to 16 for white) */
    if (result.black_ev < -16.0f) result.black_ev = -16.0f;
    if (result.black_ev > -1.0f) result.black_ev = -1.0f;
    if (result.white_ev < 1.0f) result.white_ev = 1.0f;
    if (result.white_ev > 16.0f) result.white_ev = 16.0f;

    /* Exposure adjustment: how much to shift our pipeline to match JPEG brightness
       JPEG mean vs expected 18% grey */
    float expected_grey = 0.1845f;
    if (mean_luma > 1e-6f) {
        result.exposure_ev = log2f(mean_luma / expected_grey);
    } else {
        result.exposure_ev = 0.0f;
    }

    /* Dynamic range */
    result.dynamic_range = result.white_ev - result.black_ev;

    result.valid = 1;

    printf("autotune: JPEG analysis:\n");
    printf("  p1=%d p99=%d (sRGB 0-255)\n", p1_srgb, p99_srgb);
    printf("  min_luma=%.6f max_luma=%.4f mean_luma=%.4f\n", min_luma, max_luma, mean_luma);
    printf("  grey=%.4f black_ev=%.2f white_ev=%.2f DR=%.2f\n",
           result.grey, result.black_ev, result.white_ev, result.dynamic_range);
    printf("  exposure_ev=%.2f (adjustment to match JPEG brightness)\n", result.exposure_ev);

    return result;
}

/* ============================================================================
   JPEG Exposure Matching

   Compare pipeline output to JPEG and calculate optimal EV adjustment.
   This is the "learn from reference" function.
   ============================================================================ */

/*
    Calculate mean brightness of sRGB image data (uint8, 0-255).
    Returns mean luminance in linear light (0-1 range).
*/
static float srgb_mean_brightness(const unsigned char* data, int width, int height, int channels)
{
    double sum = 0.0;
    long count = (long)width * height;

    for (long i = 0; i < count; i++) {
        const unsigned char* px = &data[i * channels];
        float r = srgb_to_linear(px[0] / 255.0f);
        float g = srgb_to_linear(px[1] / 255.0f);
        float b = srgb_to_linear(px[2] / 255.0f);
        /* Rec.709 luminance */
        float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        sum += luma;
    }

    return (float)(sum / count);
}

/*
    Calculate EV adjustment to match JPEG brightness.

    Parameters:
    - jpeg_path: path to reference JPEG
    - pipeline_srgb: pipeline output (uint8 sRGB, 3 channels)
    - width, height: image dimensions

    Returns: EV adjustment (add to current exposure to match JPEG)
*/
float jpeg_exposure_match(const char* jpeg_path,
                          const unsigned char* pipeline_srgb,
                          int width, int height)
{
    /* Load JPEG */
    int jpeg_w, jpeg_h, jpeg_ch;
    unsigned char* jpeg_data = stbi_load(jpeg_path, &jpeg_w, &jpeg_h, &jpeg_ch, 3);
    if (!jpeg_data) {
        fprintf(stderr, "autotune: cannot load JPEG %s\n", jpeg_path);
        return 0.0f;
    }

    /* Calculate mean brightness of both images */
    float jpeg_brightness = srgb_mean_brightness(jpeg_data, jpeg_w, jpeg_h, 3);
    float pipeline_brightness = srgb_mean_brightness(pipeline_srgb, width, height, 3);

    stbi_image_free(jpeg_data);

    /* Calculate EV adjustment: log2(target / current) */
    float ev_adjustment = 0.0f;
    if (pipeline_brightness > 1e-6f && jpeg_brightness > 1e-6f) {
        ev_adjustment = log2f(jpeg_brightness / pipeline_brightness);
    }

    printf("autotune: JPEG exposure match:\n");
    printf("  JPEG brightness: %.4f (linear)\n", jpeg_brightness);
    printf("  Pipeline brightness: %.4f (linear)\n", pipeline_brightness);
    printf("  EV adjustment: %+.2f\n", ev_adjustment);

    return ev_adjustment;
}

/* ============================================================================
   Embedded JPEG Extraction

   Extract the embedded preview JPEG from a RAW file.
   Uses TIFF preview offset/length parsed by sony.c.
   ============================================================================ */

/*
    Extract embedded JPEG preview from RAW file using known offset/length.

    Parameters:
    - raw_path: path to RAW file
    - preview_offset: byte offset of JPEG in file
    - preview_length: length of JPEG data
    - out_jpeg_path: where to write the extracted JPEG

    Returns: 0 on success, -1 on failure
*/
int extract_embedded_jpeg(const char* raw_path, unsigned int preview_offset,
                          unsigned int preview_length, const char* out_jpeg_path)
{
    if (preview_offset == 0 || preview_length == 0) {
        fprintf(stderr, "autotune: no embedded preview found\n");
        return -1;
    }

    FILE* f_in = fopen(raw_path, "rb");
    if (!f_in) {
        fprintf(stderr, "autotune: cannot open %s\n", raw_path);
        return -1;
    }

    /* Seek to preview offset */
    if (fseek(f_in, preview_offset, SEEK_SET) != 0) {
        fclose(f_in);
        fprintf(stderr, "autotune: cannot seek to preview at offset %u\n", preview_offset);
        return -1;
    }

    /* Read preview data */
    unsigned char* jpeg_data = (unsigned char*)malloc(preview_length);
    if (!jpeg_data) {
        fclose(f_in);
        return -1;
    }

    size_t read_len = fread(jpeg_data, 1, preview_length, f_in);
    fclose(f_in);

    if (read_len != preview_length) {
        free(jpeg_data);
        fprintf(stderr, "autotune: failed to read preview (got %zu, expected %u)\n",
                read_len, preview_length);
        return -1;
    }

    /* Verify JPEG header */
    if (jpeg_data[0] != 0xFF || jpeg_data[1] != 0xD8) {
        free(jpeg_data);
        fprintf(stderr, "autotune: preview is not valid JPEG\n");
        return -1;
    }

    /* Write to temp file */
    FILE* f_out = fopen(out_jpeg_path, "wb");
    if (!f_out) {
        free(jpeg_data);
        return -1;
    }

    fwrite(jpeg_data, 1, preview_length, f_out);
    fclose(f_out);
    free(jpeg_data);

    return 0;
}

/*
    Auto-tune exposure from embedded JPEG in RAW file.

    This is the main entry point for automatic calibration:
    1. Extract embedded JPEG from RAW using known offset/length
    2. Compare pipeline output to embedded JPEG
    3. Return EV adjustment

    Parameters:
    - raw_path: path to RAW file
    - preview_offset: byte offset of embedded JPEG
    - preview_length: length of embedded JPEG
    - pipeline_srgb: pipeline output (uint8 sRGB, 3 channels)
    - width, height: pipeline output dimensions

    Returns: EV adjustment to add to current exposure
*/
float raw_exposure_autotune(const char* raw_path,
                            unsigned int preview_offset,
                            unsigned int preview_length,
                            const unsigned char* pipeline_srgb,
                            int width, int height)
{
    const char* tmp_jpeg = "/tmp/pqtr_embedded.jpg";

    /* Extract embedded JPEG */
    if (extract_embedded_jpeg(raw_path, preview_offset, preview_length, tmp_jpeg) != 0) {
        fprintf(stderr, "autotune: cannot extract embedded JPEG, using 0 EV adjustment\n");
        return 0.0f;
    }

    printf("autotune: using embedded JPEG from %s\n", raw_path);

    /* Calculate EV adjustment */
    float ev = jpeg_exposure_match(tmp_jpeg, pipeline_srgb, width, height);

    /* Clean up temp file */
    remove(tmp_jpeg);

    return ev;
}
