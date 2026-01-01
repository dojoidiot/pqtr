/*
 * exposure_test.cpp - Test exposure module against DT dump
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

/* Exposure params (modversion 6) */
typedef struct {
    int mode;
    float black;
    float exposure;
    float deflicker_percentile;
    float deflicker_target_level;
    int compensate_exposure_bias;
} ExposureParams;

#define exposure2white(x) exp2f(-(x))

/* Process - copied from DT exposure.c process() */
static void exposure_process(const float *in, float *out,
                             int width, int height, int ch,
                             const ExposureParams *p)
{
    const float black = p->black;
    const float white = exposure2white(p->exposure);
    const float scale = 1.0f / (white - black);
    const size_t npixels = (size_t)width * height;

    for (size_t k = 0; k < (size_t)ch * npixels; k++)
    {
        out[k] = (in[k] - black) * scale;
    }
}

/* PFM loader */
static bool load_pfm(const char *path, std::vector<float> &data, int &width, int &height)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    char type[3] = {0};
    if (fscanf(f, "%2s", type) != 1 || strcmp(type, "PF") != 0) {
        fclose(f);
        return false;
    }
    fgetc(f);

    if (fscanf(f, "%d %d", &width, &height) != 2) {
        fclose(f);
        return false;
    }
    fgetc(f);

    float scale;
    if (fscanf(f, "%f", &scale) != 1) {
        fclose(f);
        return false;
    }
    fgetc(f);

    size_t npixels = (size_t)width * height;
    data.resize(npixels * 3);
    size_t read = fread(data.data(), sizeof(float), npixels * 3, f);
    fclose(f);

    return read == npixels * 3;
}

int main()
{
    const char *in_path = "/tmp/dtdump/export/0001_exposure_cpu_in_C.pfm";
    const char *ref_path = "/tmp/dtdump/export/0002_exposure_cpu_out_C.pfm";

    std::vector<float> input;
    int width, height;
    if (!load_pfm(in_path, input, width, height)) {
        fprintf(stderr, "Failed to load: %s\n", in_path);
        return 1;
    }
    printf("Input: %dx%d\n", width, height);

    std::vector<float> ref;
    int ref_w, ref_h;
    if (!load_pfm(ref_path, ref, ref_w, ref_h)) {
        fprintf(stderr, "Failed to load: %s\n", ref_path);
        return 1;
    }

    /* Params from phase2.xmp */
    ExposureParams params;
    params.mode = 0;
    params.black = -0.000244140625f;
    params.exposure = 0.7999997735023499f;
    params.deflicker_percentile = 50.0f;
    params.deflicker_target_level = -4.0f;
    params.compensate_exposure_bias = 0;

    std::vector<float> output(input.size());
    exposure_process(input.data(), output.data(), width, height, 3, &params);

    /* Compare */
    const float tolerance = 1e-5f;
    int mismatches = 0;
    float max_diff = 0.0f;

    for (size_t i = 0; i < output.size(); i++) {
        float diff = fabsf(output[i] - ref[i]);
        if (diff > max_diff) max_diff = diff;
        if (diff > tolerance) {
            if (mismatches < 5) {
                printf("Mismatch [%zu]: got %f, expected %f (diff %e)\n",
                       i, output[i], ref[i], diff);
            }
            mismatches++;
        }
    }

    printf("\nTotal: %zu, Mismatches: %d, Max diff: %e\n",
           output.size(), mismatches, max_diff);

    if (mismatches == 0) {
        printf("PASS\n");
        return 0;
    }
    return 1;
}
