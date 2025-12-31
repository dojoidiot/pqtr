/*
    Phase 1 Pipeline Test

    Tests the complete phase1 pipeline using DT dumps as reference.
    Outputs to tmp/var/phase1_output.png and tmp/var/phase1_diff.png
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "../../main/labs/pipe_state.h"
#include "../../main/labs/mods/colorout.c"

static int skip_pfm_header(FILE* f) {
    int newlines = 0;
    while (newlines < 3) {
        int c = fgetc(f);
        if (c == EOF) return -1;
        if (c == '\n') newlines++;
    }
    return 0;
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
    system(cmd);

    /* Remove temp PPM */
    remove(ppm_file);
}

int main() {
    const int width = 6048, height = 4024;
    const size_t npixels = (size_t)width * height;

    printf("Phase 1 Pipeline Test\n");
    printf("=====================\n");
    printf("Image: %dx%d (%zu pixels)\n\n", width, height, npixels);

    /* Load colorin output (Lab) from DT dump */
    printf("[1/4] Loading colorin output from DT dump...\n");
    FILE* f_in = fopen("/tmp/dtdump/export/0001_colorout_cpu_in_C.pfm", "rb");
    if (!f_in) {
        fprintf(stderr, "ERROR: Cannot open /tmp/dtdump/export/0001_colorout_cpu_in_C.pfm\n");
        fprintf(stderr, "Run: darktable-cli src/test/raws/sony.ARW src/test/raws/phase1.xmp "
                       "/tmp/out.png --core --disable-opencl --dump-pipe colorout --dumpdir /tmp/dtdump\n");
        return 1;
    }
    skip_pfm_header(f_in);

    float* lab_raw = (float*)malloc(npixels * 3 * sizeof(float));
    fread(lab_raw, sizeof(float), npixels * 3, f_in);
    fclose(f_in);

    /* Convert to 4-channel with row reversal (PFM stores rows reversed) */
    float* lab = (float*)malloc(npixels * 4 * sizeof(float));
    for (int row = 0; row < height; row++) {
        int src_row = height - 1 - row;
        for (int col = 0; col < width; col++) {
            size_t src_idx = (size_t)src_row * width + col;
            size_t dst_idx = (size_t)row * width + col;
            lab[dst_idx*4+0] = lab_raw[src_idx*3+0];
            lab[dst_idx*4+1] = lab_raw[src_idx*3+1];
            lab[dst_idx*4+2] = lab_raw[src_idx*3+2];
            lab[dst_idx*4+3] = 0.0f;
        }
    }
    free(lab_raw);

    /* Load reference output from DT dump */
    printf("[2/4] Loading reference output from DT dump...\n");
    FILE* f_ref = fopen("/tmp/dtdump/export/0002_colorout_cpu_out_C.pfm", "rb");
    if (!f_ref) {
        fprintf(stderr, "ERROR: Cannot open reference\n");
        free(lab);
        return 1;
    }
    skip_pfm_header(f_ref);

    float* ref_raw = (float*)malloc(npixels * 3 * sizeof(float));
    fread(ref_raw, sizeof(float), npixels * 3, f_ref);
    fclose(f_ref);

    float* ref = (float*)malloc(npixels * 4 * sizeof(float));
    for (int row = 0; row < height; row++) {
        int src_row = height - 1 - row;
        for (int col = 0; col < width; col++) {
            size_t src_idx = (size_t)src_row * width + col;
            size_t dst_idx = (size_t)row * width + col;
            ref[dst_idx*4+0] = ref_raw[src_idx*3+0];
            ref[dst_idx*4+1] = ref_raw[src_idx*3+1];
            ref[dst_idx*4+2] = ref_raw[src_idx*3+2];
            ref[dst_idx*4+3] = 0.0f;
        }
    }
    free(ref_raw);

    /* Run colorout */
    printf("[3/4] Running colorout (Lab -> sRGB)...\n");

    PipeState state;
    memset(&state, 0, sizeof(state));
    state.width = width;
    state.height = height;

    float cmatrix[4][4] = {
        { 3.13423491f, -1.61725771f, -0.4906919f, 0.0f },
        { -0.97874099f, 1.91611922f, 0.0334379375f, 0.0f },
        { 0.0719688162f, -0.229020134f, 1.40577972f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    float* output = (float*)malloc(npixels * 4 * sizeof(float));
    colorout_process(lab, output, &state, cmatrix);

    /* Compare and create diff */
    printf("[4/4] Comparing output vs reference...\n");

    int mismatches = 0;
    float max_diff = 0.0f;

    uint8_t* out_rgb = (uint8_t*)malloc(npixels * 3);
    uint8_t* ref_rgb = (uint8_t*)malloc(npixels * 3);
    uint8_t* diff_rgb = (uint8_t*)malloc(npixels * 3);

    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float out_v = output[i * 4 + c];
            float ref_v = ref[i * 4 + c];
            float diff = fabsf(out_v - ref_v);

            if (diff > max_diff) max_diff = diff;
            if (diff > 1e-3f) mismatches++;

            /* Clamp to [0,1] */
            out_v = out_v < 0.0f ? 0.0f : (out_v > 1.0f ? 1.0f : out_v);
            ref_v = ref_v < 0.0f ? 0.0f : (ref_v > 1.0f ? 1.0f : ref_v);

            out_rgb[i * 3 + c] = (uint8_t)(out_v * 255.0f + 0.5f);
            ref_rgb[i * 3 + c] = (uint8_t)(ref_v * 255.0f + 0.5f);

            /* Diff image: amplify differences 100x, show in red */
            float diff_amp = fminf(diff * 100.0f, 1.0f);
            if (c == 0)
                diff_rgb[i * 3 + c] = (uint8_t)(diff_amp * 255.0f);
            else
                diff_rgb[i * 3 + c] = (uint8_t)((1.0f - diff_amp) * out_v * 255.0f);
        }
    }

    printf("\nResults:\n");
    printf("  Mismatches (>1e-3): %d / %zu\n", mismatches, npixels * 3);
    printf("  Max diff: %.9f\n", max_diff);
    printf("  Status: %s\n", mismatches == 0 ? "PASS" : "FAIL");

    /* Write output images */
    printf("\nWriting output images...\n");
    write_png("tmp/var/phase1_output.png", out_rgb, width, height);
    write_png("tmp/var/phase1_ref.png", ref_rgb, width, height);
    write_png("tmp/var/phase1_diff.png", diff_rgb, width, height);

    printf("  tmp/var/phase1_output.png - Our output\n");
    printf("  tmp/var/phase1_ref.png    - DT reference\n");
    printf("  tmp/var/phase1_diff.png   - Difference (amplified 100x, red=diff)\n");

    /* Cleanup */
    free(lab);
    free(ref);
    free(output);
    free(out_rgb);
    free(ref_rgb);
    free(diff_rgb);

    return mismatches == 0 ? 0 : 1;
}
