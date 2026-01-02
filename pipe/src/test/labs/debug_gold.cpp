#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

#include "../../main/labs/plug/sony/sony.h"

extern "C" {
#include "../../main/labs/mods/rawprepare.c"
#include "../../main/labs/mods/temperature.c"
#include "../../main/labs/mods/highlights.c"
#include "../../main/labs/mods/demosaic.c"
#include "../../main/labs/mods/exposure.c"
#include "../../main/labs/mods/colorin.c"
}

int main() {
    const char* input_path = "src/test/raws/sony.ARW";
    
    std::ifstream file(input_path, std::ios::binary | std::ios::ate);
    size_t file_size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> raw_data(file_size);
    file.read(reinterpret_cast<char*>(raw_data.data()), file_size);
    file.close();

    sony::BayerU16 bayer_u16;
    sony::Info info;
    sony::RawMetadata meta;
    sony::Decoder::prepare(raw_data.data(), raw_data.size(), bayer_u16, info, meta);

    const int width = meta.width;
    const int height = meta.height;
    const size_t npixels = (size_t)width * height;

    PipeState state;
    memset(&state, 0, sizeof(state));
    state.width = width;
    state.height = height;
    const uint32_t filter_patterns[] = {0x94949494, 0x61616161, 0x16161616, 0x49494949};
    state.filters = filter_patterns[meta.bayer_pattern];

    RawprepareParams rp_params;
    rawprepare_reset(&rp_params, 0, 0, 0, 0,
                     meta.black_level, meta.black_level, meta.black_level, meta.black_level,
                     meta.white_level);
    RawprepareData rp_data;
    rawprepare_commit_params(&rp_params, &rp_data);
    float* bayer_f32 = (float*)malloc(npixels * sizeof(float));
    rawprepare_process(bayer_u16.ptr(), bayer_f32, width, height, width, height, &rp_data);

    float wb_r = (float)meta.wb_rggb[0] / (float)meta.wb_rggb[1];
    float wb_b = (float)meta.wb_rggb[2] / (float)meta.wb_rggb[1];
    TemperatureData temp_data;
    temp_data.coeffs[0] = wb_r;
    temp_data.coeffs[1] = 1.0f;
    temp_data.coeffs[2] = wb_b;
    temp_data.coeffs[3] = 1.0f;
    temp_data.preset = 4;
    float* bayer_wb = (float*)malloc(npixels * sizeof(float));
    temperature_process(bayer_f32, bayer_wb, &state, &temp_data);
    free(bayer_f32);

    HighlightsData hl_data;
    hl_data.mode = DT_IOP_HIGHLIGHTS_OPPOSED;
    hl_data.blendL = 1.0f; hl_data.blendC = 0.0f; hl_data.strength = 1.0f;
    hl_data.clip = 1.0f; hl_data.noise_level = 0.0f; hl_data.iterations = 30;
    hl_data.scales = 6; hl_data.candidating = 0.4f; hl_data.combine = 2.0f;
    hl_data.recovery = 0; hl_data.solid_color = 0.0f;
    float* bayer_hl = (float*)malloc(npixels * sizeof(float));
    highlights_process(bayer_wb, bayer_hl, &state, &hl_data);
    free(bayer_wb);

    DemosaicParams dm_params;
    memset(&dm_params, 0, sizeof(dm_params));
    dm_params.demosaicing_method = 5;
    dm_params.dual_thrs = 0.2f;
    dm_params.cs_thrs = 0.40f;
    dm_params.cs_iter = 8;
    float* rgb = (float*)malloc(npixels * 4 * sizeof(float));
    demosaic_process(bayer_hl, rgb, &state, &dm_params);
    free(bayer_hl);

    ExposureParams exp_params;
    exp_params.mode = 0;
    exp_params.black = 0.0f;
    exp_params.exposure = 0.7f;
    exp_params.deflicker_percentile = 50.0f;
    exp_params.deflicker_target_level = -4.0f;
    exp_params.compensate_exposure_bias = 0;
    float* rgb_exp = (float*)malloc(npixels * 4 * sizeof(float));
    exposure_process(rgb, rgb_exp, width, height, 4, &exp_params);
    free(rgb);

    const float cam_to_xyz[3][3] = {
        { 0.6389f, 0.1092f, 0.1820f },
        { 0.2454f, 0.7867f, -0.0321f },
        { 0.0132f, -0.1291f, 0.9523f }
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

    double sum_r = 0, sum_g = 0, sum_b = 0;
    for (size_t i = 0; i < npixels; i++) {
        sum_r += rec2020[i*4+0];
        sum_g += rec2020[i*4+1];
        sum_b += rec2020[i*4+2];
    }
    printf("Our channelmixer INPUT (Rec2020):\n");
    printf("  Mean: %.4f %.4f %.4f\n", sum_r/npixels, sum_g/npixels, sum_b/npixels);
    
    size_t center = (height/2) * width + (width/2);
    printf("  Center: %.6f %.6f %.6f\n", rec2020[center*4], rec2020[center*4+1], rec2020[center*4+2]);
    
    free(rec2020);
    return 0;
}
