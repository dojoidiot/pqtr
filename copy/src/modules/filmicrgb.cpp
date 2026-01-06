#include "filmicrgb.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cfloat>
#include <iostream>

namespace copy::modules::filmicrgb {

    // Helper functions
    static inline float sqf(float x) { return x * x; }
    static inline float CLAMPF(float x, float low, float high) { return std::max(low, std::min(x, high)); }
    static inline float clamp_simd(float x) { return CLAMPF(x, 0.0f, 1.0f); }
    static inline float max3f(const AlignedPixel p) { return std::max(std::max(p[0], p[1]), p[2]); }
    static inline float dt_fast_hypotf(float a, float b) { return sqrtf(a * a + b * b); }

    // Matrices
    static const float filmlightRGB_D65_to_LMS_D65_trans[3][4] = { { 0.95f, 0.05f, 0.00f, 0.f }, { 0.38f, 0.62f, 0.00f, 0.f }, { 0.00f, 0.03f, 0.97f, 0.f } };
    static const float LMS_D65_to_filmlightRGB_D65_trans[3][4] = { { 1.08771930f, -0.0877193f, 0.f, 0.f }, { -0.66666667f, 1.66666667f, 0.f, 0.f }, { 0.02061856f, -0.05154639f, 1.03092784f, 0.f } };
    static const float D65_r = 0.21902143f;
    static const float D65_g = 0.54371398f;

    static inline void dt_apply_transposed_color_matrix(const AlignedPixel in, const Colormatrix matrix_trans, AlignedPixel out) {
        out[0] = matrix_trans[0][0] * in[0] + matrix_trans[1][0] * in[1] + matrix_trans[2][0] * in[2];
        out[1] = matrix_trans[0][1] * in[0] + matrix_trans[1][1] * in[1] + matrix_trans[2][1] * in[2];
        out[2] = matrix_trans[0][2] * in[0] + matrix_trans[1][2] * in[1] + matrix_trans[2][2] * in[2];
        out[3] = 0.f;
    }

    static inline void dt_apply_transposed_color_matrix_3x4(const AlignedPixel in, const float matrix_trans[3][4], AlignedPixel out) {
        out[0] = matrix_trans[0][0] * in[0] + matrix_trans[1][0] * in[1] + matrix_trans[2][0] * in[2];
        out[1] = matrix_trans[0][1] * in[0] + matrix_trans[1][1] * in[1] + matrix_trans[2][1] * in[2];
        out[2] = matrix_trans[0][2] * in[0] + matrix_trans[1][2] * in[1] + matrix_trans[2][2] * in[2];
        out[3] = 0.f;
    }

    static inline void gradingRGB_to_LMS(const AlignedPixel RGB, AlignedPixel LMS) {
        dt_apply_transposed_color_matrix_3x4(RGB, filmlightRGB_D65_to_LMS_D65_trans, LMS);
    }

    static inline void LMS_to_gradingRGB(const AlignedPixel LMS, AlignedPixel RGB) {
        dt_apply_transposed_color_matrix_3x4(LMS, LMS_D65_to_filmlightRGB_D65_trans, RGB);
    }

    static inline void LMS_to_Yrg(const AlignedPixel LMS, AlignedPixel Yrg) {
        float Y = 0.68990272f * LMS[0] + 0.34832189f * LMS[1];
        float a = LMS[0] + LMS[1] + LMS[2];
        AlignedPixel lms = {0};
        for (int c = 0; c < 4; c++) lms[c] = (a == 0.f) ? 0.f : LMS[c] / a;
        AlignedPixel rgb = {0};
        LMS_to_gradingRGB(lms, rgb);
        Yrg[0] = Y; Yrg[1] = rgb[0]; Yrg[2] = rgb[1];
    }

