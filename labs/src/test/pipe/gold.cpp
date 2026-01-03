/*
 * gold.cpp - Reference pipeline test (links to pipe/ modules)
 *
 * This is a COPY of pipe/src/test/labs/gold.cpp with adjusted paths.
 * It produces the reference output that labs steps must match.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>

/* Sony ARW decoder - from pipe */
extern "C" {
#include "../../../../pipe/src/main/labs/sony.c"
}

/* PipeState and pipe_prepare */
extern "C" {
#include "../../../../pipe/src/main/labs/pipe_state.h"
#include "../../../../pipe/src/main/labs/pipe_prepare.c"
}

/* All modules (types inlined into each module) */
extern "C" {
#include "../../../../pipe/src/main/labs/mods/rawprepare.c"
#include "../../../../pipe/src/main/labs/mods/temperature.c"
#include "../../../../pipe/src/main/labs/mods/highlights.c"
#include "../../../../pipe/src/main/labs/mods/demosaic.c"
#include "../../../../pipe/src/main/labs/mods/exposure.c"
#include "../../../../pipe/src/main/labs/mods/colorin.c"
#include "../../../../pipe/src/main/labs/mods/channelmixerrgb.c"
#include "../../../../pipe/src/main/labs/mods/filmicrgb.c"
#include "../../../../pipe/src/main/labs/mods/colorbalancergb.c"
#include "../../../../pipe/src/main/labs/mods/bilat.c"
#include "../../../../pipe/src/main/labs/mods/colorout.c"
}

/* STB image write */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../pipe/src/main/labs/stb_image_write.h"

/* Dump helper */
static void dump_pfm(const char* path, const float* data, int width, int height, int ch) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "PF\n%d %d\n-1.0\n", width, height);
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            size_t i = (size_t)y * width + x;
            if (ch == 1) {
                float rgb[3] = { data[i], data[i], data[i] };
                fwrite(rgb, sizeof(float), 3, f);
            } else {
                fwrite(&data[i * ch], sizeof(float), 3, f);
            }
        }
    }
    fclose(f);
    printf("   Dumped: %s\n", path);
}

static void dump_bin(const char* path, const void* data, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fwrite(data, 1, size, f);
    fclose(f);
    printf("   Dumped: %s (%zu bytes)\n", path, size);
}

