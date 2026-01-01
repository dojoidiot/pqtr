/*
    Phase 2 Pipeline Test

    Takes the bilat output (Lab) and converts to final sRGB PNG.
    Bilat is the last module in phase2.xmp.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* Include colorout module for Lab -> sRGB conversion */
#include "../../main/labs/pipe_state.h"
#include "../../main/labs/mods/colorout.c"

/* Load PFM and convert from reversed rows to normal */
static float* load_pfm_rgba(const char* path, int* width, int* height, int expected_ch) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return NULL;
    }

    char magic[3] = {0};
    if (fscanf(f, "%2s\n", magic) != 1) {
        fclose(f);
        return NULL;
    }

    int channels;
    if (strcmp(magic, "PF") == 0) channels = 3;
    else if (strcmp(magic, "Pf") == 0) channels = 1;
    else {
        fprintf(stderr, "Invalid PFM magic: %s\n", magic);
        fclose(f);
        return NULL;
    }

    if (channels != expected_ch) {
        fprintf(stderr, "Expected %d channels, got %d\n", expected_ch, channels);
        fclose(f);
        return NULL;
    }

    int w, h;
    if (fscanf(f, "%d %d\n", &w, &h) != 2) {
        fclose(f);
        return NULL;
    }

    float scale;
    if (fscanf(f, "%f\n", &scale) != 1) {
        fclose(f);
        return NULL;
    }

    size_t npixels = (size_t)w * h;
    float* raw = (float*)malloc(npixels * channels * sizeof(float));
    if (fread(raw, sizeof(float), npixels * channels, f) != npixels * channels) {
        free(raw);
        fclose(f);
        return NULL;
    }
    fclose(f);

    /* Convert to 4-channel RGBA with row reversal (PFM stores rows bottom-up) */
    float* rgba = (float*)malloc(npixels * 4 * sizeof(float));
    for (int row = 0; row < h; row++) {
        int src_row = h - 1 - row;
        for (int col = 0; col < w; col++) {
            size_t src_idx = (size_t)src_row * w + col;
            size_t dst_idx = (size_t)row * w + col;
            rgba[dst_idx*4+0] = raw[src_idx*channels+0];
            rgba[dst_idx*4+1] = (channels >= 2) ? raw[src_idx*channels+1] : 0.0f;
            rgba[dst_idx*4+2] = (channels >= 3) ? raw[src_idx*channels+2] : 0.0f;
            rgba[dst_idx*4+3] = 0.0f;
        }
    }
    free(raw);

    *width = w;
    *height = h;
    return rgba;
}

static void write_png(const char* filename, uint8_t* data, int width, int height) {
    /* Write PPM first, then convert via Python */
    char ppm_file[512];
    snprintf(ppm_file, sizeof(ppm_file), "%s.ppm", filename);

    FILE* f = fopen(ppm_file, "wb");
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    fwrite(data, 1, (size_t)width * height * 3, f);
    fclose(f);

    /* Convert to PNG */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "python3 -c \"from PIL import Image; Image.open('%s').save('%s')\" 2>/dev/null",
        ppm_file, filename);
    int ret = system(cmd);
    (void)ret;

    /* Remove temp PPM */
    remove(ppm_file);
}

int main() {
    printf("Phase 2 Pipeline Test\n");
    printf("=====================\n\n");

    /* Load bilat output (Lab space) */
    printf("[1/3] Loading bilat output from DT dump...\n");
    int width, height;
    float* lab = load_pfm_rgba("/tmp/dtdump/export/0001_bilat_cpu_out_C.pfm", &width, &height, 3);
    if (!lab) {
        fprintf(stderr, "ERROR: Cannot load bilat output PFM\n");
        fprintf(stderr, "Run: DT_DUMP_PFM_MODULE=bilat darktable-cli src/test/raws/sony.ARW src/test/raws/phase2.xmp /tmp/out.png\n");
        return 1;
    }
    printf("  Image: %dx%d\n", width, height);

    /* Run colorout (Lab -> sRGB) */
    printf("[2/3] Converting Lab -> sRGB (colorout)...\n");

    PipeState state;
    memset(&state, 0, sizeof(state));
    state.width = width;
    state.height = height;

    /* XYZ to sRGB matrix (same as phase1 test - standard sRGB) */
    float cmatrix[4][4] = {
        { 3.13423491f, -1.61725771f, -0.4906919f, 0.0f },
        { -0.97874099f, 1.91611922f, 0.0334379375f, 0.0f },
        { 0.0719688162f, -0.229020134f, 1.40577972f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    size_t npixels = (size_t)width * height;
    float* rgb = (float*)malloc(npixels * 4 * sizeof(float));

    /* Debug: check what Lab values we have */
    printf("  Pixel 0 Lab: %.4f %.4f %.4f\n", lab[0], lab[1], lab[2]);
    printf("  Center pixel Lab: %.4f %.4f %.4f\n",
           lab[(npixels/2)*4], lab[(npixels/2)*4+1], lab[(npixels/2)*4+2]);

    colorout_process(lab, rgb, &state, cmatrix);

    /* Debug: check RGB output */
    printf("  Pixel 0 RGB: %.4f %.4f %.4f\n", rgb[0], rgb[1], rgb[2]);

    /* Convert to 8-bit sRGB */
    printf("[3/3] Converting to 8-bit PNG...\n");

    uint8_t* out_rgb = (uint8_t*)malloc(npixels * 3);
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb[i * 4 + c];
            v = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
            out_rgb[i * 3 + c] = (uint8_t)(v * 255.0f + 0.5f);
        }
    }

    /* Write output */
    printf("\nWriting output...\n");
    write_png("tmp/var/phase2_output.png", out_rgb, width, height);
    printf("  tmp/var/phase2_output.png\n");

    /* Cleanup */
    free(lab);
    free(rgb);
    free(out_rgb);

    printf("\nDone!\n");
    return 0;
}