    static inline void Yrg_to_LMS(const AlignedPixel Yrg, AlignedPixel LMS) {
        float Y = Yrg[0];
        float r = Yrg[1];
        float g = Yrg[2];
        float b = 1.f - r - g;
        AlignedPixel rgb = { r, g, b, 0.f };
        AlignedPixel lms = {0};
        gradingRGB_to_LMS(rgb, lms);
        float denom = (0.68990272f * lms[0] + 0.34832189f * lms[1]);
        float a = (denom == 0.f) ? 0.f : Y / denom;
        for (int c = 0; c < 4; c++) LMS[c] = lms[c] * a;
    }

    static inline void Yrg_to_Ych(const AlignedPixel Yrg, AlignedPixel Ych) {
        float Y = Yrg[0];
        float r = Yrg[1] - D65_r;
        float g = Yrg[2] - D65_g;
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
        float r = c * cos_h + D65_r;
        float g = c * sin_h + D65_g;
        Yrg[0] = Y; Yrg[1] = r; Yrg[2] = g;
    }

    static inline void RGB_to_Ych(const AlignedPixel in, const Colormatrix matrix_trans, AlignedPixel out) {
        AlignedPixel LMS = {0};
        AlignedPixel Yrg = {0};
        dt_apply_transposed_color_matrix(in, matrix_trans, LMS);
        LMS_to_Yrg(LMS, Yrg);
        Yrg_to_Ych(Yrg, out);
    }

    static inline void Ych_to_RGB(const AlignedPixel in, const Colormatrix matrix_trans, AlignedPixel out) {
        AlignedPixel LMS = {0};
        AlignedPixel Yrg = {0};
        Ych_to_Yrg(in, Yrg);
        Yrg_to_LMS(Yrg, LMS);
        dt_apply_transposed_color_matrix(LMS, matrix_trans, out);
    }

    static inline void gamut_check_Yrg(AlignedPixel Ych) {
        AlignedPixel Yrg = {0};
        Ych_to_Yrg(Ych, Yrg);
        float max_c = Ych[1];
        float cos_h = Ych[2];
        float sin_h = Ych[3];
        if(Yrg[1] < 0.f) max_c = std::min(-D65_r / cos_h, max_c);
        if(Yrg[2] < 0.f) max_c = std::min(-D65_g / sin_h, max_c);
        if(Yrg[1] + Yrg[2] > 1.f) max_c = std::min((1.f - D65_r - D65_g) / (cos_h + sin_h), max_c);
        Ych[1] = max_c;
    }

    static inline float _clip_chroma_white_raw(const float coeffs[3], float target_white, float Y, float cos_h, float sin_h) {
        float denominator_Y_coeff = coeffs[0] * (0.979381443298969f * cos_h + 0.391752577319588f * sin_h)
                                    + coeffs[1] * (0.0206185567010309f * cos_h + 0.608247422680412f * sin_h)
                                    - coeffs[2] * (cos_h + sin_h);
        float denominator_target_term = target_white * (0.68285981628866f * cos_h + 0.482137060515464f * sin_h);
        if(denominator_Y_coeff == 0.f) return FLT_MAX;
        float Y_asymptote = denominator_target_term / denominator_Y_coeff;
        if(Y <= Y_asymptote) return FLT_MAX;
        float denominator = Y * denominator_Y_coeff - denominator_target_term;
        float numerator = -0.427506877216495f * (Y * (coeffs[0] + 0.856492345150334f * coeffs[1] + 0.554995960637719f * coeffs[2]) - 0.988237752433297f * target_white);
        return numerator / denominator;
    }

    #define CIE_Y_1931_to_CIE_Y_2006(x) (1.05785528f * (x))

    static inline float _clip_chroma_white(const float coeffs[3], float target_white, float Y, float cos_h, float sin_h) {
        float eps = 1e-3f;
        float max_Y = CIE_Y_1931_to_CIE_Y_2006(target_white);
        float delta_Y = std::max(max_Y - Y, 0.f);
        float max_chroma;
        if(delta_Y < eps) max_chroma = delta_Y / (eps * max_Y) * _clip_chroma_white_raw(coeffs, target_white, (1.f - eps) * max_Y, cos_h, sin_h);
        else max_chroma = _clip_chroma_white_raw(coeffs, target_white, Y, cos_h, sin_h);
        return max_chroma >= 0.f ? max_chroma : FLT_MAX;
    }

