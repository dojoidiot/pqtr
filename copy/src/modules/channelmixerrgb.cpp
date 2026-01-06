#include "channelmixerrgb.hpp"
#include <cmath>
#include <cstring>
#include <cfloat>
#include <algorithm>

namespace copy::modules::channelmixerrgb {

    enum {
        DT_ADAPTATION_LINEAR_BRADFORD = 0,
        DT_ADAPTATION_CAT16 = 1,
        DT_ADAPTATION_FULL_BRADFORD = 2,
        DT_ADAPTATION_XYZ = 3,
        DT_ADAPTATION_RGB = 4,
        DT_ADAPTATION_LAST
    };

    enum {
        CHANNELMIXERRGB_V_1 = 0,
        CHANNELMIXERRGB_V_2 = 1,
        CHANNELMIXERRGB_V_3 = 2,
    };

    static const Colormatrix XYZ_to_Bradford_LMS = {
        {  0.8951f,  0.2664f, -0.1614f, 0.f },
        { -0.7502f,  1.7135f,  0.0367f, 0.f },
        {  0.0389f, -0.0685f,  1.0296f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix XYZ_to_Bradford_LMS_trans = {
        {  0.8951f, -0.7502f,  0.0389f, 0.f },
        {  0.2664f,  1.7135f, -0.0685f, 0.f },
        { -0.1614f,  0.0367f,  1.0296f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix Bradford_LMS_to_XYZ = {
        {  0.9870f, -0.1471f,  0.1600f, 0.f },
        {  0.4323f,  0.5184f,  0.0493f, 0.f },
        { -0.0085f,  0.0400f,  0.9685f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix Bradford_LMS_to_XYZ_trans = {
        {  0.9870f,  0.4323f, -0.0085f, 0.f },
        { -0.1471f,  0.5184f,  0.0400f, 0.f },
        {  0.1600f,  0.0493f,  0.9685f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix XYZ_to_CAT16_LMS = {
        {  0.401288f, 0.650173f, -0.051461f, 0.f },
        { -0.250268f, 1.204414f,  0.045854f, 0.f },
        { -0.002079f, 0.048952f,  0.953127f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix XYZ_to_CAT16_LMS_trans = {
        {  0.401288f, -0.250268f, -0.002079f, 0.f },
        {  0.650173f,  1.204414f,  0.048952f, 0.f },
        { -0.051461f,  0.045854f,  0.953127f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix CAT16_LMS_to_XYZ = {
        {  1.862068f, -1.011255f,  0.149187f, 0.f },
        {  0.38752f ,  0.621447f, -0.008974f, 0.f },
        { -0.015841f, -0.034123f,  1.049964f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const Colormatrix CAT16_LMS_to_XYZ_trans = {
        {  1.862068f,  0.38752f , -0.015841f, 0.f },
        { -1.011255f,  0.621447f, -0.034123f, 0.f },
        {  0.149187f, -0.008974f,  1.049964f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    static const struct { float x, y, Y; } D50xyY = { 0.34567f, 0.35850f, 1.0f };

    #define NORM_MIN 1.52587890625e-05f
    #define INVERSE_SQRT_3 0.5773502691896258f
    #define DT_FMA(a, b, c) ((a) * (b) + (c))

    static inline float sqf(float x) { return x * x; }

    static inline void dt_apply_transposed_color_matrix(const AlignedPixel in, const Colormatrix matrix, AlignedPixel out) {
        for(int i = 0; i < 3; i++)
            out[i] = matrix[0][i] * in[0] + matrix[1][i] * in[1] + matrix[2][i] * in[2];
        out[3] = 0.0f;
    }

    static inline void dt_colormatrix_mul(Colormatrix dst, const Colormatrix m1, const Colormatrix m2) {
        for(int k = 0; k < 3; k++) {
            for(int i = 0; i < 3; i++) {
                float sum = 0.0f;
                for(int j = 0; j < 3; j++) sum += m1[k][j] * m2[j][i];
                dst[k][i] = sum;
            }
            dst[k][3] = 0.0f;
        }
        for(int i = 0; i < 4; i++) dst[3][i] = 0.0f;
    }

    static inline void dt_colormatrix_transpose(Colormatrix dst, const Colormatrix src) {
        for(int i = 0; i < 3; i++)
            for(int j = 0; j < 3; j++)
                dst[i][j] = src[j][i];
        for(int i = 0; i < 4; i++) dst[i][3] = dst[3][i] = 0.0f;
    }

    static inline void dt_colormatrix_copy(Colormatrix dst, const Colormatrix src) {
        std::memcpy(dst, src, sizeof(Colormatrix));
    }

    static inline void convert_XYZ_to_bradford_LMS(const AlignedPixel XYZ, AlignedPixel LMS) {
        dt_apply_transposed_color_matrix(XYZ, XYZ_to_Bradford_LMS_trans, LMS);
    }

    static inline void make_RGB_to_Bradford_LMS(const Colormatrix rgb, Colormatrix lms) {
        dt_colormatrix_mul(lms, XYZ_to_Bradford_LMS, rgb);
    }

    static inline void convert_bradford_LMS_to_XYZ(const AlignedPixel LMS, AlignedPixel XYZ) {
        dt_apply_transposed_color_matrix(LMS, Bradford_LMS_to_XYZ_trans, XYZ);
    }

    static inline void make_Bradford_LMS_to_XYZ(const Colormatrix lms, Colormatrix xyz) {
        dt_colormatrix_mul(xyz, Bradford_LMS_to_XYZ, lms);
    }

    static inline void convert_XYZ_to_CAT16_LMS(const AlignedPixel XYZ, AlignedPixel LMS) {
        dt_apply_transposed_color_matrix(XYZ, XYZ_to_CAT16_LMS_trans, LMS);
    }

    static inline void make_RGB_to_CAT16_LMS(const Colormatrix rgb, Colormatrix lms) {
        dt_colormatrix_mul(lms, XYZ_to_CAT16_LMS, rgb);
    }

    static inline void convert_CAT16_LMS_to_XYZ(const AlignedPixel LMS, AlignedPixel XYZ) {
        dt_apply_transposed_color_matrix(LMS, CAT16_LMS_to_XYZ_trans, XYZ);
    }

    static inline void make_CAT16_LMS_to_XYZ(const Colormatrix lms, Colormatrix xyz) {
        dt_colormatrix_mul(xyz, CAT16_LMS_to_XYZ, lms);
    }

    static inline void convert_any_LMS_to_XYZ(const AlignedPixel LMS, AlignedPixel XYZ, int kind) {
        switch(kind) {
            case DT_ADAPTATION_FULL_BRADFORD:
            case DT_ADAPTATION_LINEAR_BRADFORD:
                convert_bradford_LMS_to_XYZ(LMS, XYZ);
                break;
            case DT_ADAPTATION_CAT16:
                convert_CAT16_LMS_to_XYZ(LMS, XYZ);
                break;
            case DT_ADAPTATION_XYZ:
            case DT_ADAPTATION_RGB:
            case DT_ADAPTATION_LAST:
            default:
                XYZ[0] = LMS[0]; XYZ[1] = LMS[1]; XYZ[2] = LMS[2];
                break;
        }
    }

    static inline void convert_any_XYZ_to_LMS(const AlignedPixel XYZ, AlignedPixel LMS, int kind) {
        switch(kind) {
            case DT_ADAPTATION_FULL_BRADFORD:
            case DT_ADAPTATION_LINEAR_BRADFORD:
                convert_XYZ_to_bradford_LMS(XYZ, LMS);
                break;
            case DT_ADAPTATION_CAT16:
                convert_XYZ_to_CAT16_LMS(XYZ, LMS);
                break;
            case DT_ADAPTATION_XYZ:
            case DT_ADAPTATION_RGB:
            case DT_ADAPTATION_LAST:
            default:
                LMS[0] = XYZ[0]; LMS[1] = XYZ[1]; LMS[2] = XYZ[2];
                break;
        }
    }

    static inline void bradford_adapt_D50(const AlignedPixel lms_in, const AlignedPixel origin_illuminant, float p, int full, AlignedPixel lms_out) {
        const AlignedPixel D50 = { 0.996078f, 1.020646f, 0.818155f, 0.f };
        AlignedPixel temp = { lms_in[0] / origin_illuminant[0], lms_in[1] / origin_illuminant[1], lms_in[2] / origin_illuminant[2], 0.f };
        if(full) temp[2] = (temp[2] > 0.f) ? std::pow(temp[2], p) : temp[2];
        lms_out[0] = D50[0] * temp[0];
        lms_out[1] = D50[1] * temp[1];
        lms_out[2] = D50[2] * temp[2];
    }

    static inline void CAT16_adapt_D50(const AlignedPixel lms_in, const AlignedPixel origin_illuminant, float D, int full, AlignedPixel lms_out) {
        const AlignedPixel D50 = { 0.994535f, 1.000997f, 0.833036f, 0.f };
        if(full) {
            lms_out[0] = lms_in[0] * D50[0] / origin_illuminant[0];
            lms_out[1] = lms_in[1] * D50[1] / origin_illuminant[1];
            lms_out[2] = lms_in[2] * D50[2] / origin_illuminant[2];
        } else {
            lms_out[0] = lms_in[0] * (D * D50[0] / origin_illuminant[0] + 1.f - D);
            lms_out[1] = lms_in[1] * (D * D50[1] / origin_illuminant[1] + 1.f - D);
            lms_out[2] = lms_in[2] * (D * D50[2] / origin_illuminant[2] + 1.f - D);
        }
    }

    static inline void dt_xyY_to_uvY(const AlignedPixel xyY, AlignedPixel uvY) {
        const float div = -2.0f * xyY[0] + 12.0f * xyY[1] + 3.0f;
        uvY[0] = 4.0f * xyY[0] / div;
        uvY[1] = 9.0f * xyY[1] / div;
        uvY[2] = xyY[2];
        uvY[3] = 0.f;
    }

    static inline void dt_uvY_to_xyY(const AlignedPixel uvY, AlignedPixel xyY) {
        const float div = 6.0f * uvY[0] - 16.0f * uvY[1] + 12.0f;
        xyY[0] = 9.0f * uvY[0] / div;
        xyY[1] = 4.0f * uvY[1] / div;
        xyY[2] = uvY[2];
        xyY[3] = 0.f;
    }

    static inline void dt_xyY_to_XYZ(const AlignedPixel xyY, AlignedPixel XYZ) {
        XYZ[0] = xyY[2] * xyY[0] / xyY[1];
        XYZ[1] = xyY[2];
        XYZ[2] = xyY[2] * (1.0f - xyY[0] - xyY[1]) / xyY[1];
        XYZ[3] = 0.f;
    }

    static inline float euclidean_norm(const AlignedPixel x) {
        return std::sqrt(sqf(x[0]) + sqf(x[1]) + sqf(x[2]));
    }

    static inline float scalar_product(const AlignedPixel x, const AlignedPixel y) {
        return x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
    }

    static inline void _gamut_mapping(const AlignedPixel input, float compression, int clip, AlignedPixel output) {
        const float sum = input[0] + input[1] + input[2];
        const float Y = input[1];
        AlignedPixel xyY = { sum > 0.0f ? input[0] / sum : D50xyY.x,
                             sum > 0.0f ? input[1] / sum : D50xyY.y,
                             Y, 0.0f };
        AlignedPixel uvY;
        dt_xyY_to_uvY(xyY, uvY);

        const float D50[2] = { 0.20915914598542354f, 0.488075320769787f };
        const float delta[2] = { D50[0] - uvY[0], D50[1] - uvY[1] };
        const float Delta = Y * (sqf(delta[0]) + sqf(delta[1]));
        const float correction = (compression == 0.0f) ? 0.f : std::pow(Delta, compression);
        for(size_t c = 0; c < 2; c++) {
            float tmp = DT_FMA(correction, delta[c], uvY[c]);
            uvY[c] = (uvY[c] > D50[c]) ? std::max(tmp, D50[c]) : std::min(tmp, D50[c]);
        }
        dt_uvY_to_xyY(uvY, xyY);
        if(clip) for(size_t c = 0; c < 2; c++) xyY[c] = std::max(xyY[c], 0.0f);
        xyY[1] = std::max(xyY[1], NORM_MIN);
        float scale = xyY[0] + xyY[1];
        bool sanitize = (scale >= 1.f);
        for(size_t c = 0; c < 2; c++) xyY[c] = (sanitize) ? xyY[c] / scale : xyY[c];
        dt_xyY_to_XYZ(xyY, output);
    }

    static inline void _luma_chroma(const AlignedPixel input, const AlignedPixel saturation, const AlignedPixel lightness, AlignedPixel output, int version) {
        float norm = euclidean_norm(input);
        float avg = std::max((input[0] + input[1] + input[2]) / 3.0f, NORM_MIN);

        if(norm > 0.f && avg > 0.f) {
            float mix = scalar_product(input, lightness);
            if(version == CHANNELMIXERRGB_V_3) norm *= INVERSE_SQRT_3;
            for(int c=0; c<3; c++) output[c] = input[c] / norm;
            float coeff_ratio = 0.f;
            if(version == CHANNELMIXERRGB_V_1) {
                for(int c=0; c<3; c++) coeff_ratio += sqf(1.0f - output[c]) * saturation[c];
            } else {
                coeff_ratio = scalar_product(output, saturation) / 3.f;
            }
            for(int c=0; c<3; c++) {
                float min_ratio = (output[c] < 0.0f) ? output[c] : 0.0f;
                float output_inverse = 1.0f - output[c];
                output[c] = std::max(DT_FMA(output_inverse, coeff_ratio, output[c]), min_ratio);
            }
            if(version == CHANNELMIXERRGB_V_3) norm /= euclidean_norm(output) * INVERSE_SQRT_3;
            norm *= std::max(1.f + mix / avg, 0.f);
            for(int c=0; c<3; c++) output[c] *= norm;
        } else {
            for(int c=0; c<3; c++) output[c] = input[c];
        }
    }

    static inline void dt_vector_max_nan(AlignedPixel out, const float *in, const AlignedPixel min_value) {
        for(int c=0; c<4; c++) out[c] = (std::isnan(in[c]) || in[c] < min_value[c]) ? min_value[c] : in[c];
    }

    static inline void dt_vector_clipneg_nan(AlignedPixel out) {
        for(int c=0; c<4; c++) out[c] = (std::isnan(out[c]) || out[c] < 0.0f) ? 0.0f : out[c];
    }

    static inline void copy_pixel(AlignedPixel out, const AlignedPixel in) {
        std::memcpy(out, in, sizeof(AlignedPixel));
    }

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, 
                 const Colormatrix& RGB_to_XYZ, const Colormatrix& XYZ_to_RGB, const Params& p) {
        
        Colormatrix RGB_to_LMS = {{0}};
        Colormatrix MIX_to_XYZ = {{0}};

        switch (p.adaptation) {
            case DT_ADAPTATION_FULL_BRADFORD:
            case DT_ADAPTATION_LINEAR_BRADFORD:
                make_RGB_to_Bradford_LMS(RGB_to_XYZ, RGB_to_LMS);
                make_Bradford_LMS_to_XYZ(p.MIX, MIX_to_XYZ);
                break;
            case DT_ADAPTATION_CAT16:
                make_RGB_to_CAT16_LMS(RGB_to_XYZ, RGB_to_LMS);
                make_CAT16_LMS_to_XYZ(p.MIX, MIX_to_XYZ);
                break;
            case DT_ADAPTATION_XYZ:
                dt_colormatrix_copy(RGB_to_LMS, RGB_to_XYZ);
                dt_colormatrix_copy(MIX_to_XYZ, p.MIX);
                break;
            case DT_ADAPTATION_RGB:
            case DT_ADAPTATION_LAST:
            default:
                dt_colormatrix_mul(MIX_to_XYZ, RGB_to_XYZ, p.MIX);
                break;
        }

        float minval = p.clip ? 0.0f : -FLT_MAX;
        AlignedPixel min_value = { minval, minval, minval, minval };

        Colormatrix RGB_to_XYZ_trans, RGB_to_LMS_trans, MIX_to_XYZ_trans, XYZ_to_RGB_trans;
        dt_colormatrix_transpose(RGB_to_XYZ_trans, RGB_to_XYZ);
        dt_colormatrix_transpose(RGB_to_LMS_trans, RGB_to_LMS);
        dt_colormatrix_transpose(MIX_to_XYZ_trans, MIX_to_XYZ);
        dt_colormatrix_transpose(XYZ_to_RGB_trans, XYZ_to_RGB);

        size_t count = in.count() / 4;
        const float* input = in.data();
        float* output = out.data();

        #pragma omp parallel for
        for(size_t k = 0; k < count; k++) {
            AlignedPixel temp_one, temp_two;
            dt_vector_max_nan(temp_two, input + k*4, min_value);

            switch(p.adaptation) {
                case DT_ADAPTATION_LINEAR_BRADFORD:
                    dt_apply_transposed_color_matrix(temp_two, RGB_to_LMS_trans, temp_one);
                    bradford_adapt_D50(temp_one, p.illuminant, p.p, 0, temp_two);
                    break;
                case DT_ADAPTATION_FULL_BRADFORD:
                    dt_apply_transposed_color_matrix(temp_two, RGB_to_XYZ_trans, temp_one);
                    {
                        float Y = temp_one[1];
                        convert_XYZ_to_bradford_LMS(temp_one, temp_two);
                        temp_two[0] /= Y; temp_two[1] /= Y; temp_two[2] /= Y;
                        bradford_adapt_D50(temp_two, p.illuminant, p.p, 1, temp_one);
                        temp_one[0] *= Y; temp_one[1] *= Y; temp_one[2] *= Y;
                        copy_pixel(temp_two, temp_one);
                    }
                    break;
                case DT_ADAPTATION_CAT16:
                    dt_apply_transposed_color_matrix(temp_two, RGB_to_LMS_trans, temp_one);
                    CAT16_adapt_D50(temp_one, p.illuminant, 1.0f, 1, temp_two);
                    break;
                case DT_ADAPTATION_XYZ:
                    dt_apply_transposed_color_matrix(temp_two, RGB_to_XYZ_trans, temp_one);
                    {
                        const AlignedPixel D50 = { 0.9642119944211994f, 1.0f, 0.8251882845188288f, 0.f };
                        temp_two[0] = temp_one[0] * D50[0] / p.illuminant[0];
                        temp_two[1] = temp_one[1] * D50[1] / p.illuminant[1];
                        temp_two[2] = temp_one[2] * D50[2] / p.illuminant[2];
                    }
                    break;
                case DT_ADAPTATION_RGB:
                default:
                    for(int c=0; c<4; c++) temp_one[c] = 0.0f;
            }

            dt_apply_transposed_color_matrix(temp_two, MIX_to_XYZ_trans, temp_one);
            if(p.clip) dt_vector_clipneg_nan(temp_one);
            _gamut_mapping(temp_one, p.gamut, p.clip, temp_two);

            switch(p.adaptation) {
                case DT_ADAPTATION_FULL_BRADFORD:
                case DT_ADAPTATION_LINEAR_BRADFORD:
                case DT_ADAPTATION_CAT16:
                case DT_ADAPTATION_XYZ:
                    convert_any_XYZ_to_LMS(temp_two, temp_one, p.adaptation);
                    break;
                default:
                    dt_apply_transposed_color_matrix(temp_two, RGB_to_XYZ_trans, temp_one);
                    break;
            }

            if(p.clip) dt_vector_clipneg_nan(temp_one);
            _luma_chroma(temp_one, p.saturation, p.lightness, temp_two, p.version);
            if(p.clip) dt_vector_clipneg_nan(temp_two);

            if(p.apply_grey) {
                float grey_mix = std::max(scalar_product(temp_two, p.grey), 0.0f);
                temp_two[0] = temp_two[1] = temp_two[2] = grey_mix;
            } else {
                switch(p.adaptation) {
                    case DT_ADAPTATION_FULL_BRADFORD:
                    case DT_ADAPTATION_LINEAR_BRADFORD:
                    case DT_ADAPTATION_CAT16:
                    case DT_ADAPTATION_XYZ:
                        convert_any_LMS_to_XYZ(temp_two, temp_one, p.adaptation);
                        break;
                    default:
                        dt_apply_transposed_color_matrix(temp_two, RGB_to_XYZ_trans, temp_one);
                        break;
                }
                if(p.clip) dt_vector_clipneg_nan(temp_one);
                dt_apply_transposed_color_matrix(temp_one, XYZ_to_RGB_trans, temp_two);
                if(p.clip) dt_vector_clipneg_nan(temp_two);
            }

            output[k*4+0] = temp_two[0];
            output[k*4+1] = temp_two[1];
            output[k*4+2] = temp_two[2];
            output[k*4+3] = input[k*4+3];
        }
    }

}
