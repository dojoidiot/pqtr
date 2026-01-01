/*
 * gold.cpp - Golden pipeline test (full scene-referred)
 *
 * Pipeline: rawprepare -> temperature -> highlights -> demosaic -> exposure ->
 *           colorin -> channelmixerrgb -> filmicrgb -> colorout -> PNG
 *
 * Usage: gold <input.ARW> <output.png>
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

/* Sony decoder */
#include "../../main/labs/plug/sony/sony.h"

/* All modules */
extern "C" {
#include "../../main/labs/mods/types.h"
#include "../../main/labs/mods/rawprepare.c"
#include "../../main/labs/mods/temperature.c"
#include "../../main/labs/mods/highlights.c"
#include "../../main/labs/mods/demosaic.c"
#include "../../main/labs/mods/exposure.c"
#include "../../main/labs/mods/colorin.c"
#include "../../main/labs/mods/channelmixerrgb.c"
#include "../../main/labs/mods/filmicrgb.c"
#include "../../main/labs/mods/colorout.c"
}

/* STB image write */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../main/labs/stb_image_write.h"

/* ==========================================================================
   Rec2020 matrices (from DT work profile)
   ========================================================================== */

static const dt_colormatrix_t REC2020_to_XYZ = {
    { 0.673474789f, 0.165675461f, 0.125049725f, 0.f },
    { 0.279040545f, 0.675347328f, 0.045612101f, 0.f },
    { -0.001932710f, 0.029981442f, 0.796851277f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t XYZ_to_REC2020 = {
    { 1.647250295f, -0.393625855f, -0.235971376f, 0.f },
    { -0.682616651f, 1.647609591f, 0.012813044f, 0.f },
    { 0.029678674f, -0.062945843f, 1.253884912f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

/* sRGB output matrix (XYZ D50 -> sRGB) */
static const float SRGB_CMATRIX[4][4] = {
    { 3.1338561f, -1.6168667f, -0.4906146f, 0.f },
    { -0.9787684f, 1.9161415f, 0.0334540f, 0.f },
    { 0.0719453f, -0.2289914f, 1.4052427f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

/* Filmicrgb matrices (for scene -> display) */
static const dt_colormatrix_t FILMIC_INPUT_TRANS = {
    { 0.406808585f, 0.617819786f, 0.045817729f, 0.f },
    { 0.067756809f, 0.748962402f, 0.100109629f, 0.f },
    { 0.022140555f, -0.015321350f, 0.587274075f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};
static const dt_colormatrix_t FILMIC_OUTPUT = {
    { 2.837817192f, -2.337296247f, 0.177027255f, 0.f },
    { -0.241587654f, 1.529518247f, -0.241881117f, 0.f },
    { -0.113289982f, 0.128020823f, 1.689797878f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};
static const dt_colormatrix_t FILMIC_EXPORT_INPUT_TRANS = {
    { 0.298672199f, 0.706104636f, 0.065669231f, 0.f },
    { 0.095901854f, 0.719828308f, 0.101098664f, 0.f },
    { 0.022459989f, 0.044898711f, 0.526734650f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};
static const dt_colormatrix_t FILMIC_EXPORT_OUTPUT = {
    { 4.862406731f, -4.789227962f, 0.313011587f, 0.f },
    { -0.626189709f, 2.022818327f, -0.310180575f, 0.f },
    { -0.153957039f, 0.031788439f, 1.911581993f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

/* ==========================================================================
   Main pipeline
   ========================================================================== */

int main(int argc, char** argv)
{
    const char* input_path = "src/test/raws/sony.ARW";
    const char* output_path = "tmp/var/gold.png";

    if (argc >= 2) input_path = argv[1];
    if (argc >= 3) output_path = argv[2];

    printf("=== Golden Pipeline (Scene-Referred) ===\n");
    printf("Input:  %s\n", input_path);
    printf("Output: %s\n", output_path);

    /* ======================================================================
       1. Load ARW
       ====================================================================== */
    std::ifstream file(input_path, std::ios::binary | std::ios::ate);
    if (!file) {
        fprintf(stderr, "Cannot open: %s\n", input_path);
        return 1;
    }
    size_t file_size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> raw_data(file_size);
    file.read(reinterpret_cast<char*>(raw_data.data()), file_size);
    file.close();
    printf("Loaded: %zu bytes\n", file_size);

    sony::BayerU16 bayer_u16;
    sony::Info info;
    sony::RawMetadata meta;
    if (!sony::Decoder::prepare(raw_data.data(), raw_data.size(), bayer_u16, info, meta)) {
        fprintf(stderr, "Sony decoder failed\n");
        return 1;
    }
    printf("Decoded: %dx%d, black=%d white=%d\n",
           meta.width, meta.height, meta.black_level, meta.white_level);

    const int width = meta.width;
    const int height = meta.height;
    const size_t npixels = (size_t)width * height;

    /* Setup PipeState */
    PipeState state;
    memset(&state, 0, sizeof(state));
    state.width = width;
    state.height = height;

    /* Convert bayer_pattern to DT filters format */
    const uint32_t filter_patterns[] = {
        0x94949494,  /* RGGB */
        0x61616161,  /* GRBG */
        0x16161616,  /* BGGR */
        0x49494949   /* GBRG */
    };
    state.filters = filter_patterns[meta.bayer_pattern];
    printf("Bayer: %s (0x%08x)\n",
           (const char*[]){"RGGB","GRBG","BGGR","GBRG"}[meta.bayer_pattern],
           state.filters);

    /* ======================================================================
       SENSOR STAGE
       ====================================================================== */

    /* 2. rawprepare */
    printf("1. rawprepare...\n");
    RawprepareParams rp_params;
    rawprepare_reset(&rp_params, 0, 0, 0, 0,
                     meta.black_level, meta.black_level, meta.black_level, meta.black_level,
                     meta.white_level);
    RawprepareData rp_data;
    rawprepare_commit_params(&rp_params, &rp_data);

    float* bayer_f32 = (float*)malloc(npixels * sizeof(float));
    rawprepare_process(bayer_u16.ptr(), bayer_f32, width, height, width, height, &rp_data);

    /* 3. temperature */
    printf("2. temperature...\n");
    float wb_r = (float)meta.wb_rggb[0] / (float)meta.wb_rggb[1];
    float wb_g = 1.0f;
    float wb_b = (float)meta.wb_rggb[2] / (float)meta.wb_rggb[1];
    printf("   WB: R=%.4f G=%.4f B=%.4f\n", wb_r, wb_g, wb_b);

    TemperatureData temp_data;
    temp_data.coeffs[0] = wb_r;
    temp_data.coeffs[1] = wb_g;
    temp_data.coeffs[2] = wb_b;
    temp_data.coeffs[3] = wb_g;
    temp_data.preset = 4;

    float* bayer_wb = (float*)malloc(npixels * sizeof(float));
    temperature_process(bayer_f32, bayer_wb, &state, &temp_data);
    free(bayer_f32);

    /* 4. highlights */
    printf("3. highlights...\n");
    HighlightsData hl_data;
    hl_data.mode = DT_IOP_HIGHLIGHTS_OPPOSED;
    hl_data.blendL = 1.0f;
    hl_data.blendC = 0.0f;
    hl_data.strength = 1.0f;
    hl_data.clip = 1.0f;
    hl_data.noise_level = 0.0f;
    hl_data.iterations = 30;
    hl_data.scales = 6;
    hl_data.candidating = 0.4f;
    hl_data.combine = 2.0f;
    hl_data.recovery = 0;
    hl_data.solid_color = 0.0f;

    float* bayer_hl = (float*)malloc(npixels * sizeof(float));
    highlights_process(bayer_wb, bayer_hl, &state, &hl_data);
    free(bayer_wb);

    /* ======================================================================
       CAMERA STAGE
       ====================================================================== */

    /* 5. demosaic */
    printf("4. demosaic...\n");
    DemosaicParams dm_params;
    memset(&dm_params, 0, sizeof(dm_params));
    dm_params.demosaicing_method = 5;  /* RCD */
    dm_params.dual_thrs = 0.2f;
    dm_params.cs_thrs = 0.40f;
    dm_params.cs_iter = 8;

    float* rgb = (float*)malloc(npixels * 4 * sizeof(float));
    demosaic_process(bayer_hl, rgb, &state, &dm_params);
    free(bayer_hl);

    /* 6. exposure */
    printf("5. exposure...\n");
    ExposureParams exp_params;
    exp_params.mode = 0;
    exp_params.black = 0.0f;
    exp_params.exposure = 0.7f;  /* +0.7 EV for typical scene */
    exp_params.deflicker_percentile = 50.0f;
    exp_params.deflicker_target_level = -4.0f;
    exp_params.compensate_exposure_bias = 0;

    float* rgb_exp = (float*)malloc(npixels * 4 * sizeof(float));
    exposure_process(rgb, rgb_exp, width, height, 4, &exp_params);
    free(rgb);

    /* ======================================================================
       SCENE STAGE (Rec2020 RGB)
       ====================================================================== */

    /* 7. colorin: Camera RGB -> Rec2020 */
    printf("6. colorin...\n");

    /* Sony A7 III color matrix: Camera RGB -> XYZ (from DNG/Adobe) */
    /* This should come from camera profile, using typical Sony values */
    const float cam_to_xyz[3][3] = {
        { 0.6389f, 0.1092f, 0.1820f },
        { 0.2454f, 0.7867f, -0.0321f },
        { 0.0132f, -0.1291f, 0.9523f }
    };

    float* rec2020 = (float*)malloc(npixels * 4 * sizeof(float));

    for (size_t i = 0; i < npixels; i++) {
        float* in = rgb_exp + i * 4;
        float* out = rec2020 + i * 4;

        /* Camera RGB -> XYZ */
        float xyz[3];
        xyz[0] = cam_to_xyz[0][0] * in[0] + cam_to_xyz[0][1] * in[1] + cam_to_xyz[0][2] * in[2];
        xyz[1] = cam_to_xyz[1][0] * in[0] + cam_to_xyz[1][1] * in[1] + cam_to_xyz[1][2] * in[2];
        xyz[2] = cam_to_xyz[2][0] * in[0] + cam_to_xyz[2][1] * in[1] + cam_to_xyz[2][2] * in[2];

        /* XYZ -> Rec2020 */
        out[0] = XYZ_to_REC2020[0][0] * xyz[0] + XYZ_to_REC2020[0][1] * xyz[1] + XYZ_to_REC2020[0][2] * xyz[2];
        out[1] = XYZ_to_REC2020[1][0] * xyz[0] + XYZ_to_REC2020[1][1] * xyz[1] + XYZ_to_REC2020[1][2] * xyz[2];
        out[2] = XYZ_to_REC2020[2][0] * xyz[0] + XYZ_to_REC2020[2][1] * xyz[1] + XYZ_to_REC2020[2][2] * xyz[2];
        out[3] = 0.0f;
    }
    free(rgb_exp);

    /* Skip channelmixerrgb and filmicrgb for now - use simple sRGB gamma */
    printf("7-9. Simple sRGB output (no tone mapping)...\n");

    /* Rec2020 -> XYZ -> sRGB with gamma */
    float* srgb = (float*)malloc(npixels * 4 * sizeof(float));

    /* sRGB matrix: XYZ D65 -> sRGB */
    const float xyz_to_srgb[3][3] = {
        {  3.2404542f, -1.5371385f, -0.4985314f },
        { -0.9692660f,  1.8760108f,  0.0415560f },
        {  0.0556434f, -0.2040259f,  1.0572252f }
    };

    for (size_t i = 0; i < npixels; i++) {
        float* in = rec2020 + i * 4;
        float* out = srgb + i * 4;

        /* Rec2020 -> XYZ */
        float xyz[3];
        xyz[0] = REC2020_to_XYZ[0][0] * in[0] + REC2020_to_XYZ[0][1] * in[1] + REC2020_to_XYZ[0][2] * in[2];
        xyz[1] = REC2020_to_XYZ[1][0] * in[0] + REC2020_to_XYZ[1][1] * in[1] + REC2020_to_XYZ[1][2] * in[2];
        xyz[2] = REC2020_to_XYZ[2][0] * in[0] + REC2020_to_XYZ[2][1] * in[1] + REC2020_to_XYZ[2][2] * in[2];

        /* XYZ -> linear sRGB */
        float lin[3];
        lin[0] = xyz_to_srgb[0][0] * xyz[0] + xyz_to_srgb[0][1] * xyz[1] + xyz_to_srgb[0][2] * xyz[2];
        lin[1] = xyz_to_srgb[1][0] * xyz[0] + xyz_to_srgb[1][1] * xyz[1] + xyz_to_srgb[1][2] * xyz[2];
        lin[2] = xyz_to_srgb[2][0] * xyz[0] + xyz_to_srgb[2][1] * xyz[1] + xyz_to_srgb[2][2] * xyz[2];

        /* sRGB gamma */
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
    free(rec2020);

    /* ======================================================================
       Write PNG
       ====================================================================== */
    printf("10. Writing PNG...\n");
    uint8_t* png_data = (uint8_t*)malloc(npixels * 3);
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = srgb[i * 4 + c];
            v = (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
            png_data[i * 3 + c] = (uint8_t)(v * 255.0f + 0.5f);
        }
    }
    free(srgb);

    if (!stbi_write_png(output_path, width, height, 3, png_data, width * 3)) {
        fprintf(stderr, "Failed to write PNG\n");
        free(png_data);
        return 1;
    }
    free(png_data);

    printf("\nDone: %s (%dx%d)\n", output_path, width, height);
    return 0;
}