    static inline float _clip_chroma_black(const float coeffs[3], float cos_h, float sin_h) {
        float denominator = coeffs[0] * (0.979381443298969f * cos_h + 0.391752577319588f * sin_h)
                            + coeffs[1] * (0.0206185567010309f * cos_h + 0.608247422680412f * sin_h)
                            - coeffs[2] * (cos_h + sin_h);
        if(denominator == 0.f) return FLT_MAX;
        float numerator = -0.427506877216495f * (coeffs[0] + 0.856492345150334f * coeffs[1] + 0.554995960637719f * coeffs[2]);
        float max_chroma = numerator / denominator;
        return max_chroma >= 0.f ? max_chroma : FLT_MAX;
    }

    static inline float Ych_max_chroma_without_negatives(const Colormatrix matrix_out, float cos_h, float sin_h) {
        float chroma_R_black = _clip_chroma_black(matrix_out[0], cos_h, sin_h);
        float chroma_G_black = _clip_chroma_black(matrix_out[1], cos_h, sin_h);
        float chroma_B_black = _clip_chroma_black(matrix_out[2], cos_h, sin_h);
        return std::min(std::min(chroma_R_black, chroma_G_black), chroma_B_black);
    }

    static inline float Ych_max_chroma(const Colormatrix matrix_out, float target_white, float Y, float cos_h, float sin_h) {
        float chroma_R_white = _clip_chroma_white(matrix_out[0], target_white, Y, cos_h, sin_h);
        float chroma_G_white = _clip_chroma_white(matrix_out[1], target_white, Y, cos_h, sin_h);
        float chroma_B_white = _clip_chroma_white(matrix_out[2], target_white, Y, cos_h, sin_h);
        float max_chroma_white = std::min(std::min(chroma_R_white, chroma_G_white), chroma_B_white);
        float max_chroma_black = Ych_max_chroma_without_negatives(matrix_out, cos_h, sin_h);
        return std::min(max_chroma_black, max_chroma_white);
    }

    static inline void gamut_check_RGB(const Colormatrix matrix_in_trans, const Colormatrix matrix_out, const Colormatrix matrix_out_trans, float display_black, float display_white, const AlignedPixel Ych_in, AlignedPixel RGB_out) {
        AlignedPixel RGB_brightened = {0};
        Ych_to_RGB(Ych_in, matrix_out_trans, RGB_brightened);
        float min_pix = std::min(std::min(RGB_brightened[0], RGB_brightened[1]), RGB_brightened[2]);
        float black_offset = std::max(-min_pix, 0.f);
        for (int c = 0; c < 4; c++) RGB_brightened[c] += black_offset;
        AlignedPixel Ych_brightened = {0};
        RGB_to_Ych(RGB_brightened, matrix_in_trans, Ych_brightened);
        float Y = CLAMPF((Ych_in[0] + Ych_brightened[0]) / 2.f, CIE_Y_1931_to_CIE_Y_2006(display_black), CIE_Y_1931_to_CIE_Y_2006(display_white));
        float cos_h = Ych_in[2];
        float sin_h = Ych_in[3];
        float new_chroma = std::min(Ych_in[1], Ych_max_chroma(matrix_out, display_white, Y, cos_h, sin_h));
        AlignedPixel Ych = { Y, new_chroma, cos_h, sin_h };
        Ych_to_RGB(Ych, matrix_out_trans, RGB_out);
        for (int c = 0; c < 4; c++) RGB_out[c] = CLAMPF(RGB_out[c], 0.f, display_white);
    }

