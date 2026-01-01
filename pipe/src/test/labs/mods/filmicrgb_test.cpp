/*
 * filmicrgb test - compare our output to DT's dumped PFM
 *
 * Runtime matrices dumped from DT for phase2.xmp
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>

/* Include the module */
extern "C" {
#include "../../../main/labs/mods/filmicrgb.c"
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

/* Expand RGB to RGBA */
static float* rgb_to_rgba(const float* rgb, int width, int height)
{
    size_t npixels = (size_t)width * height;
    float* rgba = (float*)malloc(npixels * 4 * sizeof(float));
    for (size_t i = 0; i < npixels; i++) {
        rgba[i*4 + 0] = rgb[i*3 + 0];
        rgba[i*4 + 1] = rgb[i*3 + 1];
        rgba[i*4 + 2] = rgb[i*3 + 2];
        rgba[i*4 + 3] = 0.0f;
    }
    return rgba;
}

int main()
{
    /* Load DT's input and output PFMs */
    int in_w, in_h, in_c;
    float* in_rgb = read_pfm("/tmp/dtdump/export/0000_filmicrgb_cpu_in_C.pfm", &in_w, &in_h, &in_c);
    if (!in_rgb) {
        fprintf(stderr, "Failed to load input PFM\n");
        return 1;
    }
    printf("Loaded input: %dx%d, %d channels\n", in_w, in_h, in_c);

    int out_w, out_h, out_c;
    float* out_rgb = read_pfm("/tmp/dtdump/export/0001_filmicrgb_cpu_out_C.pfm", &out_w, &out_h, &out_c);
    if (!out_rgb) {
        fprintf(stderr, "Failed to load output PFM\n");
        free(in_rgb);
        return 1;
    }
    printf("Loaded output: %dx%d, %d channels\n", out_w, out_h, out_c);

    if (in_w != out_w || in_h != out_h) {
        fprintf(stderr, "Dimension mismatch\n");
        free(in_rgb);
        free(out_rgb);
        return 1;
    }

    /* Convert to RGBA */
    float* in_rgba = rgb_to_rgba(in_rgb, in_w, in_h);
    float* dt_rgba = rgb_to_rgba(out_rgb, out_w, out_h);
    free(in_rgb);
    free(out_rgb);

    /* Allocate our output */
    size_t npixels = (size_t)in_w * in_h;
    float* my_rgba = (float*)malloc(npixels * 4 * sizeof(float));

    /* Initialize module data */
    FilmicRGBData data;
    filmicrgb_reset(&data);

    /* Matrices dumped from DT */
    const dt_colormatrix_t input_matrix_trans = {
        { 0.406808585f, 0.067756809f, 0.022140555f, 0.f },
        { 0.617819786f, 0.748962402f, -0.015321350f, 0.f },
        { 0.045817729f, 0.100109629f, 0.587274075f, 0.f }
    };
    const dt_colormatrix_t output_matrix = {
        { 2.837817192f, -2.337296247f, 0.177027255f, 0.f },
        { -0.241587654f, 1.529518247f, -0.241881117f, 0.f },
        { -0.113289982f, 0.128020823f, 1.689797878f, 0.f }
    };
    dt_colormatrix_t output_matrix_trans;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            output_matrix_trans[i][j] = (j < 3) ? output_matrix[j][i] : 0.f;

    const dt_colormatrix_t export_input_matrix_trans = {
        { 0.298672199f, 0.095901854f, 0.022459989f, 0.f },
        { 0.706104636f, 0.719828308f, 0.044898711f, 0.f },
        { 0.065669231f, 0.101098664f, 0.526734650f, 0.f }
    };
    const dt_colormatrix_t export_output_matrix = {
        { 4.862406731f, -4.789227962f, 0.313011587f, 0.f },
        { -0.626189709f, 2.022818327f, -0.310180575f, 0.f },
        { -0.153957039f, 0.031788439f, 1.911581993f, 0.f }
    };
    dt_colormatrix_t export_output_matrix_trans;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            export_output_matrix_trans[i][j] = (j < 3) ? export_output_matrix[j][i] : 0.f;

    /* Display black/white from DT */
    const float display_black = 0.000151763f;
    const float display_white = 1.000000000f;
    const int use_output_profile = 1;

    /* Debug: trace pixel 0 and 708 (k=2832) */
    printf("Input pixel 0: %.9f %.9f %.9f\n", in_rgba[0], in_rgba[1], in_rgba[2]);
    printf("DT output pixel 0: %.9f %.9f %.9f\n", dt_rgba[0], dt_rgba[1], dt_rgba[2]);
    printf("Input pixel 708 (k=2832): %.9f %.9f %.9f\n", in_rgba[2832], in_rgba[2833], in_rgba[2834]);
    printf("DT output pixel 708: %.9f %.9f %.9f\n", dt_rgba[2832], dt_rgba[2833], dt_rgba[2834]);

    /* Process */
    printf("Processing %zu pixels...\n", npixels);
    filmicrgb_process(in_rgba, my_rgba, in_w, in_h, &data,
                       input_matrix_trans, output_matrix, output_matrix_trans,
                       export_input_matrix_trans, export_output_matrix, export_output_matrix_trans,
                       display_black, display_white, use_output_profile);

    /* Compare */
    const float tolerance = 3e-2f;
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
