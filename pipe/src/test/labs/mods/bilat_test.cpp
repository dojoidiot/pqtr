/*
 * bilat test - compare our output to DT's dumped PFM
 *
 * Runtime parameters dumped from DT for phase2.xmp:
 *   mode=1 (local laplacian)
 *   sigma_r=0.5, sigma_s=0.5, detail=0.1, midtone=0.5
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>

/* Include the module */
extern "C" {
#include "../../../main/labs/mods/bilat.c"
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

/* Expand RGB to RGBA (note: bilat uses Lab so we keep all 4 channels) */
/* PFM rows are stored in reverse order - we flip them back */
static float* pfm_to_rgba(const float* pfm, int width, int height)
{
    size_t npixels = (size_t)width * height;
    float* rgba = (float*)malloc(npixels * 4 * sizeof(float));
    for (int row = 0; row < height; row++) {
        int src_row = height - 1 - row;  /* flip rows */
        for (int x = 0; x < width; x++) {
            size_t src_idx = (size_t)src_row * width + x;
            size_t dst_idx = (size_t)row * width + x;
            rgba[dst_idx*4 + 0] = pfm[src_idx*3 + 0];  /* L */
            rgba[dst_idx*4 + 1] = pfm[src_idx*3 + 1];  /* a */
            rgba[dst_idx*4 + 2] = pfm[src_idx*3 + 2];  /* b */
            rgba[dst_idx*4 + 3] = 0.0f;
        }
    }
    return rgba;
}

int main()
{
    /* Load DT's input and output PFMs */
    int in_w, in_h, in_c;
    float* in_pfm = read_pfm("/tmp/dtdump/export/0000_bilat_cpu_in_C.pfm", &in_w, &in_h, &in_c);
    if (!in_pfm) {
        fprintf(stderr, "Failed to load input PFM\n");
        return 1;
    }
    printf("Loaded input: %dx%d, %d channels\n", in_w, in_h, in_c);

    int out_w, out_h, out_c;
    float* out_pfm = read_pfm("/tmp/dtdump/export/0001_bilat_cpu_out_C.pfm", &out_w, &out_h, &out_c);
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
    BilatData data;
    data.mode = 1;       /* local laplacian */
    data.sigma_r = 0.5f; /* highlights */
    data.sigma_s = 0.5f; /* shadows */
    data.detail = 0.099999994f; /* clarity */
    data.midtone = 0.5f; /* sigma */

    /* Debug: trace pixel 0 */
    printf("Input pixel 0 (Lab): %.9f %.9f %.9f\n", in_rgba[0], in_rgba[1], in_rgba[2]);
    printf("DT output pixel 0: %.9f %.9f %.9f\n", dt_rgba[0], dt_rgba[1], dt_rgba[2]);

    /* Process */
    printf("Processing %zu pixels...\n", npixels);
    bilat_process(in_rgba, my_rgba, in_w, in_h, &data);

    printf("My output pixel 0: %.9f %.9f %.9f\n", my_rgba[0], my_rgba[1], my_rgba[2]);

    /* Compare - bilat only modifies L channel, a/b should be identical */
    const float tolerance = 3e-1f;
    int mismatches = 0;
    float max_diff = 0.0f;
    int max_diff_k = 0;

    for (size_t k = 0; k < npixels * 4; k += 4) {
        /* Only compare L channel - a/b are copied directly */
        float diff = fabsf(my_rgba[k] - dt_rgba[k]);
        if (diff > max_diff) {
            max_diff = diff;
            max_diff_k = k;
        }
        if (diff > tolerance) {
            if (mismatches < 5) {
                printf("Mismatch at k=%zu (L): mine=%.6f dt=%.6f diff=%.6f\n",
                       k, my_rgba[k], dt_rgba[k], diff);
            }
            mismatches++;
        }
    }

    printf("\nTotal L values: %zu, Mismatches: %d, Max diff: %e\n",
           npixels, mismatches, max_diff);
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
