#include "colorbalancergb.hpp"
#include <cmath>
#include <cstring>
#include <cfloat>
#include <algorithm>

namespace copy::modules::colorbalancergb {

    #define LUT_ELEM 512
    #define DT_UCS_L_STAR_RANGE 2.098883786377f
    #define DT_UCS_L_STAR_UPPER_LIMIT 2.09885f
    #define M_PI_F 3.14159265358979324f

    using AlignedPixel = float[4];

    static inline float sqf(float x) { return x * x; }
    static inline float dt_fast_hypotf(float x, float y) { return std::sqrt(x * x + y * y); }
    static inline float scalar_product(const float a[4], const float b[4]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
    static inline float CLAMPF(float x, float low, float high) { return std::max(low, std::min(x, high)); }

    static inline void dt_colormatrix_transpose(Colormatrix dst, const Colormatrix src) {
        for(int i = 0; i < 3; i++)
            for(int j = 0; j < 3; j++)
                dst[i][j] = src[j][i];
        for(int i = 0; i < 4; i++) dst[i][3] = dst[3][i] = 0.0f;
    }

    static inline void dt_apply_transposed_color_matrix(const AlignedPixel in, const Colormatrix matrix, AlignedPixel out) {
        for(int i = 0; i < 3; i++)
            out[i] = matrix[0][i] * in[0] + matrix[1][i] * in[1] + matrix[2][i] * in[2];
        out[3] = 0.0f;
    }

    static inline void dot_product(const AlignedPixel in, const Colormatrix matrix, AlignedPixel out) {
        for(int i = 0; i < 3; i++)
            out[i] = matrix[i][0] * in[0] + matrix[i][1] * in[1] + matrix[i][2] * in[2];
        out[3] = 0.0f;
    }

    static const Colormatrix XYZ_D65_to_LMS_2006_D65 = {
        {  0.257085f,  0.859943f, -0.031061f, 0.f },
        { -0.394427f,  1.175800f,  0.106423f, 0.f },
        {  0.064856f, -0.076250f,  0.559067f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix LMS_2006_D65_to_XYZ_D65 = {
        {  1.80794659f, -1.29971660f,  0.34785879f, 0.f },
        {  0.61783960f,  0.39595453f, -0.04104687f, 0.f },
        { -0.12546960f,  0.20478038f,  1.74274183f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix LMS_D65_to_filmlightRGB_D65_trans_4x4 = {
        {  1.0877193f, -0.0877193f,  0.0f, 0.f },
        { -0.66666667f, 1.66666667f, 0.0f, 0.f },
        {  0.02061856f, -0.05154639f, 1.03092784f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix filmlightRGB_D65_to_LMS_D65_trans_4x4 = {
        { 0.95f, 0.05f, 0.00f, 0.f },
        { 0.38f, 0.62f, 0.00f, 0.f },
        { 0.00f, 0.03f, 0.97f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static inline void gradingRGB_to_LMS(const AlignedPixel RGB, AlignedPixel LMS) {
        dt_apply_transposed_color_matrix(RGB, filmlightRGB_D65_to_LMS_D65_trans_4x4, LMS);
    }

    static inline void LMS_to_gradingRGB(const AlignedPixel LMS, AlignedPixel RGB) {
        dt_apply_transposed_color_matrix(LMS, LMS_D65_to_filmlightRGB_D65_trans_4x4, RGB);
    }

    static inline void LMS_to_Yrg(const AlignedPixel LMS, AlignedPixel Yrg) {
        float Y = 0.68990272f * LMS[0] + 0.34832189f * LMS[1];
        float a = LMS[0] + LMS[1] + LMS[2];
        AlignedPixel lms = {0}, rgb = {0};
        for(int c=0; c<3; c++) lms[c] = (a == 0.f) ? 0.f : LMS[c] / a;
        LMS_to_gradingRGB(lms, rgb);
        Yrg[0] = Y; Yrg[1] = rgb[0]; Yrg[2] = rgb[1];
    }

    static inline void Yrg_to_LMS(const AlignedPixel Yrg, AlignedPixel LMS) {
        float Y = Yrg[0];
        float r = Yrg[1];
        float g = Yrg[2];
        float b = 1.f - r - g;
        AlignedPixel rgb = { r, g, b, 0.f }, lms = {0};
        gradingRGB_to_LMS(rgb, lms);
        float denom = (0.68990272f * lms[0] + 0.34832189f * lms[1]);
        float a = (denom == 0.f) ? 0.f : Y / denom;
        for(int c=0; c<3; c++) LMS[c] = lms[c] * a;
        LMS[3] = 0.f;
    }

    static inline void Yrg_to_Ych(const AlignedPixel Yrg, AlignedPixel Ych) {
        float Y = Yrg[0];
        float r = Yrg[1] - 0.21902143f;
        float g = Yrg[2] - 0.54371398f;
        float c = dt_fast_hypotf(g, r);
        float cos_h = c != 0.f ? r / c : 1.f;
        float sin_h = c != 0.f ? g / c : 0.f;
        Ych[0] = Y; Ych[1] = c; Ych[2] = cos_h; Ych[3] = sin_h;
    }

    static inline void Ych_to_Yrg(const AlignedPixel Ych, AlignedPixel Yrg) {
        float Y = Ych[0];
        float c = Ych[1];
        float cos_h = Ych[2];
        float sin_h = Ych[3];
        float r = c * cos_h + 0.21902143f;
        float g = c * sin_h + 0.54371398f;
        Yrg[0] = Y; Yrg[1] = r; Yrg[2] = g;
    }

    static inline void LMS_to_XYZ(const AlignedPixel LMS, AlignedPixel XYZ) {
        dot_product(LMS, LMS_2006_D65_to_XYZ_D65, XYZ);
    }

    static inline void gamut_check_Yrg(AlignedPixel Ych) {
        AlignedPixel Yrg = {0};
        Ych_to_Yrg(Ych, Yrg);
        const float D65_r = 0.21902143f;
        const float D65_g = 0.54371398f;
        float max_c = Ych[1];
        float cos_h = Ych[2];
        float sin_h = Ych[3];
        if(Yrg[1] < 0.f) max_c = std::min(-D65_r / cos_h, max_c);
        if(Yrg[2] < 0.f) max_c = std::min(-D65_g / sin_h, max_c);
        if(Yrg[1] + Yrg[2] > 1.f) max_c = std::min((1.f - D65_r - D65_g) / (cos_h + sin_h), max_c);
        Ych[1] = max_c;
    }

    static inline void dt_D65_XYZ_to_xyY(const AlignedPixel XYZ, AlignedPixel xyY) {
        float sum = XYZ[0] + XYZ[1] + XYZ[2];
        xyY[0] = (sum > 0.f) ? XYZ[0] / sum : 0.31272660439158345f;
        xyY[1] = (sum > 0.f) ? XYZ[1] / sum : 0.32902315240275790f;
        xyY[2] = XYZ[1];
    }

    static inline void dt_xyY_to_XYZ(const AlignedPixel xyY, AlignedPixel XYZ) {
        float x = xyY[0];
        float y = std::max(xyY[1], 1e-6f);
        float Y = xyY[2];
        XYZ[0] = Y * x / y;
        XYZ[1] = Y;
        XYZ[2] = Y * (1.f - x - y) / y;
        XYZ[3] = 0.f;
    }

    static inline float Y_to_dt_UCS_L_star(float Y) {
        float Y_hat = std::pow(Y, 0.631651345306265f);
        return DT_UCS_L_STAR_RANGE * Y_hat / (Y_hat + 1.12426773749357f);
    }

    static inline float dt_UCS_L_star_to_Y(float L_star) {
        return std::pow((1.12426773749357f * L_star / (DT_UCS_L_STAR_RANGE - L_star)), 1.5831518565279648f);
    }

    static inline void xyY_to_dt_UCS_UV(const AlignedPixel xyY, float UV_star_prime[2]) {
        const float x_factors[4] = { -0.783941002840055f,  0.745273540913283f, 0.318707282433486f, 0.f };
        const float y_factors[4] = {  0.277512987809202f, -0.205375866083878f, 2.16743692732158f,  0.f };
        const float offsets[4]   = {  0.153836578598858f, -0.165478376301988f, 0.291320554395942f, 0.f };
        float UVD[3] = {0};
        for(int c = 0; c < 3; c++) UVD[c] = x_factors[c] * xyY[0] + y_factors[c] * xyY[1] + offsets[c];
        float div = (UVD[2] >= 0.0f) ? std::max(FLT_MIN, UVD[2]) : std::min(-FLT_MIN, UVD[2]);
        UVD[0] /= div; UVD[1] /= div;
        float UV_star[2] = {0};
        const float factors[2] = { 1.39656225667f, 1.4513954287f };
        const float half_values[2] = { 1.49217352929f, 1.52488637914f };
        for(int c = 0; c < 2; c++) UV_star[c] = factors[c] * UVD[c] / (std::abs(UVD[c]) + half_values[c]);
        UV_star_prime[0] = -1.124983854323892f * UV_star[0] - 0.980483721769325f * UV_star[1];
        UV_star_prime[1] =  1.86323315098672f  * UV_star[0] + 1.971853092390862f * UV_star[1];
    }

    static inline void dt_UCS_LUV_to_JCH(float L_star, float L_white, const float UV_star_prime[2], AlignedPixel JCH) {
        float M2 = UV_star_prime[0] * UV_star_prime[0] + UV_star_prime[1] * UV_star_prime[1];
        JCH[0] = L_star / L_white;
        JCH[1] = 15.932993652962535f * std::pow(L_star, 0.6523997524738018f) * std::pow(M2, 0.6007557017508491f) / L_white;
        JCH[2] = std::atan2(UV_star_prime[1], UV_star_prime[0]);
    }

    static inline void xyY_to_dt_UCS_JCH(const AlignedPixel xyY, float L_white, AlignedPixel JCH) {
        float UV_star_prime[2];
        xyY_to_dt_UCS_UV(xyY, UV_star_prime);
        dt_UCS_LUV_to_JCH(Y_to_dt_UCS_L_star(xyY[2]), L_white, UV_star_prime, JCH);
    }

    static inline void dt_UCS_JCH_to_xyY(const AlignedPixel JCH, float L_white, AlignedPixel xyY) {
        float L_star = CLAMPF(JCH[0] * L_white, 0.f, DT_UCS_L_STAR_UPPER_LIMIT);
        float M = L_star != 0.f
            ? std::pow(JCH[1] * L_white / (15.932993652962535f * std::pow(L_star, 0.6523997524738018f)), 0.8322850678616855f)
            : 0.f;
        float U_star_prime = M * std::cos(JCH[2]);
        float V_star_prime = M * std::sin(JCH[2]);
        float UV_star[2] = { -5.037522385190711f * U_star_prime - 2.504856328185843f * V_star_prime,
                              4.760029407436461f * U_star_prime + 2.874012963239247f * V_star_prime };
        float UV[2] = {0};
        const float factors[2] = { 1.39656225667f, 1.4513954287f };
        const float half_values[2] = { 1.49217352929f, 1.52488637914f };
        for(int c = 0; c < 2; c++) UV[c] = -half_values[c] * UV_star[c] / (std::abs(UV_star[c]) - factors[c]);
        const float U_factors[4] = {  0.167171472114775f,   -0.150959086409163f,    0.940254742367256f,  0.f };
        const float V_factors[4] = {  0.141299802443708f,   -0.155185060382272f,    1.000000000000000f,  0.f };
        const float offsets[4]   = { -0.00801531300850582f, -0.00843312433578007f, -0.0256325967652889f, 0.f };
        float xyD[4] = {0};
        for(int c = 0; c < 3; c++) xyD[c] = U_factors[c] * UV[0] + V_factors[c] * UV[1] + offsets[c];
        float div = (xyD[2] >= 0.0f) ? std::max(FLT_MIN, xyD[2]) : std::min(-FLT_MIN, xyD[2]);
        xyY[0] = xyD[0] / div; xyY[1] = xyD[1] / div; xyY[2] = dt_UCS_L_star_to_Y(L_star);
    }

    static inline void dt_UCS_JCH_to_HCB(const AlignedPixel JCH, AlignedPixel HCB) {
        HCB[2] = JCH[0] * (std::pow(JCH[1], 1.33654221029386f) + 1.f);
        HCB[1] = JCH[1];
        HCB[0] = JCH[2];
    }

    static inline void dt_UCS_HCB_to_JCH(const AlignedPixel HCB, AlignedPixel JCH) {
        JCH[2] = HCB[0];
        JCH[1] = HCB[1];
        JCH[0] = HCB[2] / (std::pow(HCB[1], 1.33654221029386f) + 1.f);
    }

    static inline void dt_UCS_JCH_to_HSB(const AlignedPixel JCH, AlignedPixel HSB) {
        HSB[2] = JCH[0] * (std::pow(JCH[1], 1.33654221029386f) + 1.f);
        HSB[1] = (HSB[2] > 0.f) ? JCH[1] / HSB[2] : 0.f;
        HSB[0] = JCH[2];
    }

    static inline void dt_UCS_HSB_to_JCH(const AlignedPixel HSB, AlignedPixel JCH) {
        JCH[2] = HSB[0];
        JCH[1] = HSB[1] * HSB[2];
        JCH[0] = HSB[2] / (std::pow(JCH[1], 1.33654221029386f) + 1.f);
    }

    static inline float lookup_gamut(const float gamut_lut[LUT_ELEM], float hue) {
        float x_test = (float)LUT_ELEM * (hue + M_PI_F) / (2.f * M_PI_F);
        float x_prev = std::floor(x_test);
        float x_next = std::ceil(x_test);
        int xi = (int)x_prev & (LUT_ELEM - 1);
        int xii = (int)x_next & (LUT_ELEM - 1);
        float y_prev = gamut_lut[xi];
        return y_prev + ((xi != xii) ? (x_test - x_prev) * (gamut_lut[xii] - y_prev) : 0.0f);
    }

    static inline float soft_clip(float x, float soft_threshold, float hard_threshold) {
        float norm = hard_threshold - soft_threshold;
        return (x > soft_threshold) ? soft_threshold + (1.f - std::exp(-(x - soft_threshold) / norm)) * norm : x;
    }

    static inline void opacity_masks(float x, float shadows_weight, float highlights_weight, float midtones_weight, float mask_grey_fulcrum, AlignedPixel output, AlignedPixel output_comp) {
        float x_offset = (x - mask_grey_fulcrum);
        float x_offset_norm = x_offset / mask_grey_fulcrum;
        float alpha = 1.f / (1.f + std::exp(x_offset_norm * shadows_weight));
        float beta = 1.f / (1.f + std::exp(-x_offset_norm * highlights_weight));
        float alpha_comp = 1.f - alpha;
        float beta_comp = 1.f - beta;
        float gamma = std::exp(-sqf(x_offset) * midtones_weight / 4.f) * sqf(alpha_comp) * sqf(beta_comp) * 8.f;
        float gamma_comp = 1.f - gamma;
        output[0] = alpha; output[1] = gamma; output[2] = beta; output[3] = 0.f;
        if(output_comp) { output_comp[0] = alpha_comp; output_comp[1] = gamma_comp; output_comp[2] = beta_comp; output_comp[3] = 0.f; }
    }

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, 
                 const Colormatrix& input_matrix, const Colormatrix& output_matrix, const Params& d) {
        
        Colormatrix input_matrix_trans, output_matrix_trans;
        dt_colormatrix_transpose(input_matrix_trans, input_matrix);
        dt_colormatrix_transpose(output_matrix_trans, output_matrix);

        float L_white = Y_to_dt_UCS_L_star(d.white_fulcrum);
        float hue_rotation_matrix[2][2] = {
            { std::cos(d.hue_angle), -std::sin(d.hue_angle) },
            { std::sin(d.hue_angle),  std::cos(d.hue_angle) },
        };

        size_t count = in.count() / 4;
        const float* input = in.data();
        float* output = out.data();

        #pragma omp parallel for
        for(size_t k = 0; k < count; k++) {
            AlignedPixel RGB;
            RGB[0] = std::max(input[k*4+0], 0.f);
            RGB[1] = std::max(input[k*4+1], 0.f);
            RGB[2] = std::max(input[k*4+2], 0.f);
            RGB[3] = 0.f;

            AlignedPixel LMS;
            dt_apply_transposed_color_matrix(RGB, input_matrix_trans, LMS);

            AlignedPixel Yrg = {0};
            LMS_to_Yrg(LMS, Yrg);

            AlignedPixel Ych = {0};
            Yrg_to_Ych(Yrg, Ych);
            Ych[0] = std::max(Ych[0], 0.f);

            AlignedPixel opacities, opacities_comp;
            opacity_masks(std::pow(Ych[0], 0.4101205819200422f), d.shadows_weight, d.highlights_weight, d.midtones_weight, d.mask_grey_fulcrum, opacities, opacities_comp);

            float cos_h = Ych[2];
            float sin_h = Ych[3];
            Ych[2] = hue_rotation_matrix[0][0] * cos_h + hue_rotation_matrix[0][1] * sin_h;
            Ych[3] = hue_rotation_matrix[1][0] * cos_h + hue_rotation_matrix[1][1] * sin_h;

            float chroma_boost = d.chroma_global + scalar_product(opacities, d.chroma);
            float vibrance = d.vibrance * (1.0f - std::pow(Ych[1], std::abs(d.vibrance)));
            float chroma_factor = std::max(1.f + chroma_boost + vibrance, 0.f);
            Ych[1] *= chroma_factor;

            gamut_check_Yrg(Ych);
            Ych_to_Yrg(Ych, Yrg);
            Yrg_to_LMS(Yrg, LMS);
            LMS_to_gradingRGB(LMS, RGB);

            for(int c=0; c<4; c++) RGB[c] += d.global[c];
            for(int c=0; c<4; c++) RGB[c] *= opacities_comp[2] * (opacities_comp[0] + opacities[0] * d.shadows[c]) + opacities[2] * d.highlights[c];

            float sign[4], abs_RGB[4], scaled_RGB[4];
            for(int c=0; c<4; c++) sign[c] = (RGB[c] < 0.f) ? -1.f : 1.f;
            for(int c=0; c<4; c++) abs_RGB[c] = std::abs(RGB[c]);
            for(int c=0; c<4; c++) scaled_RGB[c] = abs_RGB[c] / d.white_fulcrum;
            for(int c=0; c<4; c++) RGB[c] = std::pow(scaled_RGB[c], d.midtones[c]) * sign[c] * d.white_fulcrum;

            gradingRGB_to_LMS(RGB, LMS);
            LMS_to_Yrg(LMS, Yrg);
            Yrg[0] = std::pow(std::max(Yrg[0] / d.white_fulcrum, 0.f), d.midtones_Y) * d.white_fulcrum;
            Yrg[0] = d.grey_fulcrum * std::pow(Yrg[0] / d.grey_fulcrum, d.contrast);
            Yrg_to_LMS(Yrg, LMS);
            AlignedPixel XYZ_D65 = {0};
            LMS_to_XYZ(LMS, XYZ_D65);

            if(d.saturation_formula != 0) {
                AlignedPixel xyY, JCH, HCB;
                dt_D65_XYZ_to_xyY(XYZ_D65, xyY);
                xyY_to_dt_UCS_JCH(xyY, L_white, JCH);
                dt_UCS_JCH_to_HCB(JCH, HCB);

                float radius = dt_fast_hypotf(HCB[1], HCB[2]);
                float sin_T = (radius > 0.f) ? HCB[1] / radius : 0.f;
                float cos_T = (radius > 0.f) ? HCB[2] / radius : 0.f;
                float M_rot_inv[2][2] = { { cos_T,  sin_T }, { -sin_T, cos_T } };
                float P = std::max(FLT_MIN, HCB[1]);
                float W = sin_T * HCB[1] + cos_T * HCB[2];
                float a = std::max(1.f + d.saturation_global + scalar_product(opacities, d.saturation), 0.f);
                float b = std::max(1.f + d.brilliance_global + scalar_product(opacities, d.brilliance), 0.f);
                float max_a = dt_fast_hypotf(P, W) / P;
                a = soft_clip(a, 0.5f * max_a, max_a);
                float P_prime = (a - 1.f) * P;
                float W_prime = std::sqrt(sqf(P) * (1.f - sqf(a)) + sqf(W)) * b;
                HCB[1] = std::max(M_rot_inv[0][0] * P_prime + M_rot_inv[0][1] * W_prime, 0.f);
                HCB[2] = std::max(M_rot_inv[1][0] * P_prime + M_rot_inv[1][1] * W_prime, 0.f);
                dt_UCS_HCB_to_JCH(HCB, JCH);

                float max_colorfulness = lookup_gamut(d.gamut_LUT, JCH[2]);
                float max_chroma = (15.932993652962535f * std::pow(JCH[0] * L_white, 0.6523997524738018f) * std::pow(max_colorfulness, 0.6007557017508491f) / L_white);
                AlignedPixel JCH_gamut_boundary = { JCH[0], max_chroma, JCH[2], 0.f };
                AlignedPixel HSB_gamut_boundary;
                dt_UCS_JCH_to_HSB(JCH_gamut_boundary, HSB_gamut_boundary);
                AlignedPixel HSB = { HCB[0], (HCB[2] > 0.f) ? HCB[1] / HCB[2] : 0.f, HCB[2], 0.f };
                HSB[1] = soft_clip(HSB[1], 0.8f * HSB_gamut_boundary[1], HSB_gamut_boundary[1]);
                dt_UCS_HSB_to_JCH(HSB, JCH);
                dt_UCS_JCH_to_xyY(JCH, L_white, xyY);
                dt_xyY_to_XYZ(xyY, XYZ_D65);
            }

            AlignedPixel pix_out;
            dt_apply_transposed_color_matrix(XYZ_D65, output_matrix_trans, pix_out);
            pix_out[0] = std::max(pix_out[0], 0.f);
            pix_out[1] = std::max(pix_out[1], 0.f);
            pix_out[2] = std::max(pix_out[2], 0.f);

            output[k*4+0] = pix_out[0];
            output[k*4+1] = pix_out[1];
            output[k*4+2] = pix_out[2];
            output[k*4+3] = input[k*4+3];
        }
    }

}