int main(int argc, char** argv)
{
    const char* input_path = "src/test/raws/sony.ARW";
    const char* output_dir = "tmp/var/pipe";

    if (argc >= 2) input_path = argv[1];
    if (argc >= 3) output_dir = argv[2];

    printf("=== Reference Pipeline (pipe/ modules) ===\n");
    printf("Input:  %s\n", input_path);
    printf("Output: %s/\n", output_dir);

    /* Create output directory */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", output_dir);
    system(cmd);

    /* 1. Extract metadata */
    SonyARWMeta meta;
    if (sony_arw_read_meta(input_path, &meta) != 0) {
        fprintf(stderr, "Cannot read metadata from: %s\n", input_path);
        return 1;
    }

    const int width = meta.width;
    const int height = meta.height;
    const size_t npixels = (size_t)width * height;

    printf("Dimensions: %dx%d\n", width, height);

    /* 2. Setup PipeState */
    PipeState state;
    memset(&state, 0, sizeof(state));
    state.width = width;
    state.height = height;
    state.filters = meta.filters;
    state.chroma.as_shot[0] = meta.wb_rggb[0];
    state.chroma.as_shot[1] = meta.wb_rggb[1];
    state.chroma.as_shot[2] = meta.wb_rggb[2];
    state.chroma.as_shot[3] = meta.wb_rggb[3];
    state.chroma.late_correction = 1;
    state.chroma.D65coeffs[0] = meta.d65_coeffs[0];
    state.chroma.D65coeffs[1] = meta.d65_coeffs[1];
    state.chroma.D65coeffs[2] = meta.d65_coeffs[2];
    state.chroma.D65coeffs[3] = meta.d65_coeffs[3];
    state.exposure_bias = meta.exposure_bias;

    /* 3. Decode RAW */
    FILE* f = fopen(input_path, "rb");
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, meta.strip_offset, SEEK_SET);
    int compressed_size = file_size - meta.strip_offset;
    uint8_t* compressed = (uint8_t*)malloc(compressed_size);
    fread(compressed, 1, compressed_size, f);
    fclose(f);

    uint16_t* bayer_u16 = (uint16_t*)malloc(npixels * sizeof(uint16_t));
    sony_arw2_decode(compressed, compressed_size, width, height, meta.sony_curve, bayer_u16);
    free(compressed);

    char path[512];

    /* 4. rawprepare */
    printf("1. rawprepare\n");
    RawprepareParams rp_params;
    rawprepare_reset(&rp_params, 0, 0, 0, 0,
                     meta.black_level, meta.black_level, meta.black_level, meta.black_level,
                     meta.white_level);
    RawprepareData rp_data;
    rawprepare_commit_params(&rp_params, &rp_data);

    float* bayer_f32 = (float*)malloc(npixels * sizeof(float));
    rawprepare_process(bayer_u16, bayer_f32, width, height, width, height, &rp_data);
    free(bayer_u16);

    snprintf(path, sizeof(path), "%s/01_rawprepare.bin", output_dir);
    dump_bin(path, bayer_f32, npixels * sizeof(float));

    /* 5. temperature */
    printf("2. temperature\n");
    TemperatureData temp_data;
    temp_data.coeffs[0] = (float)state.chroma.as_shot[0];
    temp_data.coeffs[1] = (float)state.chroma.as_shot[1];
    temp_data.coeffs[2] = (float)state.chroma.as_shot[2];
    temp_data.coeffs[3] = (float)state.chroma.as_shot[1];
    temp_data.preset = 4;

    float* bayer_wb = (float*)malloc(npixels * sizeof(float));
    temperature_process(bayer_f32, bayer_wb, &state, &temp_data);
    free(bayer_f32);

    snprintf(path, sizeof(path), "%s/02_temperature.bin", output_dir);
    dump_bin(path, bayer_wb, npixels * sizeof(float));

    /* 6. highlights */
    printf("3. highlights\n");
    HighlightsParams hl_params;
    highlights_reset(&hl_params, DT_IOP_HIGHLIGHTS_OPPOSED, 1.0f, 1.0f);
    HighlightsData hl_data = hl_params;

    state.temperature.enabled = 1;
    state.temperature.coeffs[0] = temp_data.coeffs[0];
    state.temperature.coeffs[1] = temp_data.coeffs[1];
    state.temperature.coeffs[2] = temp_data.coeffs[2];
    state.temperature.coeffs[3] = temp_data.coeffs[3];

    float* bayer_hl = (float*)malloc(npixels * sizeof(float));
    highlights_process(bayer_wb, bayer_hl, &state, &hl_data);
    free(bayer_wb);

    snprintf(path, sizeof(path), "%s/03_highlights.bin", output_dir);
    dump_bin(path, bayer_hl, npixels * sizeof(float));

    /* 7. demosaic */
    printf("4. demosaic\n");
    DemosaicParams dm_params;
    memset(&dm_params, 0, sizeof(dm_params));
    dm_params.demosaicing_method = 0;
    dm_params.dual_thrs = 0.2f;
    dm_params.cs_thrs = 0.40f;
    dm_params.cs_iter = 8;

    float* rgb = (float*)malloc(npixels * 4 * sizeof(float));
    demosaic_process(bayer_hl, rgb, &state, &dm_params);
    free(bayer_hl);

    snprintf(path, sizeof(path), "%s/04_demosaic.bin", output_dir);
    dump_bin(path, rgb, npixels * 4 * sizeof(float));

    /* 8. exposure */
    printf("5. exposure\n");
    ExposureParams exp_params;
    exp_params.mode = 0;
    exp_params.black = 0.0f;
    exp_params.exposure = state.exposure_bias;
    exp_params.deflicker_percentile = 50.0f;
    exp_params.deflicker_target_level = -4.0f;
    exp_params.compensate_exposure_bias = 0;

    float* rgb_exp = (float*)malloc(npixels * 4 * sizeof(float));
    exposure_process(rgb, rgb_exp, width, height, 4, &exp_params);
    free(rgb);

    snprintf(path, sizeof(path), "%s/05_exposure.bin", output_dir);
    dump_bin(path, rgb_exp, npixels * 4 * sizeof(float));

    /* 9. colorin */
    printf("6. colorin\n");
    const float cam_to_xyz[3][3] = {
        { 0.673474789f, 0.165675461f, 0.125049725f },
        { 0.279040545f, 0.675347328f, 0.045612101f },
        { -0.001932710f, 0.029981442f, 0.796851277f }
    };

    float* rec2020 = (float*)malloc(npixels * 4 * sizeof(float));
    for (size_t i = 0; i < npixels; i++) {
        float* in = rgb_exp + i * 4;
        float* out = rec2020 + i * 4;
        float xyz[3];
        xyz[0] = cam_to_xyz[0][0] * in[0] + cam_to_xyz[0][1] * in[1] + cam_to_xyz[0][2] * in[2];
        xyz[1] = cam_to_xyz[1][0] * in[0] + cam_to_xyz[1][1] * in[1] + cam_to_xyz[1][2] * in[2];
        xyz[2] = cam_to_xyz[2][0] * in[0] + cam_to_xyz[2][1] * in[1] + cam_to_xyz[2][2] * in[2];
        out[0] = XYZ_to_REC2020[0][0] * xyz[0] + XYZ_to_REC2020[0][1] * xyz[1] + XYZ_to_REC2020[0][2] * xyz[2];
        out[1] = XYZ_to_REC2020[1][0] * xyz[0] + XYZ_to_REC2020[1][1] * xyz[1] + XYZ_to_REC2020[1][2] * xyz[2];
        out[2] = XYZ_to_REC2020[2][0] * xyz[0] + XYZ_to_REC2020[2][1] * xyz[1] + XYZ_to_REC2020[2][2] * xyz[2];
        out[3] = 0.0f;
    }
    free(rgb_exp);

    snprintf(path, sizeof(path), "%s/06_colorin.bin", output_dir);
    dump_bin(path, rec2020, npixels * 4 * sizeof(float));

    /* 10. channelmixerrgb */
    printf("7. channelmixerrgb\n");
    dt_colormatrix_t rec2020_to_xyz_4x4 = {
        { REC2020_to_XYZ[0][0], REC2020_to_XYZ[0][1], REC2020_to_XYZ[0][2], 0.0f },
        { REC2020_to_XYZ[1][0], REC2020_to_XYZ[1][1], REC2020_to_XYZ[1][2], 0.0f },
        { REC2020_to_XYZ[2][0], REC2020_to_XYZ[2][1], REC2020_to_XYZ[2][2], 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };
    dt_colormatrix_t xyz_to_rec2020_4x4 = {
        { XYZ_to_REC2020[0][0], XYZ_to_REC2020[0][1], XYZ_to_REC2020[0][2], 0.0f },
        { XYZ_to_REC2020[1][0], XYZ_to_REC2020[1][1], XYZ_to_REC2020[1][2], 0.0f },
        { XYZ_to_REC2020[2][0], XYZ_to_REC2020[2][1], XYZ_to_REC2020[2][2], 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    ChannelMixerRGBData cm_data;
    cm_data.adaptation = DT_ADAPTATION_CAT16;
    cm_data.illuminant[0] = 1.003973126f;
    cm_data.illuminant[1] = 0.993787944f;
    cm_data.illuminant[2] = 0.741390944f;
    cm_data.illuminant[3] = 0.0f;
    memset(cm_data.MIX, 0, sizeof(cm_data.MIX));
    cm_data.MIX[0][0] = 1.0f;
    cm_data.MIX[1][1] = 1.0f;
    cm_data.MIX[2][2] = 1.0f;
    for (int i = 0; i < 4; i++) {
        cm_data.saturation[i] = 0.0f;
        cm_data.lightness[i] = 0.0f;
        cm_data.grey[i] = 0.0f;
    }
    cm_data.p = 1.008250713f;
    cm_data.gamut = 1.0f;
    cm_data.clip = 1;
    cm_data.apply_grey = 0;
    cm_data.version = CHANNELMIXERRGB_V_3;

    float* rec2020_cm = (float*)malloc(npixels * 4 * sizeof(float));
    channelmixerrgb_process(rec2020, rec2020_cm, width, height,
                            rec2020_to_xyz_4x4, xyz_to_rec2020_4x4, &cm_data);
    free(rec2020);

    snprintf(path, sizeof(path), "%s/07_channelmixer.bin", output_dir);
    dump_bin(path, rec2020_cm, npixels * 4 * sizeof(float));

    /* 11. colorbalancergb */
    printf("8. colorbalancergb\n");
    ColorBalanceRGBData cb_data;
    colorbalancergb_reset(&cb_data);

    dt_colormatrix_t cb_input_matrix = {
        { 0.406808585f, 0.617819786f, 0.045817737f, 0.0f },
        { 0.067756824f, 0.748962402f, 0.100109622f, 0.0f },
        { 0.022140553f, -0.015321352f, 0.587274075f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };
    dt_colormatrix_t cb_output_matrix = {
        { 1.662934422f, -0.321330518f, -0.237917423f, 0.0f },
        { -0.681079328f, 1.609099507f, 0.035052136f, 0.0f },
        { 0.029973516f, -0.075743161f, 0.961853564f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    float* rec2020_cb = (float*)malloc(npixels * 4 * sizeof(float));
    colorbalancergb_process(rec2020_cm, rec2020_cb, width, height,
                            cb_input_matrix, cb_output_matrix, &cb_data);
    free(rec2020_cm);

    snprintf(path, sizeof(path), "%s/08_colorbalance.bin", output_dir);
    dump_bin(path, rec2020_cb, npixels * 4 * sizeof(float));

    /* 12. filmicrgb */
    printf("9. filmicrgb\n");
    FilmicRGBData filmic_data;
    filmicrgb_reset(&filmic_data);

    float* rec2020_filmic = (float*)malloc(npixels * 4 * sizeof(float));
    filmicrgb_process(rec2020_cb, rec2020_filmic, width, height, &filmic_data,
                      FILMIC_INPUT_MATRIX_TRANS, FILMIC_OUTPUT_MATRIX,
                      FILMIC_OUTPUT_MATRIX_TRANS, FILMIC_EXPORT_INPUT_MATRIX_TRANS,
                      FILMIC_EXPORT_OUTPUT_MATRIX, FILMIC_EXPORT_OUTPUT_MATRIX_TRANS,
                      0.0f, 1.0f, 1);
    free(rec2020_cb);

    snprintf(path, sizeof(path), "%s/09_filmic.bin", output_dir);
    dump_bin(path, rec2020_filmic, npixels * 4 * sizeof(float));

    /* 13. bilat */
    printf("10. bilat\n");
    BilatData bilat_data;
    bilat_data.mode = 1;
    bilat_data.sigma_r = 0.5f;
    bilat_data.sigma_s = 0.5f;
    bilat_data.detail = 0.1f;
    bilat_data.midtone = 0.5f;

    float* rec2020_bilat = (float*)malloc(npixels * 4 * sizeof(float));
    bilat_process(rec2020_filmic, rec2020_bilat, width, height, &bilat_data);
    free(rec2020_filmic);

    snprintf(path, sizeof(path), "%s/10_bilat.bin", output_dir);
    dump_bin(path, rec2020_bilat, npixels * 4 * sizeof(float));

    /* 14. colorout */
    printf("11. colorout\n");
    float* srgb = (float*)malloc(npixels * 4 * sizeof(float));
    for (size_t i = 0; i < npixels; i++) {
        float* in = rec2020_bilat + i * 4;
        float* out = srgb + i * 4;
        float xyz[3];
        xyz[0] = REC2020_to_XYZ[0][0] * in[0] + REC2020_to_XYZ[0][1] * in[1] + REC2020_to_XYZ[0][2] * in[2];
        xyz[1] = REC2020_to_XYZ[1][0] * in[0] + REC2020_to_XYZ[1][1] * in[1] + REC2020_to_XYZ[1][2] * in[2];
        xyz[2] = REC2020_to_XYZ[2][0] * in[0] + REC2020_to_XYZ[2][1] * in[1] + REC2020_to_XYZ[2][2] * in[2];
        float lin[3];
        lin[0] = XYZ_D65_to_sRGB[0][0] * xyz[0] + XYZ_D65_to_sRGB[0][1] * xyz[1] + XYZ_D65_to_sRGB[0][2] * xyz[2];
        lin[1] = XYZ_D65_to_sRGB[1][0] * xyz[0] + XYZ_D65_to_sRGB[1][1] * xyz[1] + XYZ_D65_to_sRGB[1][2] * xyz[2];
        lin[2] = XYZ_D65_to_sRGB[2][0] * xyz[0] + XYZ_D65_to_sRGB[2][1] * xyz[1] + XYZ_D65_to_sRGB[2][2] * xyz[2];
        for (int c = 0; c < 3; c++) {
            float v = lin[c];
            if (v < 0.0f) v = 0.0f;
            if (v <= 0.0031308f)
                out[c] = 12.92f * v;
            else
                out[c] = 1.055f * powf(v, 1.0f/2.4f) - 0.055f;
        }
        out[3] = 0.0f;
    }
    free(rec2020_bilat);

    snprintf(path, sizeof(path), "%s/11_colorout.bin", output_dir);
    dump_bin(path, srgb, npixels * 4 * sizeof(float));

    /* Write PNG */
    printf("12. Writing PNG\n");
    uint8_t* png_data = (uint8_t*)malloc(npixels * 3);
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = srgb[i * 4 + c];
            v = (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
            png_data[i * 3 + c] = (uint8_t)(v * 255.0f + 0.5f);
        }
    }
    free(srgb);

    snprintf(path, sizeof(path), "%s/gold.png", output_dir);
    stbi_write_png(path, width, height, 3, png_data, width * 3);
    free(png_data);

    printf("\nDone: %s/gold.png\n", output_dir);
    return 0;
}