    static inline void gamut_mapping(AlignedPixel Ych_final, const AlignedPixel Ych_original, AlignedPixel pix_out,
                                     const Colormatrix input_matrix_trans, const Colormatrix output_matrix, const Colormatrix output_matrix_trans,
                                     const Colormatrix export_input_matrix_trans, const Colormatrix export_output_matrix, const Colormatrix export_output_matrix_trans,
                                     float display_black, float display_white, int use_output_profile) {
        Ych_final[2] = Ych_original[2];
        Ych_final[3] = Ych_original[3];
        Ych_final[0] = CLAMPF(Ych_final[0], CIE_Y_1931_to_CIE_Y_2006(display_black), CIE_Y_1931_to_CIE_Y_2006(display_white));
        gamut_check_Yrg(Ych_final);
        if(!use_output_profile) {
            gamut_check_RGB(input_matrix_trans, output_matrix, output_matrix_trans, display_black, display_white, Ych_final, pix_out);
        } else {
            gamut_check_RGB(export_input_matrix_trans, export_output_matrix, export_output_matrix_trans, display_black, display_white, Ych_final, pix_out);
            AlignedPixel LMS = {0};
            dt_apply_transposed_color_matrix(pix_out, export_input_matrix_trans, LMS);
            dt_apply_transposed_color_matrix(LMS, output_matrix_trans, pix_out);
        }
    }

    static inline float exp_tonemapping_v2(float x, float grey, float black, float dynamic_range) {
        return grey * exp2f(dynamic_range * x + black);
    }

    static inline float log_tonemapping_v2_1ch(float x, float grey, float black, float dynamic_range) {
        return clamp_simd((log2f(x / grey) - black) / dynamic_range);
    }

    static inline void log_tonemapping_v2(AlignedPixel mapped, const AlignedPixel x, float grey, float black, float dynamic_range) {
        for (int c = 0; c < 4; c++) {
            float scaled = x[c] / grey;
            float log_val = log2f(scaled);
            mapped[c] = CLAMPF((log_val - black) / dynamic_range, 0.f, 1.f);
        }
    }

    static inline float filmic_spline(float x, const AlignedPixel M1, const AlignedPixel M2, const AlignedPixel M3, const AlignedPixel M4, const AlignedPixel M5, float latitude_min, float latitude_max, const CurveType type[2]) {
        float result;
        if(x < latitude_min) {
            if(type[0] == DT_FILMIC_CURVE_POLY_4) result = M1[0] + x * (M2[0] + x * (M3[0] + x * (M4[0] + x * M5[0])));
            else if(type[0] == DT_FILMIC_CURVE_POLY_3) result = M1[0] + x * (M2[0] + x * (M3[0] + x * M4[0]));
            else { float xi = latitude_min - x; float rat = xi * (xi * M2[0] + 1.f); result = M4[0] - M1[0] * rat / (rat + M3[0]); }
        } else if(x > latitude_max) {
            if(type[1] == DT_FILMIC_CURVE_POLY_4) result = M1[1] + x * (M2[1] + x * (M3[1] + x * (M4[1] + x * M5[1])));
            else if(type[1] == DT_FILMIC_CURVE_POLY_3) result = M1[1] + x * (M2[1] + x * (M3[1] + x * M4[1]));
            else { float xi = x - latitude_max; float rat = xi * (xi * M2[1] + 1.f); result = M4[1] + M1[1] * rat / (rat + M3[1]); }
        } else {
            result = M1[2] + x * M2[2];
        }
        return result;
    }

    static inline void RGB_tone_mapping_v4(const AlignedPixel pix_in, AlignedPixel pix_out, const Params& data, float display_black, float display_white) {
        AlignedPixel mapped;
        log_tonemapping_v2(mapped, pix_in, data.grey_source, data.black_source, data.dynamic_range);
        for(int c = 0; c < 3; c++) mapped[c] = filmic_spline(mapped[c], data.spline.M1, data.spline.M2, data.spline.M3, data.spline.M4, data.spline.M5, data.spline.latitude_min, data.spline.latitude_max, data.spline.type);
        for(int c = 0; c < 4; c++) mapped[c] = CLAMPF(mapped[c], 0.0f, display_white);
        for(int c = 0; c < 4; c++) pix_out[c] = powf(mapped[c], data.output_power);
    }

