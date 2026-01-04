/*
 * gold.cpp - Golden pipeline test (full scene-referred)
 *
 * Pipeline: sony.c decoder -> rawprepare -> temperature -> highlights -> demosaic ->
 *           exposure -> colorin -> channelmixerrgb -> colorbalancergb -> filmicrgb ->
 *           bilat -> colorout -> PNG
 *
 * Uses sony.c decoder (COPIED from DT/RawSpeed) for exact pixel match.
 * PipeState is initialized from DT dump values (COPY approach).
 *
 * Usage: gold <input.ARW> <output.png>
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>

/* Camera database - from DT adobe_coeff.c */
extern "C" {
#include "../../main/labs/cameras.c"
}

/* Sony ARW decoder - COPIED from DT/RawSpeed */
extern "C" {
#include "../../main/labs/sony.c"
}

/* PipeState and pipe_prepare */
extern "C" {
#include "../../main/labs/pipe_state.h"
#include "../../main/labs/pipe_prepare.c"
}

/* All modules (types inlined into each module) */
extern "C" {
#include "../../main/labs/mods/rawprepare.c"
#include "../../main/labs/mods/temperature.c"
#include "../../main/labs/mods/highlights.c"
#include "../../main/labs/mods/demosaic.c"
#include "../../main/labs/mods/exposure.c"
#include "../../main/labs/mods/colorin.c"
#include "../../main/labs/mods/channelmixerrgb.c"
#include "../../main/labs/mods/filmicrgb.c"
#include "../../main/labs/mods/colorbalancergb.c"
#include "../../main/labs/mods/bilat.c"
#include "../../main/labs/mods/colorout.c"
#include "../../main/labs/mods/autotune.c"
}

