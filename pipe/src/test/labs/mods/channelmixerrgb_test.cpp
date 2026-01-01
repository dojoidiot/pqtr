/*
 * channelmixerrgb test - compare our output to DT's dumped PFM
 *
 * Runtime data from phase3.xmp:
 *   adaptation=1 (LINEAR_BRADFORD)
 *   illuminant=[1.004, 0.994, 0.741, 0]
 *   p=1.008, gamut=1.0, clip=1
 *   MIX=identity
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>

/* Include the module */
extern "C" {
#include "../../../main/labs/mods/channelmixerrgb.c"
}

/* PFM reader */
static float* read_pfm(const char* path, int* width, int* height, int* channels)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return nullptr;
    }

    char magic[3] = {0};
    if (fscanf(f, "%2s\n", magic) != 1) {
        fclose(f);
        return nullptr;
    }

    if (strcmp(magic, "PF") == 0) *channels = 3;
    else if (strcmp(magic, "Pf") == 0) *channels = 1;
    else {
        fprintf(stderr, "Invalid PFM magic: %s\n", magic);
        fclose(f);
        return nullptr;
    }

    if (fscanf(f, "%d %d\n", width, height) != 2) {
        fclose(f);
        return nullptr;
    }

    float scale;
    if (fscanf(f, "%f\n", &scale) != 1) {
        fclose(f);
        return nullptr;
    }

    size_t npixels = (size_t)(*width) * (*height) * (*channels);
    float* data = (float*)malloc(npixels * sizeof(float));
    if (!data) {
        fclose(f);
        return nullptr;
    }

    if (fread(data, sizeof(float), npixels, f) != npixels) {
        free(data);
        fclose(f);
        return nullptr;
    }

    fclose(f);
    return data;
}

/* Expand RGB to RGBA with row flip (PFM is bottom-up) */
static float* pfm_to_rgba(const float* pfm, int width, int height)
{
    size_t npixels = (size_t)width * height;
    float* rgba = (float*)malloc(npixels * 4 * sizeof(float));
    for (int row = 0; row < height; row++) {
        int src_row = height - 1 - row;
        for (int x = 0; x < width; x++) {
            size_t src_idx = (size_t)src_row * width + x;
            size_t dst_idx = (size_t)row * width + x;
            rgba[dst_idx*4 + 0] = pfm[src_idx*3 + 0];
            rgba[dst_idx*4 + 1] = pfm[src_idx*3 + 1];
            rgba[dst_idx*4 + 2] = pfm[src_idx*3 + 2];
            rgba[dst_idx*4 + 3] = 0.0f;
        }
    }
    return rgba;
}

