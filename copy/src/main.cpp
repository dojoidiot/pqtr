#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

#include "core/types.hpp"
#include "core/image_buffer.hpp"
#include "core/pipe_state.hpp"
#include "core/metadata.hpp"
#include "core/cameras.hpp"
#include "io/sony_decoder.hpp"
#include "../inc/stb_image_write.h"

#include "modules/rawprepare.hpp"
#include "modules/temperature.hpp"
#include "modules/highlights.hpp"
#include "modules/demosaic.hpp"
#include "modules/exposure.hpp"
#include "modules/colorin.hpp"
#include "modules/channelmixerrgb.hpp"
#include "modules/denoiseprofile.hpp"
#include "modules/colorbalancergb.hpp"
#include "modules/filmicrgb.hpp"
#include "modules/bilat.hpp"
#include "modules/colorout.hpp"
#include "modules/autotune.hpp"
#include "debug_dump.hpp"

using namespace copy;

int main(int argc, char** argv) {
    std::string input_path = "../args/src/test/raws/DSC00458.ARW";
    std::string output_path = "output/out.png";

    if (argc >= 2) input_path = argv[1];
    if (argc >= 3) output_path = argv[2];

    std::cout << "=== Copy Pipeline ===\n";
    std::cout << "Input:  " << input_path << "\n";
    std::cout << "Output: " << output_path << "\n";

    // 1. Metadata
    core::MetaData meta;
    if (!io::SonyDecoder::read_meta(input_path, meta)) {
        std::cerr << "Cannot read metadata\n";
        return 1;
    }

    std::cout << "Dimensions: " << meta.width << "x" << meta.height << "\n";
    std::cout << "Filters: 0x" << std::hex << meta.filters << std::dec << "\n";
    std::cout << "Black: " << meta.black_level << ", White: " << meta.white_level << "\n";

    // 2. PipeState
    core::PipeState state;
    state.width = meta.width;
    state.height = meta.height;
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

    // 3. Decode
    core::ImageBuffer<core::u16> bayer_u16(meta.width, meta.height);
    std::cout << "Decoding...\n";
    if (!io::SonyDecoder::decode(input_path, meta, bayer_u16)) {
        std::cerr << "Decode failed\n";
        return 1;
    }
    debug::dump_buffer("00_decode", bayer_u16);

    // 4. rawprepare
    std::cout << "1. rawprepare...\n";
    modules::rawprepare::Params rp_p;
    rp_p.left = rp_p.top = rp_p.right = rp_p.bottom = 0;
    for(int i=0; i<4; i++) rp_p.raw_black_level_separate[i] = meta.black_level;
    rp_p.raw_white_point = meta.white_level;
    core::ImageBuffer<core::f32> bayer_f32(meta.width, meta.height);
    modules::rawprepare::process(bayer_u16, bayer_f32, rp_p);
    debug::dump_buffer("01_rawprepare", bayer_f32);

    // 5. temperature
    std::cout << "2. temperature...\n";
    modules::temperature::Params temp_p;
    temp_p.coeffs[0] = (float)state.chroma.as_shot[0];
    temp_p.coeffs[1] = (float)state.chroma.as_shot[1];
    temp_p.coeffs[2] = (float)state.chroma.as_shot[2];
    temp_p.coeffs[3] = (float)state.chroma.as_shot[1];
    core::ImageBuffer<core::f32> bayer_wb(meta.width, meta.height);
    modules::temperature::process(bayer_f32, bayer_wb, state, temp_p);
    debug::dump_buffer("02_temperature", bayer_wb);

    // 6. highlights
    std::cout << "3. highlights...\n";
    modules::highlights::Params hl_p;
    hl_p.clip = 1.0f;
    // state temperature coeffs set by temperature module
    core::ImageBuffer<core::f32> bayer_hl(meta.width, meta.height);
    modules::highlights::process(bayer_wb, bayer_hl, state, hl_p);
    debug::dump_buffer("03_highlights", bayer_hl);

    // 7. demosaic
    std::cout << "4. demosaic...\n";
    modules::demosaic::Params dm_p;
    dm_p.demosaicing_method = 5; // RCD
    core::ImageBuffer<core::f32> rgb(meta.width, meta.height, 4);
    modules::demosaic::process(bayer_hl, rgb, state, dm_p);
    debug::dump_buffer("04_demosaic", rgb);

    // 8. exposure
    std::cout << "5. exposure...\n";
    modules::exposure::Params exp_p;
    exp_p.exposure = meta.camera ? meta.camera->style.exposure_ev : 0.0f;
    std::cout << "   Exposure: " << exp_p.exposure << " EV\n";
    core::ImageBuffer<core::f32> rgb_exp(meta.width, meta.height, 4);
    modules::exposure::process(rgb, rgb_exp, exp_p);
    debug::dump_buffer("05_exposure", rgb_exp);

    // 9. colorin
    std::cout << "6. colorin...\n";
    // Hardcoded matrix from args/main.c
    static const float cam_to_xyz[3][3] = {
        { 0.673474789f, 0.165675461f, 0.125049725f },
        { 0.279040545f, 0.675347328f, 0.045612101f },
        { -0.001932710f, 0.029981442f, 0.796851277f }
    };
    core::ImageBuffer<core::f32> rec2020(meta.width, meta.height, 4);
    modules::colorin::process(rgb_exp, rec2020, cam_to_xyz);
    debug::dump_buffer("06_colorin", rec2020);

    // 10. channelmixerrgb
    std::cout << "7. channelmixerrgb...\n";
    modules::channelmixerrgb::Params cm_p;
    // Defaults matching channelmixerrgb_reset in args
    cm_p.adaptation = 1; // CAT16
    cm_p.illuminant[0] = 1.003973126f; cm_p.illuminant[1] = 0.993787944f; cm_p.illuminant[2] = 0.741390944f; cm_p.illuminant[3] = 0.0f;
    std::memset(cm_p.MIX, 0, sizeof(cm_p.MIX)); cm_p.MIX[0][0]=1; cm_p.MIX[1][1]=1; cm_p.MIX[2][2]=1;
    std::memset(cm_p.saturation, 0, sizeof(cm_p.saturation));
    std::memset(cm_p.lightness, 0, sizeof(cm_p.lightness));
    std::memset(cm_p.grey, 0, sizeof(cm_p.grey));
    cm_p.p = 1.008250713f;
    cm_p.gamut = 1.0f;
    cm_p.clip = 1;
    cm_p.apply_grey = 0;
    cm_p.version = 2; // V3

    // Need matrices
    // REC2020_to_XYZ (4x4)
    static const float REC2020_to_XYZ[4][4] = {
        { 0.673474789f, 0.165675461f, 0.125049725f, 0.f },
        { 0.279040545f, 0.675347328f, 0.045612101f, 0.f },
        { -0.001932710f, 0.029981442f, 0.796851277f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };
    static const float XYZ_to_REC2020[4][4] = {
        { 1.647250295f, -0.393625855f, -0.235971376f, 0.f },
        { -0.682616651f, 1.647609591f, 0.012813044f, 0.f },
        { 0.029678674f, -0.062945843f, 1.253884912f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    core::ImageBuffer<core::f32> rec2020_cm(meta.width, meta.height, 4);
    modules::channelmixerrgb::process(rec2020, rec2020_cm, REC2020_to_XYZ, XYZ_to_REC2020, cm_p);
    debug::dump_buffer("07_channelmixerrgb", rec2020_cm);

    // 11. denoiseprofile
    std::cout << "8. denoiseprofile...\n";
    modules::denoiseprofile::Params dn_p;
    dn_p.strength = 1.0f; dn_p.shadows = 1.0f; dn_p.max_scales = 5;
    modules::denoiseprofile::set_profile(dn_p, "SONY", "ILCE-7M3", meta.iso > 0 ? meta.iso : 100);
    float iso_factor = std::log((float)(meta.iso > 0 ? meta.iso : 100) / 100.0f) / std::log(2.0f);
    dn_p.strength = 0.5f + iso_factor * 0.15f;
    std::cout << "   Denoise strength: " << dn_p.strength << "\n";
    core::ImageBuffer<core::f32> rec2020_dn(meta.width, meta.height, 4);
    modules::denoiseprofile::process(rec2020_cm, rec2020_dn, dn_p);
    debug::dump_buffer("08_denoiseprofile", rec2020_dn);

    // 12. colorbalancergb
    std::cout << "9. colorbalancergb...\n";
    modules::colorbalancergb::Params cb_p;
    // Defaults
    std::memset(&cb_p, 0, sizeof(cb_p));
    cb_p.global[0] = -0.000000119f; cb_p.midtones[0] = 1.000000119f; 
    cb_p.midtones[1]=1; cb_p.midtones[2]=1; cb_p.midtones[3]=1;
    for(int i=0;i<4;i++) { cb_p.shadows[i]=1.0f; cb_p.highlights[i]=1.0f; }
    cb_p.shadows[0]=0.999999881f; cb_p.highlights[0]=0.999999881f;
    cb_p.midtones_Y=1.0f; cb_p.contrast=1.0f;
    cb_p.shadows_weight=4.0f; cb_p.highlights_weight=4.0f; cb_p.midtones_weight=8.0f; cb_p.mask_grey_fulcrum=0.5f;
    cb_p.white_fulcrum=1.0f; cb_p.grey_fulcrum=0.184499994f;
    for(int i=0;i<512;i++) cb_p.gamut_LUT[i]=1.0f;
    cb_p.saturation_formula=1;

    if (meta.profile.saturation != 0.0f || meta.profile.vibrance != 0.0f) {
        cb_p.saturation_global = meta.profile.saturation;
        cb_p.vibrance = meta.profile.vibrance;
    }
    if (meta.dro_shadow_lift != 1.0f) {
        cb_p.shadows[0] = cb_p.shadows[1] = cb_p.shadows[2] = meta.dro_shadow_lift;
    }

    static const float cb_input_matrix[4][4] = {
        { 0.406808585f, 0.617819786f, 0.045817737f, 0.0f },
        { 0.067756824f, 0.748962402f, 0.100109622f, 0.0f },
        { 0.022140553f, -0.015321352f, 0.587274075f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };
    static const float cb_output_matrix[4][4] = {
        { 1.662934422f, -0.321330518f, -0.237917423f, 0.0f },
        { -0.681079328f, 1.609099507f, 0.035052136f, 0.0f },
        { 0.029973516f, -0.075743161f, 0.961853564f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    core::ImageBuffer<core::f32> rec2020_cb(meta.width, meta.height, 4);
    modules::colorbalancergb::process(rec2020_dn, rec2020_cb, cb_input_matrix, cb_output_matrix, cb_p);
    debug::dump_buffer("09_colorbalancergb", rec2020_cb);

    // 13. filmicrgb
    std::cout << "10. filmicrgb...\n";
    modules::filmicrgb::Params filmic_p;
    // Defaults
    filmic_p.grey_source=0.1845f; filmic_p.black_source=-8.0f; filmic_p.white_source=4.0f;
    filmic_p.dynamic_range=12.0f; filmic_p.normalize=11.881188393f; filmic_p.output_power=4.0f;
    filmic_p.contrast=1.0f; filmic_p.saturation=0.0f; filmic_p.sigma_toe=0.05f; filmic_p.sigma_shoulder=0.05f;
    modules::filmicrgb::compute_spline(filmic_p);
    
    // Autotune
    modules::filmicrgb::autotune(filmic_p, rec2020_cb);

    // Matrices
    static const float FILMIC_INPUT_MATRIX_TRANS[4][4] = {
        { 0.406808585f, 0.067756809f, 0.022140555f, 0.f },
        { 0.617819786f, 0.748962402f, -0.015321350f, 0.f },
        { 0.045817729f, 0.100109629f, 0.587274075f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };
    static const float FILMIC_OUTPUT_MATRIX[4][4] = {
        { 2.837817192f, -2.337296247f, 0.177027255f, 0.f },
        { -0.241587654f, 1.529518247f, -0.241881117f, 0.f },
        { -0.113289982f, 0.128020823f, 1.689797878f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };
    static const float FILMIC_OUTPUT_MATRIX_TRANS[4][4] = {
        { 2.837817192f, -0.241587654f, -0.113289982f, 0.f },
        { -2.337296247f, 1.529518247f, 0.128020823f, 0.f },
        { 0.177027255f, -0.241881117f, 1.689797878f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };
    static const float FILMIC_EXPORT_INPUT_MATRIX_TRANS[4][4] = {
        { 0.298672199f, 0.095901854f, 0.022459989f, 0.f },
        { 0.706104636f, 0.719828308f, 0.044898711f, 0.f },
        { 0.065669231f, 0.101098664f, 0.526734650f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };
    static const float FILMIC_EXPORT_OUTPUT_MATRIX[4][4] = {
        { 4.862406731f, -4.789227962f, 0.313011587f, 0.f },
        { -0.626189709f, 2.022818327f, -0.310180575f, 0.f },
        { -0.153957039f, 0.031788439f, 1.911581993f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };
    static const float FILMIC_EXPORT_OUTPUT_MATRIX_TRANS[4][4] = {
        { 4.862406731f, -0.626189709f, -0.153957039f, 0.f },
        { -4.789227962f, 2.022818327f, 0.031788439f, 0.f },
        { 0.313011587f, -0.310180575f, 1.911581993f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    core::ImageBuffer<core::f32> rec2020_filmic(meta.width, meta.height, 4);
    modules::filmicrgb::process(rec2020_cb, rec2020_filmic, filmic_p, FILMIC_INPUT_MATRIX_TRANS, FILMIC_OUTPUT_MATRIX, FILMIC_OUTPUT_MATRIX_TRANS, FILMIC_EXPORT_INPUT_MATRIX_TRANS, FILMIC_EXPORT_OUTPUT_MATRIX, FILMIC_EXPORT_OUTPUT_MATRIX_TRANS, 0.0f, 1.0f, 1);
    debug::dump_buffer("10_filmicrgb", rec2020_filmic);

    // 14. bilat
    std::cout << "11. bilat...\n";
    modules::bilat::Params bilat_p;
    bilat_p.mode = 1; bilat_p.sigma_r = 0.5f; bilat_p.sigma_s = 0.5f; bilat_p.detail = 0.1f; bilat_p.midtone = 0.5f;
    core::ImageBuffer<core::f32> rec2020_bilat(meta.width, meta.height, 4);
    modules::bilat::process(rec2020_filmic, rec2020_bilat, bilat_p);
    debug::dump_buffer("11_bilat", rec2020_bilat);

    // 15. colorout
    std::cout << "12. colorout...\n";
    static const float XYZ_D65_to_sRGB[4][4] = {
        {  3.2404542f, -1.5371385f, -0.4985314f, 0.f },
        { -0.9692660f,  1.8760108f,  0.0415560f, 0.f },
        {  0.0556434f, -0.2040259f,  1.0572252f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };
    // Need Rec2020 -> sRGB.
    // Logic in args/main.c manually does Rec2020->XYZ then XYZ->sRGB inline.
    // But calls it 'colorout_process' implicitly? No, main.c does it inline.
    // My module 'colorout' accepts 'cmatrix'.
    // If I pass 'Rec2020 -> sRGB' matrix to colorout?
    // args/mods/colorout.c expects Lab input if using that module directly? 
    // No, I implemented colorout.cpp to take Lab input and do Lab->XYZ->RGB.
    // BUT main.c does NOT use colorout.c logic. It uses inline Rec2020->XYZ->sRGB.
    // So my colorout module is actually wrong for this pipeline if I want to mimic main.c!
    
    // I should rewrite colorout module or just do it inline here to match main.c exact logic.
    // Or write a new module 'rec2020_to_srgb'.
    // Since I want 'idiomatic' modules, I should probably adapt 'colorout' to be generic RGB->RGB if needed.
    // But 'colorout' in args implies Lab input.
    // I'll just do it inline here or create a small helper.
    // Inline is safer to match args main.c exactly.
    
    core::ImageBuffer<core::f32> srgb(meta.width, meta.height, 4);
    {
        // Rec2020 -> XYZ (D65) -> sRGB (D65)
        // Matrix Rec2020 -> XYZ is in channelmixerrgb
        // Matrix XYZ -> sRGB is XYZ_D65_to_sRGB
        size_t count = rec2020_bilat.count() / 4;
        const float* in = rec2020_bilat.data();
        float* out = srgb.data();

        // Constants from main.c
        static const float REC2020_to_XYZ_3x3[3][3] = {
            { 0.636958048f, 0.144616904f, 0.168880975f },
            { 0.262700213f, 0.677998072f, 0.059301716f },
            { 0.000000000f, 0.028072693f, 1.060985058f }
        };
        // Note: args/src/main/mods/colorin.c defines them slightly differently?
        // Let's check args/src/main/main.c lines 525+.
        /*
        xyz[0] = REC2020_to_XYZ[0][0] * in[0] + ...
        */
        // REC2020_to_XYZ in main.c comes from pipe_prepare? Or colorin?
        // It seems to be from 'mods/colorin.c' included in main.c? 
        // No, 'mods/colorin.c' has XYZ_to_REC2020 but REC2020_to_XYZ is used.
        // Let's look at `args/src/main/mods/colorin.c`
        /* 
        static const float REC2020_to_XYZ[4][4] = {
            { 0.673474789f, 0.165675461f, 0.125049725f, 0.f },
            ...
        };
        */
        // Wait, that [0][0] is 0.673... 
        // Standard Rec2020 D65 to XYZ is ~0.636...
        // Ah, `args` defines `REC2020_to_XYZ` in `mods/colorin.c` with those values.
        // Let's use THOSE values.
        
        static const float REC2020_to_XYZ_ARGS[3][3] = {
            { 0.673474789f, 0.165675461f, 0.125049725f },
            { 0.279040545f, 0.675347328f, 0.045612101f },
            { -0.001932710f, 0.029981442f, 0.796851277f }
        };
        
        static const float XYZ_D65_to_sRGB_ARGS[3][3] = {
            {  3.2404542f, -1.5371385f, -0.4985314f },
            { -0.9692660f,  1.8760108f,  0.0415560f },
            {  0.0556434f, -0.2040259f,  1.0572252f }
        };

        #pragma omp parallel for
        for (size_t i = 0; i < count; i++) {
            float in0 = in[i*4+0]; float in1 = in[i*4+1]; float in2 = in[i*4+2];
            float xyz[3];
            xyz[0] = REC2020_to_XYZ_ARGS[0][0] * in0 + REC2020_to_XYZ_ARGS[0][1] * in1 + REC2020_to_XYZ_ARGS[0][2] * in2;
            xyz[1] = REC2020_to_XYZ_ARGS[1][0] * in0 + REC2020_to_XYZ_ARGS[1][1] * in1 + REC2020_to_XYZ_ARGS[1][2] * in2;
            xyz[2] = REC2020_to_XYZ_ARGS[2][0] * in0 + REC2020_to_XYZ_ARGS[2][1] * in1 + REC2020_to_XYZ_ARGS[2][2] * in2;

            float lin[3];
            lin[0] = XYZ_D65_to_sRGB_ARGS[0][0] * xyz[0] + XYZ_D65_to_sRGB_ARGS[0][1] * xyz[1] + XYZ_D65_to_sRGB_ARGS[0][2] * xyz[2];
            lin[1] = XYZ_D65_to_sRGB_ARGS[1][0] * xyz[0] + XYZ_D65_to_sRGB_ARGS[1][1] * xyz[1] + XYZ_D65_to_sRGB_ARGS[1][2] * xyz[2];
            lin[2] = XYZ_D65_to_sRGB_ARGS[2][0] * xyz[0] + XYZ_D65_to_sRGB_ARGS[2][1] * xyz[1] + XYZ_D65_to_sRGB_ARGS[2][2] * xyz[2];

            for (int c = 0; c < 3; c++) {
                float v = lin[c];
                if (v < 0.0f) v = 0.0f;
                if (v <= 0.0031308f) out[i*4+c] = 12.92f * v;
                else out[i*4+c] = 1.055f * std::pow(v, 1.0f/2.4f) - 0.055f;
            }
            out[i*4+3] = 0.0f;
        }
    }
    debug::dump_buffer("12_colorout", srgb);

    // 16. Write PNG
    std::cout << "13. Writing PNG...\n";
    std::vector<uint8_t> png_data(meta.width * meta.height * 3);
    const float* srgb_data = srgb.data();
    #pragma omp parallel for
    for (size_t i = 0; i < (size_t)meta.width * meta.height; i++) {
        for (int c = 0; c < 3; c++) {
            float v = srgb_data[i * 4 + c];
            v = std::max(0.0f, std::min(1.0f, v));
            png_data[i * 3 + c] = (uint8_t)(v * 255.0f + 0.5f);
        }
    }

    if (!stbi_write_png(output_path.c_str(), meta.width, meta.height, 3, png_data.data(), meta.width * 3)) {
        std::cerr << "Failed to write PNG\n";
        return 1;
    }

    // 17. Autotune match
    std::cout << "14. autotune...\n";
    float current_ev = meta.camera ? meta.camera->style.exposure_ev : 0.0f;
    float ev_adj = modules::autotune::raw_exposure_autotune(input_path, meta.preview_offset, meta.preview_length, png_data.data(), meta.width, meta.height);
    std::cout << "   Current exposure: " << current_ev << " EV\n";
    std::cout << "   Optimal exposure: " << (current_ev + ev_adj) << " EV\n";

    std::cout << "\nDone: " << output_path << " (" << meta.width << "x" << meta.height << ")\n";

    return 0;
}