    static inline void norm_tone_mapping_v4(const AlignedPixel pix_in, AlignedPixel pix_out, const Params& data, float norm_min, float norm_max, float display_black, float display_white) {
        float norm = CLAMPF(max3f(pix_in), norm_min, norm_max);
        AlignedPixel ratios = {0.0f};
        for(int c = 0; c < 4; c++) ratios[c] = pix_in[c] / norm;
        norm = log_tonemapping_v2_1ch(norm, data.grey_source, data.black_source, data.dynamic_range);
        norm = powf(CLAMPF(filmic_spline(norm, data.spline.M1, data.spline.M2, data.spline.M3, data.spline.M4, data.spline.M5, data.spline.latitude_min, data.spline.latitude_max, data.spline.type), display_black, display_white), data.output_power);
        for(int c = 0; c < 4; c++) pix_out[c] = ratios[c] * norm;
    }

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, 
                 const Params& data,
                 const Colormatrix& input_matrix_trans,
                 const Colormatrix& output_matrix,
                 const Colormatrix& output_matrix_trans,
                 const Colormatrix& export_input_matrix_trans,
                 const Colormatrix& export_output_matrix,
                 const Colormatrix& export_output_matrix_trans,
                 float display_black, float display_white,
                 int use_output_profile) {
        
        float norm_min = exp_tonemapping_v2(0.f, data.grey_source, data.black_source, data.dynamic_range);
        float norm_max = exp_tonemapping_v2(1.f, data.grey_source, data.black_source, data.dynamic_range);

        size_t count = in.count() / 4;
        const float* input = in.data();
        float* output = out.data();

        #pragma omp parallel for
        for(size_t k = 0; k < count; k++) {
            const float* pix_in = input + k*4;
            AlignedPixel max_rgb = {0.f};
            AlignedPixel naive_rgb = {0.f};
            RGB_tone_mapping_v4(pix_in, naive_rgb, data, display_black, display_white);
            norm_tone_mapping_v4(pix_in, max_rgb, data, norm_min, norm_max, display_black, display_white);
            AlignedPixel pix_out;
            for(int c = 0; c < 4; c++) pix_out[c] = 0.5f * naive_rgb[c] + 0.5f * max_rgb[c];
            AlignedPixel Ych_original = {0.f};
            RGB_to_Ych(pix_in, input_matrix_trans, Ych_original);
            AlignedPixel Ych_final = {0.f};
            RGB_to_Ych(pix_out, input_matrix_trans, Ych_final);
            Ych_final[1] = std::min(Ych_original[1], Ych_final[1]);
            gamut_mapping(Ych_final, Ych_original, pix_out, input_matrix_trans, output_matrix, output_matrix_trans, export_input_matrix_trans, export_output_matrix, export_output_matrix_trans, display_black, display_white, use_output_profile);
            for(int c = 0; c < 4; c++) output[k*4 + c] = pix_out[c];
        }
    }

    // --- Gaussian Elimination ---
    static int gauss_make_triangular(double *A, int *p, int n) {
        p[n - 1] = n - 1;
        for(int k = 0; k < n; ++k) {
            int m = k;
            for(int i = k + 1; i < n; ++i) if(std::abs(A[k + n * i]) > std::abs(A[k + n * m])) m = i;
            p[k] = m;
            double t1 = A[k + n * m];
            A[k + n * m] = A[k + n * k];
            A[k + n * k] = t1;
            if(t1 != 0) {
                for(int i = k + 1; i < n; ++i) A[k + n * i] /= -t1;
                if(k != m) for(int i = k + 1; i < n; ++i) { double t2 = A[i + n * m]; A[i + n * m] = A[i + n * k]; A[i + n * k] = t2; }
                for(int j = k + 1; j < n; ++j) for(int i = k + 1; i < n; ++i) A[i + n * j] += A[k + j * n] * A[i + k * n];
            } else return 0;
        }
        return 1;
    }

    static void gauss_solve_triangular(const double *A, const int *p, double *b, int n) {
        for(int k = 0; k < n - 1; ++k) {
            int m = p[k];
            double t = b[m];
            b[m] = b[k];
            b[k] = t;
            for(int i = k + 1; i < n; ++i) b[i] += A[k + n * i] * t;
        }
        for(int k = n - 1; k > 0; --k) {
            b[k] /= A[k + n * k];
            double t = b[k];
            for(int i = 0; i < k; ++i) b[i] -= A[k + n * i] * t;
        }
        b[0] /= A[0 + 0 * n];
    }

    static int gauss_solve(double *A, double *b, int n) {
        int p[5];
        int err_code = gauss_make_triangular(A, p, n);
        if(err_code) gauss_solve_triangular(A, p, b, n);
        return err_code;
    }

    void compute_spline(Params& d) {
        float grey_display = powf(0.1845f, 1.0f / d.output_power);
        float black_display = 0.0f;
        float white_display = 1.0f;
        float black_log = 0.0f;
        float grey_log = fabsf(d.black_source) / d.dynamic_range;
        float white_log = 1.0f;
        float latitude = 0.01f;
        float hardness = d.output_power;
        float slope = d.contrast * d.dynamic_range / 8.0f;
        float min_contrast = 1.0f;
        min_contrast = std::max(min_contrast, (white_display - grey_display) / (white_log - grey_log));
        min_contrast = std::max(min_contrast, (grey_display - black_display) / (grey_log - black_log));
        min_contrast += 0.01f;
        float actual_contrast = slope / (hardness * powf(grey_display, hardness - 1.0f));
        actual_contrast = CLAMPF(actual_contrast, min_contrast, 100.0f);
        float linear_intercept = grey_display - (actual_contrast * grey_log);
        float xmin = (black_display + 0.01f * (white_display - black_display) - linear_intercept) / actual_contrast;
        float xmax = (white_display - 0.01f * (white_display - black_display) - linear_intercept) / actual_contrast;
        float toe_log = (1.0f - latitude) * grey_log + latitude * xmin;
        float shoulder_log = (1.0f - latitude) * grey_log + latitude * xmax;
        float toe_display = toe_log * actual_contrast + linear_intercept;
        float shoulder_display = shoulder_log * actual_contrast + linear_intercept;

        d.spline.x[0] = black_log; d.spline.x[1] = toe_log; d.spline.x[2] = grey_log; d.spline.x[3] = shoulder_log; d.spline.x[4] = white_log;
        d.spline.y[0] = black_display; d.spline.y[1] = toe_display; d.spline.y[2] = grey_display; d.spline.y[3] = shoulder_display; d.spline.y[4] = white_display;
        d.spline.latitude_min = toe_log; d.spline.latitude_max = shoulder_log;
        d.spline.type[0] = DT_FILMIC_CURVE_POLY_4; d.spline.type[1] = DT_FILMIC_CURVE_POLY_4;

        d.spline.M2[2] = actual_contrast;
        d.spline.M1[2] = d.spline.y[1] - d.spline.M2[2] * d.spline.x[1];
        d.spline.M3[2] = 0.f; d.spline.M4[2] = 0.f; d.spline.M5[2] = 0.f;

        const double Tl = d.spline.x[1];
        double Tl2 = Tl * Tl; double Tl3 = Tl2 * Tl; double Tl4 = Tl3 * Tl;
        double A0[25] = { 0., 0., 0., 0., 1., 0., 0., 0., 1., 0., Tl4, Tl3, Tl2, Tl, 1., 4.*Tl3, 3.*Tl2, 2.*Tl, 1., 0., 12.*Tl2, 6.*Tl, 2., 0., 0. };
        double b0[5] = { (double)d.spline.y[0], 0., (double)d.spline.y[1], (double)d.spline.M2[2], 0. };
        gauss_solve(A0, b0, 5);
        d.spline.M5[0] = b0[0]; d.spline.M4[0] = b0[1]; d.spline.M3[0] = b0[2]; d.spline.M2[0] = b0[3]; d.spline.M1[0] = b0[4];

        const double Sl = d.spline.x[3];
        double Sl2 = Sl * Sl; double Sl3 = Sl2 * Sl;
        double A1[16] = { 1., 1., 1., 1., Sl3, Sl2, Sl, 1., 3.*Sl2, 2.*Sl, 1., 0., 6.*Sl, 2., 0., 0. };
        double b1[4] = { (double)d.spline.y[4], (double)d.spline.y[3], (double)d.spline.M2[2], 0. };
        gauss_solve(A1, b1, 4);
        d.spline.M5[1] = 0.0f; d.spline.M4[1] = b1[0]; d.spline.M3[1] = b1[1]; d.spline.M2[1] = b1[2]; d.spline.M1[1] = b1[3];

        d.sigma_toe = powf(d.spline.latitude_min / 3.0f, 2.0f);
        d.sigma_shoulder = powf((1.0f - d.spline.latitude_max) / 3.0f, 2.0f);
    }

    void autotune(Params& data, const core::ImageBuffer<core::f32>& in) {
        float min_rgb[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
        float max_rgb[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        size_t count = in.count() / 4;
        const float* pixels = in.data();

        std::cout << "autotune: count=" << count << " ptr=" << (void*)pixels << "\n";

        for(size_t i=0; i<count; i++) {
            for(int c=0; c<3; c++) {
                float v = pixels[4*i+c];
                if(v < min_rgb[c]) min_rgb[c] = v;
                if(v > max_rgb[c]) max_rgb[c] = v;
            }
        }

        float black = std::max(std::max(min_rgb[0], min_rgb[1]), min_rgb[2]);
        float white = std::max(std::max(max_rgb[0], max_rgb[1]), max_rgb[2]);
        float grey = data.grey_source;
        float EVmin = CLAMPF(log2f(black / grey), -16.0f, -1.0f);
        float EVmax = CLAMPF(log2f(white / grey), 1.0f, 16.0f);

        data.black_source = std::max(EVmin, -16.0f);
        data.white_source = EVmax;
        data.dynamic_range = data.white_source - data.black_source;
        data.output_power = CLAMPF(logf(18.45f / 100.0f) / logf(-data.black_source / data.dynamic_range), 1.0f, 10.0f);
        float reconstruct_threshold = powf(2.0f, data.white_source + 0.0f) * grey;
        float reconstruct_feather = exp2f(12.f / 3.0f);
        data.normalize = reconstruct_feather / reconstruct_threshold;

        std::cout << std::scientific;
        std::cout.precision(10);
        std::cout << "filmicrgb_autotune: min_rgb=[" << min_rgb[0] << "," << min_rgb[1] << "," << min_rgb[2] << "] max_rgb=[" 
                  << max_rgb[0] << "," << max_rgb[1] << "," << max_rgb[2] << "] black=" << black << " white=" << white 
                  << " grey=" << grey << " EVmin=" << EVmin << " EVmax=" << EVmax << "\n";
        std::cout << "filmicrgb_autotune: black_ev=" << data.black_source << " white_ev=" << data.white_source 
                  << " DR=" << data.dynamic_range << " output_power=" << data.output_power << "\n";
        std::cout << std::defaultfloat;

        compute_spline(data);
        std::cout << "Spline M1: " << data.spline.M1[0] << " " << data.spline.M1[1] << " " << data.spline.M1[2] << "\n";
    }

}