int main()
{
    /* Load DT's input and output PFMs */
    int in_w, in_h, in_c;
    float* in_pfm = read_pfm("/tmp/dtdump/export/0000_channelmixer_cpu_in_C.pfm", &in_w, &in_h, &in_c);
    if (!in_pfm) {
        fprintf(stderr, "Failed to load input PFM\n");
        return 1;
    }
    printf("Loaded input: %dx%d, %d channels\n", in_w, in_h, in_c);

    int out_w, out_h, out_c;
    float* out_pfm = read_pfm("/tmp/dtdump/export/0001_channelmixer_cpu_out_C.pfm", &out_w, &out_h, &out_c);
    if (!out_pfm) {
        fprintf(stderr, "Failed to load output PFM\n");
        free(in_pfm);
        return 1;
    }
    printf("Loaded output: %dx%d, %d channels\n", out_w, out_h, out_c);

    if (in_w != out_w || in_h != out_h) {
        fprintf(stderr, "Dimension mismatch\n");
        free(in_pfm);
        free(out_pfm);
        return 1;
    }

    /* Convert to RGBA */
    float* in_rgba = pfm_to_rgba(in_pfm, in_w, in_h);
    float* dt_rgba = pfm_to_rgba(out_pfm, out_w, out_h);
    free(in_pfm);
    free(out_pfm);

    /* Allocate our output */
    size_t npixels = (size_t)in_w * in_h;
    float* my_rgba = (float*)malloc(npixels * 4 * sizeof(float));

    /* Initialize module data - from DT dump */
    ChannelMixerRGBData data;
    channelmixerrgb_reset(&data);

    /* Override with dumped values - adaptation=1 from dump means CAT16 */
    data.adaptation = DT_ADAPTATION_CAT16;
    data.illuminant[0] = 1.003973126f;
    data.illuminant[1] = 0.993787944f;
    data.illuminant[2] = 0.741390944f;
    data.illuminant[3] = 0.0f;
    data.p = 1.008250713f;  /* from dump */
    data.gamut = 1.0f;
    data.clip = 1;
    data.apply_grey = 0;
    data.version = CHANNELMIXERRGB_V_3;

    /* Identity MIX matrix (from dump) */
    memset(data.MIX, 0, sizeof(data.MIX));
    data.MIX[0][0] = 1.0f;
    data.MIX[1][1] = 1.0f;
    data.MIX[2][2] = 1.0f;

    /* saturation and lightness = 0 (from dump) */
    for(int i = 0; i < 4; i++) {
        data.saturation[i] = 0.0f;
        data.lightness[i] = 0.0f;
        data.grey[i] = 0.0f;
    }

    /* RGB_to_XYZ and XYZ_to_RGB matrices from DT's work profile dump */
    const dt_colormatrix_t RGB_to_XYZ = {
        { 0.673474789f, 0.165675461f, 0.125049725f, 0.f },
        { 0.279040545f, 0.675347328f, 0.045612101f, 0.f },
        { -0.001932710f, 0.029981442f, 0.796851277f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };
    const dt_colormatrix_t XYZ_to_RGB = {
        { 1.647250295f, -0.393625855f, -0.235971376f, 0.f },
        { -0.682616651f, 1.647609591f, 0.012813044f, 0.f },
        { 0.029678674f, -0.062945843f, 1.253884912f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    /* Debug: trace first few pixels */
    printf("Input pixel 0: %.9f %.9f %.9f\n", in_rgba[0], in_rgba[1], in_rgba[2]);
    printf("Input pixel 1: %.9f %.9f %.9f\n", in_rgba[4], in_rgba[5], in_rgba[6]);
    printf("DT output pixel 0: %.9f %.9f %.9f\n", dt_rgba[0], dt_rgba[1], dt_rgba[2]);
    printf("DT output pixel 1: %.9f %.9f %.9f\n", dt_rgba[4], dt_rgba[5], dt_rgba[6]);

    /* Process */
    printf("Processing %zu pixels...\n", npixels);
    channelmixerrgb_process(in_rgba, my_rgba, in_w, in_h, RGB_to_XYZ, XYZ_to_RGB, &data);

    printf("My output pixel 0: %.9f %.9f %.9f\n", my_rgba[0], my_rgba[1], my_rgba[2]);
    printf("My output pixel 1: %.9f %.9f %.9f\n", my_rgba[4], my_rgba[5], my_rgba[6]);

    /* Compare */
    const float tolerance = 1e-3f;
    int mismatches = 0;
    float max_diff = 0.0f;
    int max_diff_k = 0;

    for (size_t k = 0; k < npixels * 4; k += 4) {
        for (int c = 0; c < 3; c++) {
            float diff = fabsf(my_rgba[k+c] - dt_rgba[k+c]);
            if (diff > max_diff) {
                max_diff = diff;
                max_diff_k = k;
            }
            if (diff > tolerance) {
                if (mismatches < 5) {
                    printf("Mismatch at k=%zu c=%d: mine=%.6f dt=%.6f diff=%.6f\n",
                           k, c, my_rgba[k+c], dt_rgba[k+c], diff);
                }
                mismatches++;
            }
        }
    }

    printf("\nTotal RGB: %zu, Mismatches: %d, Max diff: %e\n",
           npixels * 3, mismatches, max_diff);
    if (max_diff > 0) {
        printf("Max diff at k=%d: mine=[%.6f,%.6f,%.6f] dt=[%.6f,%.6f,%.6f]\n",
               max_diff_k,
               my_rgba[max_diff_k], my_rgba[max_diff_k+1], my_rgba[max_diff_k+2],
               dt_rgba[max_diff_k], dt_rgba[max_diff_k+1], dt_rgba[max_diff_k+2]);
    }

    free(in_rgba);
    free(dt_rgba);
    free(my_rgba);

    if (mismatches == 0) {
        printf("PASS\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}