/* STB image write */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../main/labs/stb_image_write.h"

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
       1. Extract metadata from raw file
       ====================================================================== */
    SonyARWMeta meta;
    if (sony_arw_read_meta(input_path, &meta) != 0) {
        fprintf(stderr, "Cannot read metadata from: %s\n", input_path);
        return 1;
    }

    const int width = meta.width;
    const int height = meta.height;
    const size_t npixels = (size_t)width * height;

    printf("Dimensions: %dx%d\n", width, height);
    printf("Filters: 0x%08x\n", meta.filters);
    printf("Strip offset: %d\n", meta.strip_offset);
    printf("Sony curve: %d %d %d %d\n",
           meta.sony_curve[0], meta.sony_curve[1], meta.sony_curve[2], meta.sony_curve[3]);
    printf("Black: %d, White: %d\n", meta.black_level, meta.white_level);
    printf("WB RGGB: %.4f %.4f %.4f %.4f\n",
           meta.wb_rggb[0], meta.wb_rggb[1], meta.wb_rggb[2], meta.wb_rggb[3]);
    printf("Exposure bias: %.1f EV\n", meta.exposure_bias);
    printf("D65coeffs: %.4f %.4f %.4f (from cameras.xml)\n",
           meta.d65_coeffs[0], meta.d65_coeffs[1], meta.d65_coeffs[2]);

    /* ======================================================================
       2. Setup PipeState from extracted metadata
       ====================================================================== */
    PipeState state;
    memset(&state, 0, sizeof(state));

    state.width = width;
    state.height = height;
    state.filters = meta.filters;

    /* WB from raw file */
    state.chroma.as_shot[0] = meta.wb_rggb[0];
    state.chroma.as_shot[1] = meta.wb_rggb[1];
    state.chroma.as_shot[2] = meta.wb_rggb[2];
    state.chroma.as_shot[3] = meta.wb_rggb[3];
    state.chroma.late_correction = 1;

    /* D65coeffs computed from cameras.xml matrix */
    state.chroma.D65coeffs[0] = meta.d65_coeffs[0];
    state.chroma.D65coeffs[1] = meta.d65_coeffs[1];
    state.chroma.D65coeffs[2] = meta.d65_coeffs[2];
    state.chroma.D65coeffs[3] = meta.d65_coeffs[3];

    /* Exposure bias from camera style */
    state.exposure_bias = meta.exposure_bias;

    /* ======================================================================
       3. Decode RAW with sony.c (COPIED from DT/RawSpeed)
       ====================================================================== */

    /* Read compressed data from file */
    FILE* f = fopen(input_path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", input_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, meta.strip_offset, SEEK_SET);

    int compressed_size = file_size - meta.strip_offset;
    uint8_t* compressed = (uint8_t*)malloc(compressed_size);
    if (!compressed) {
        fprintf(stderr, "Cannot allocate compressed buffer\n");
        fclose(f);
        return 1;
    }
    fread(compressed, 1, compressed_size, f);
    fclose(f);

    /* Decode to uint16 Bayer */
    uint16_t* bayer_u16 = (uint16_t*)malloc(npixels * sizeof(uint16_t));
    if (!bayer_u16) {
        fprintf(stderr, "Cannot allocate bayer buffer\n");
        free(compressed);
        return 1;
    }

    printf("Decoding with sony.c...\n");
    if (sony_arw2_decode(compressed, compressed_size, width, height, meta.sony_curve, bayer_u16) != 0) {
        fprintf(stderr, "Decode failed\n");
        free(compressed);
        free(bayer_u16);
        return 1;
    }
    free(compressed);

    /* ======================================================================
       SENSOR STAGE
       ====================================================================== */

    /* 4. rawprepare */
    printf("1. rawprepare...\n");
    RawprepareParams rp_params;
    rawprepare_reset(&rp_params, 0, 0, 0, 0,
                     meta.black_level, meta.black_level, meta.black_level, meta.black_level,
                     meta.white_level);
    RawprepareData rp_data;
    rawprepare_commit_params(&rp_params, &rp_data);

    float* bayer_f32 = (float*)malloc(npixels * sizeof(float));
    rawprepare_process(bayer_u16, bayer_f32, width, height, width, height, &rp_data);

    /* 5. temperature - use as-shot WB */
    printf("2. temperature...\n");
    printf("   WB: R=%.4f G=%.4f B=%.4f\n",
           meta.wb_rggb[0], meta.wb_rggb[1], meta.wb_rggb[2]);

    TemperatureData temp_data;
    temp_data.coeffs[0] = (float)state.chroma.as_shot[0];
    temp_data.coeffs[1] = (float)state.chroma.as_shot[1];
    temp_data.coeffs[2] = (float)state.chroma.as_shot[2];
    temp_data.coeffs[3] = (float)state.chroma.as_shot[1];
    temp_data.preset = 4;

    float* bayer_wb = (float*)malloc(npixels * sizeof(float));
    temperature_process(bayer_f32, bayer_wb, &state, &temp_data);
    free(bayer_f32);

    /* 6. highlights - OPPOSED mode */
    printf("3. highlights...\n");
    HighlightsParams hl_params;
    highlights_reset(&hl_params, DT_IOP_HIGHLIGHTS_OPPOSED, 1.0f, 1.0f);
    HighlightsData hl_data = hl_params;

    /* Set temperature coefficients in state for highlights */
    state.temperature.enabled = 1;
    state.temperature.coeffs[0] = temp_data.coeffs[0];
    state.temperature.coeffs[1] = temp_data.coeffs[1];
    state.temperature.coeffs[2] = temp_data.coeffs[2];
    state.temperature.coeffs[3] = temp_data.coeffs[3];

    float* bayer_hl = (float*)malloc(npixels * sizeof(float));
    highlights_process(bayer_wb, bayer_hl, &state, &hl_data);
    free(bayer_wb);

    /* ======================================================================
       CAMERA STAGE
       ====================================================================== */

    /* 7. demosaic */
    printf("4. demosaic...\n");
    DemosaicParams dm_params;
    memset(&dm_params, 0, sizeof(dm_params));
    dm_params.demosaicing_method = 0;  /* PPG - simpler, more robust */
    dm_params.dual_thrs = 0.2f;
    dm_params.cs_thrs = 0.40f;
    dm_params.cs_iter = 8;

    float* rgb = (float*)malloc(npixels * 4 * sizeof(float));
    demosaic_process(bayer_hl, rgb, &state, &dm_params);
    free(bayer_hl);

    /* 8. exposure */
    printf("5. exposure...\n");
    ExposureParams exp_params;
    exp_params.mode = 0;
    exp_params.black = 0.0f;
    /* Use camera style exposure from database */
    exp_params.exposure = meta.camera ? meta.camera->style.exposure_ev : 0.0f;
    printf("   Exposure: %.2f EV (from camera style)\n", exp_params.exposure);
    exp_params.deflicker_percentile = 50.0f;
    exp_params.deflicker_target_level = -4.0f;
    exp_params.compensate_exposure_bias = 0;

    float* rgb_exp = (float*)malloc(npixels * 4 * sizeof(float));
    exposure_process(rgb, rgb_exp, width, height, 4, &exp_params);
    free(rgb);

    /* ======================================================================
       SCENE STAGE (Rec2020 RGB)
       ====================================================================== */

    /* 9. colorin: Camera RGB -> Rec2020 */
    printf("6. colorin...\n");

    /* cam_to_xyz from DT dump (PQTR_CHANNELMIXER RGB_to_XYZ) */
    const float cam_to_xyz[3][3] = {
        { 0.673474789f, 0.165675461f, 0.125049725f },
        { 0.279040545f, 0.675347328f, 0.045612101f },
        { -0.001932710f, 0.029981442f, 0.796851277f }
    };

    printf("   cam_to_xyz[0]: %.4f %.4f %.4f (sum=%.4f)\n",
           cam_to_xyz[0][0], cam_to_xyz[0][1], cam_to_xyz[0][2],
           cam_to_xyz[0][0] + cam_to_xyz[0][1] + cam_to_xyz[0][2]);

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

    /* ======================================================================
       10. channelmixerrgb: Chromatic adaptation (D65 illuminant)
       ====================================================================== */
    /* Matrices for Rec2020 pipeline (4x4 format) - used by colorbalancergb */
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

    printf("7. channelmixerrgb...\n");

    /* Use exact values from DT debug output (COPY, not compute) */
    ChannelMixerRGBData cm_data;
    cm_data.adaptation = DT_ADAPTATION_CAT16;  /* from DT: adaptation=1 */

    /* Illuminant in CAT16 LMS - computed by DT from camera WB */
    cm_data.illuminant[0] = 1.003973126f;
    cm_data.illuminant[1] = 0.993787944f;
    cm_data.illuminant[2] = 0.741390944f;
    cm_data.illuminant[3] = 0.0f;

    /* Identity MIX matrix */
    memset(cm_data.MIX, 0, sizeof(cm_data.MIX));
    cm_data.MIX[0][0] = 1.0f;
    cm_data.MIX[1][1] = 1.0f;
    cm_data.MIX[2][2] = 1.0f;

    /* Neutral saturation/lightness */
    for (int i = 0; i < 4; i++) {
        cm_data.saturation[i] = 0.0f;
        cm_data.lightness[i] = 0.0f;
        cm_data.grey[i] = 0.0f;
    }

    cm_data.p = 1.008250713f;  /* from DT */
    cm_data.gamut = 1.0f;
    cm_data.clip = 1;
    cm_data.apply_grey = 0;
    cm_data.version = CHANNELMIXERRGB_V_3;

    /* Dump channelmixer input for comparison with DT */
    {
        FILE* pfm = fopen("/tmp/gold_channelmixer_in.pfm", "wb");
        if (pfm) {
            fprintf(pfm, "PF\n%d %d\n-1.0\n", width, height);
            for (int y = height - 1; y >= 0; y--) {  /* PFM is bottom-up */
                for (int x = 0; x < width; x++) {
                    size_t i = (size_t)y * width + x;
                    fwrite(&rec2020[i*4], sizeof(float), 3, pfm);
                }
            }
            fclose(pfm);
            printf("   Wrote /tmp/gold_channelmixer_in.pfm\n");
        }
    }

    float* rec2020_cm = (float*)malloc(npixels * 4 * sizeof(float));
    channelmixerrgb_process(rec2020, rec2020_cm, width, height,
                            rec2020_to_xyz_4x4, xyz_to_rec2020_4x4, &cm_data);
    free(rec2020);

    /* ======================================================================
       11. colorbalancergb: Color grading (order 41.5 - before filmicrgb!)
       ====================================================================== */
    printf("8. colorbalancergb...\n");

    ColorBalanceRGBData cb_data;
    colorbalancergb_reset(&cb_data);

    /* Apply Picture Profile saturation/vibrance from RAW metadata */
    if (meta.profile.saturation != 0.0f || meta.profile.vibrance != 0.0f) {
        cb_data.saturation_global = meta.profile.saturation;
        cb_data.vibrance = meta.profile.vibrance;
        printf("   Picture Profile: saturation=%.2f vibrance=%.2f\n",
               meta.profile.saturation, meta.profile.vibrance);
    }

    /* Apply DRO (Dynamic Range Optimizer) shadow lift */
    if (meta.dro_shadow_lift != 1.0f) {
        cb_data.shadows[0] = meta.dro_shadow_lift;
        cb_data.shadows[1] = meta.dro_shadow_lift;
        cb_data.shadows[2] = meta.dro_shadow_lift;
        /* No highlight compensation - DRO only lifts shadows */
        printf("   DRO: shadow_lift=%.2f\n", meta.dro_shadow_lift);
    }

    /* DT pre-multiplied matrices from phase2.xmp dump */
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

    /* ======================================================================
       12. filmicrgb: Tone mapping (HDR -> SDR) (order 46)
       ====================================================================== */
    printf("9. filmicrgb...\n");

    FilmicRGBData filmic_data;
    filmicrgb_reset(&filmic_data);

    /* Scene-adaptive autotune: analyze image to set black/white EV points */
    filmicrgb_autotune(&filmic_data, rec2020_cb, width, height);

    float* rec2020_filmic = (float*)malloc(npixels * 4 * sizeof(float));
    filmicrgb_process(rec2020_cb, rec2020_filmic, width, height, &filmic_data,
                      FILMIC_INPUT_MATRIX_TRANS, FILMIC_OUTPUT_MATRIX,
                      FILMIC_OUTPUT_MATRIX_TRANS, FILMIC_EXPORT_INPUT_MATRIX_TRANS,
                      FILMIC_EXPORT_OUTPUT_MATRIX, FILMIC_EXPORT_OUTPUT_MATRIX_TRANS,
                      0.0f, 1.0f, 1 /* use_output_profile for sRGB export */);
    free(rec2020_cb);

    /* ======================================================================
       13. bilat: Local contrast (order 54)
       ====================================================================== */
    printf("10. bilat...\n");

    BilatData bilat_data;
    bilat_data.mode = 1;           /* local_laplacian */
    bilat_data.sigma_r = 0.5f;     /* highlights - from phase2.xmp */
    bilat_data.sigma_s = 0.5f;     /* shadows */
    bilat_data.detail = 0.1f;      /* clarity */
    bilat_data.midtone = 0.5f;     /* sigma */

    float* rec2020_bilat = (float*)malloc(npixels * 4 * sizeof(float));
    bilat_process(rec2020_filmic, rec2020_bilat, width, height, &bilat_data);
    free(rec2020_filmic);

    /* ======================================================================
       14. colorout: Rec2020 -> sRGB with gamma
       ====================================================================== */
    printf("11. colorout...\n");

    float* srgb = (float*)malloc(npixels * 4 * sizeof(float));

    for (size_t i = 0; i < npixels; i++) {
        float* in = rec2020_bilat + i * 4;
        float* out = srgb + i * 4;

        /* Rec2020 -> XYZ */
        float xyz[3];
        xyz[0] = REC2020_to_XYZ[0][0] * in[0] + REC2020_to_XYZ[0][1] * in[1] + REC2020_to_XYZ[0][2] * in[2];
        xyz[1] = REC2020_to_XYZ[1][0] * in[0] + REC2020_to_XYZ[1][1] * in[1] + REC2020_to_XYZ[1][2] * in[2];
        xyz[2] = REC2020_to_XYZ[2][0] * in[0] + REC2020_to_XYZ[2][1] * in[1] + REC2020_to_XYZ[2][2] * in[2];

        /* XYZ -> linear sRGB */
        float lin[3];
        lin[0] = XYZ_D65_to_sRGB[0][0] * xyz[0] + XYZ_D65_to_sRGB[0][1] * xyz[1] + XYZ_D65_to_sRGB[0][2] * xyz[2];
        lin[1] = XYZ_D65_to_sRGB[1][0] * xyz[0] + XYZ_D65_to_sRGB[1][1] * xyz[1] + XYZ_D65_to_sRGB[1][2] * xyz[2];
        lin[2] = XYZ_D65_to_sRGB[2][0] * xyz[0] + XYZ_D65_to_sRGB[2][1] * xyz[1] + XYZ_D65_to_sRGB[2][2] * xyz[2];

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
    free(rec2020_bilat);

    /* ======================================================================
       14. Write PNG
       ====================================================================== */
    printf("11. Writing PNG...\n");
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

    /* ======================================================================
       15. Auto-tune: compare to embedded JPEG, report optimal exposure
       ====================================================================== */
    printf("12. autotune...\n");
    float current_ev = meta.camera ? meta.camera->style.exposure_ev : 0.0f;
    float ev_adjustment = raw_exposure_autotune(input_path, png_data, width, height);
    float optimal_ev = current_ev + ev_adjustment;
    printf("   Current exposure: %.2f EV\n", current_ev);
    printf("   Optimal exposure: %.2f EV\n", optimal_ev);

    free(png_data);

    printf("\nDone: %s (%dx%d)\n", output_path, width, height);
    return 0;
}
