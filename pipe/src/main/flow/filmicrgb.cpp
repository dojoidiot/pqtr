// filmicrgb.cpp - Scene-referred filmic tone mapping
//
// CLEAN COPY from DT iop/filmicrgb.c
// Key functions:
//   log_tonemapping_v2_1ch() - line 907
//   filmic_spline() - line 947
//   dt_iop_filmic_rgb_compute_spline() - line 2750
//   filmic_chroma_v2_v3() - line 1551

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace flow
{

// Constants from DT
static constexpr float NORM_MIN = 1.52587890625e-05f;  // 2^(-16)
static constexpr float SAFETY_MARGIN = 0.01f;

// Curve types from DT
enum CurveType
{
    CURVE_POLY_4 = 0,  // hard
    CURVE_POLY_3 = 1,  // soft
    CURVE_RATIONAL = 2 // safe
};

// Spline structure - CLEAN COPY from line 145
struct FilmicSpline
{
    float M1[3], M2[3], M3[3], M4[3], M5[3];
    float latitude_min, latitude_max;
    float x[5], y[5];
    CurveType type[2];
};

// Gaussian elimination - CLEAN COPY from iop/gaussian_elimination.h
static void gauss_solve(double* A, double* b, int n)
{
    for (int col = 0; col < n; col++)
    {
        // Find pivot
        int pivot = col;
        for (int row = col + 1; row < n; row++)
            if (std::abs(A[row * n + col]) > std::abs(A[pivot * n + col]))
                pivot = row;

        // Swap rows
        if (pivot != col)
        {
            for (int j = 0; j < n; j++)
                std::swap(A[col * n + j], A[pivot * n + j]);
            std::swap(b[col], b[pivot]);
        }

        // Eliminate
        for (int row = col + 1; row < n; row++)
        {
            double f = A[row * n + col] / A[col * n + col];
            for (int j = col; j < n; j++)
                A[row * n + j] -= f * A[col * n + j];
            b[row] -= f * b[col];
        }
    }

    // Back substitution
    for (int row = n - 1; row >= 0; row--)
    {
        for (int j = row + 1; j < n; j++)
            b[row] -= A[row * n + j] * b[j];
        b[row] /= A[row * n + row];
    }
}

// log_tonemapping_v2_1ch - CLEAN COPY from line 907
static inline float log_tonemapping_v2_1ch(float x, float grey, float black, float dynamic_range)
{
    float temp = (std::log2(x / grey) - black) / dynamic_range;
    return std::max(0.0f, std::min(1.0f, temp));
}

// filmic_spline - CLEAN COPY from line 947
static inline float filmic_spline(float x, const float* M1, const float* M2,
                                   const float* M3, const float* M4, const float* M5,
                                   float latitude_min, float latitude_max,
                                   const CurveType type[2])
{
    float result;

    if (x < latitude_min)
    {
        // toe
        if (type[0] == CURVE_POLY_4)
            result = M1[0] + x * (M2[0] + x * (M3[0] + x * (M4[0] + x * M5[0])));
        else if (type[0] == CURVE_POLY_3)
            result = M1[0] + x * (M2[0] + x * (M3[0] + x * M4[0]));
        else
        {
            float xi = latitude_min - x;
            float rat = xi * (xi * M2[0] + 1.0f);
            result = M4[0] - M1[0] * rat / (rat + M3[0]);
        }
    }
    else if (x > latitude_max)
    {
        // shoulder
        if (type[1] == CURVE_POLY_4)
            result = M1[1] + x * (M2[1] + x * (M3[1] + x * (M4[1] + x * M5[1])));
        else if (type[1] == CURVE_POLY_3)
            result = M1[1] + x * (M2[1] + x * (M3[1] + x * M4[1]));
        else
        {
            float xi = x - latitude_max;
            float rat = xi * (xi * M2[1] + 1.0f);
            result = M4[1] + M1[1] * rat / (rat + M3[1]);
        }
    }
    else
    {
        // latitude (linear)
        result = M1[2] + x * M2[2];
    }

    return result;
}

// Compute spline coefficients - CLEAN COPY from line 2750
// Simplified for spline_version V3 with POLY_4 curves (default)
static void compute_spline(float grey_point_source, float black_point_source,
                           float white_point_source, float contrast,
                           float latitude_pct, float output_power,
                           float grey_point_target, float black_point_target,
                           float white_point_target, FilmicSpline& spline)
{
    // grey_display from output_power - line 2765
    float grey_display = std::pow(0.1845f, 1.0f / output_power);

    float dynamic_range = white_point_source - black_point_source;

    // log coordinates - line 2773
    float black_log = 0.0f;
    float grey_log = std::abs(black_point_source) / dynamic_range;
    float white_log = 1.0f;

    // display coordinates with output_power - line 2792
    float black_display = std::pow(std::max(0.0f, std::min(black_point_target / 100.0f, grey_point_target / 100.0f)),
                                    1.0f / output_power);
    float white_display = std::pow(std::max(white_point_target / 100.0f, grey_point_target / 100.0f),
                                    1.0f / output_power);

    // Spline version V3 computation - line 2828
    float hardness = output_power;
    float latitude = std::max(0.0f, std::min(100.0f, latitude_pct)) / 100.0f;
    float slope = contrast * dynamic_range / 8.0f;

    // min_contrast - line 2834
    float min_contrast = 1.0f;
    min_contrast = std::max(min_contrast, (white_display - grey_display) / (white_log - grey_log));
    min_contrast = std::max(min_contrast, (grey_display - black_display) / (grey_log - black_log));
    min_contrast += SAFETY_MARGIN;

    // contrast from slope - line 2847
    float actual_contrast = slope / (hardness * std::pow(grey_display, hardness - 1.0f));
    actual_contrast = std::max(min_contrast, std::min(100.0f, actual_contrast));

    // linear_intercept - line 2853
    float linear_intercept = grey_display - (actual_contrast * grey_log);

    // xmin, xmax - line 2859
    float xmin = (black_display + SAFETY_MARGIN * (white_display - black_display) - linear_intercept) / actual_contrast;
    float xmax = (white_display - SAFETY_MARGIN * (white_display - black_display) - linear_intercept) / actual_contrast;

    // toe_log, shoulder_log - line 2864
    float toe_log = (1.0f - latitude) * grey_log + latitude * xmin;
    float shoulder_log = (1.0f - latitude) * grey_log + latitude * xmax;

    // toe_display, shoulder_display - line 2877
    float toe_display = toe_log * actual_contrast + linear_intercept;
    float shoulder_display = shoulder_log * actual_contrast + linear_intercept;

    // Store nodes - line 2892
    spline.x[0] = black_log;
    spline.x[1] = toe_log;
    spline.x[2] = grey_log;
    spline.x[3] = shoulder_log;
    spline.x[4] = white_log;

    spline.y[0] = black_display;
    spline.y[1] = toe_display;
    spline.y[2] = grey_display;
    spline.y[3] = shoulder_display;
    spline.y[4] = white_display;

    spline.latitude_min = toe_log;
    spline.latitude_max = shoulder_log;

    // Default curve types: POLY_4 (hard)
    spline.type[0] = CURVE_POLY_4;
    spline.type[1] = CURVE_POLY_4;

    // Linear part - line 2933
    spline.M2[2] = actual_contrast;
    spline.M1[2] = toe_display - actual_contrast * toe_log;
    spline.M3[2] = spline.M4[2] = spline.M5[2] = 0.0f;

    // Toe polynomial (4th order) - line 2943
    double Tl = toe_log;
    double Tl2 = Tl * Tl, Tl3 = Tl2 * Tl, Tl4 = Tl3 * Tl;

    double A0[25] = {
        0., 0., 0., 0., 1.,
        0., 0., 0., 1., 0.,
        Tl4, Tl3, Tl2, Tl, 1.,
        4.*Tl3, 3.*Tl2, 2.*Tl, 1., 0.,
        12.*Tl2, 6.*Tl, 2., 0., 0.
    };
    double b0[5] = { black_display, 0., toe_display, actual_contrast, 0. };
    gauss_solve(A0, b0, 5);

    spline.M5[0] = static_cast<float>(b0[0]);
    spline.M4[0] = static_cast<float>(b0[1]);
    spline.M3[0] = static_cast<float>(b0[2]);
    spline.M2[0] = static_cast<float>(b0[3]);
    spline.M1[0] = static_cast<float>(b0[4]);

    // Shoulder polynomial (4th order) - line 3012
    double Sl = shoulder_log;
    double Sl2 = Sl * Sl, Sl3 = Sl2 * Sl, Sl4 = Sl3 * Sl;

    double A1[25] = {
        1., 1., 1., 1., 1.,
        4., 3., 2., 1., 0.,
        Sl4, Sl3, Sl2, Sl, 1.,
        4.*Sl3, 3.*Sl2, 2.*Sl, 1., 0.,
        12.*Sl2, 6.*Sl, 2., 0., 0.
    };
    double b1[5] = { white_display, 0., shoulder_display, actual_contrast, 0. };
    gauss_solve(A1, b1, 5);

    spline.M5[1] = static_cast<float>(b1[0]);
    spline.M4[1] = static_cast<float>(b1[1]);
    spline.M3[1] = static_cast<float>(b1[2]);
    spline.M2[1] = static_cast<float>(b1[3]);
    spline.M1[1] = static_cast<float>(b1[4]);
}

// Power norm from DT - line 837
static inline float pixel_rgb_norm_power(float r, float g, float b)
{
    const float power = 2.43f;  // sRGB power
    return std::pow((std::pow(std::max(0.0f, r), power) +
                     std::pow(std::max(0.0f, g), power) +
                     std::pow(std::max(0.0f, b), power)) / 3.0f,
                    1.0f / power);
}

class FilmicrgbImpl : public Filmicrgb
{
    float grey_source_ = 0.1845f;
    float black_source_ = -8.0f;
    float white_source_ = 4.0f;
    float dynamic_range_ = 12.0f;
    float output_power_ = 4.0f;
    float contrast_ = 1.0f;
    float latitude_ = 0.01f;
    FilmicSpline spline_;
    bool enabled_ = false;

public:
    std::string name() const override { return "filmicrgb"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setParams(float grey, float black_ev, float white_ev,
                   float contrast, float latitude, float hardness) override
    {
        grey_source_ = grey / 100.0f;  // convert from % to ratio
        black_source_ = black_ev;
        white_source_ = white_ev;
        dynamic_range_ = white_ev - black_ev;
        contrast_ = contrast;
        latitude_ = latitude;
        output_power_ = hardness;
        enabled_ = true;

        // Compute spline coefficients
        compute_spline(grey, black_ev, white_ev, contrast, latitude, hardness,
                       18.45f, 0.01517634f, 100.0f, spline_);
    }

    void process(Flow& flow) override
    {
        if (!enabled_) return;

        auto& root = flow.info().root();
        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());

        float* rgb = flow.rgb();
        if (!rgb) return;

        size_t npixels = static_cast<size_t>(width) * height;

        // filmic_chroma_v2_v3 - CLEAN COPY from line 1551
        for (size_t k = 0; k < npixels; k++)
        {
            float* pix = rgb + k * 4;

            // Get pixel norm (power norm) - line 1565
            float norm = std::max(pixel_rgb_norm_power(pix[0], pix[1], pix[2]), NORM_MIN);

            // Save ratios - line 1568
            float ratios[3] = { pix[0] / norm, pix[1] / norm, pix[2] / norm };

            // Sanitize ratios - line 1574
            float min_ratios = std::min(std::min(ratios[0], ratios[1]), ratios[2]);
            if (min_ratios < 0.0f)
            {
                ratios[0] -= min_ratios;
                ratios[1] -= min_ratios;
                ratios[2] -= min_ratios;
            }

            // Log tone-mapping - line 1582
            norm = log_tonemapping_v2_1ch(norm, grey_source_, black_source_, dynamic_range_);

            // Filmic S curve + output power - line 1589
            float curved = filmic_spline(norm, spline_.M1, spline_.M2, spline_.M3,
                                          spline_.M4, spline_.M5,
                                          spline_.latitude_min, spline_.latitude_max,
                                          spline_.type);
            norm = std::pow(std::max(0.0f, std::min(1.0f, curved)), output_power_);

            // Re-apply ratios - line 1594 (simplified, no desaturation for now)
            pix[0] = ratios[0] * norm;
            pix[1] = ratios[1] * norm;
            pix[2] = ratios[2] * norm;

            // Gamut mapping - line 1607
            float max_pix = std::max(std::max(pix[0], pix[1]), pix[2]);
            if (max_pix > 1.0f)
            {
                pix[0] = std::max(0.0f, std::min(1.0f, pix[0] / max_pix));
                pix[1] = std::max(0.0f, std::min(1.0f, pix[1] / max_pix));
                pix[2] = std::max(0.0f, std::min(1.0f, pix[2] / max_pix));
            }
        }
    }
};

std::unique_ptr<Filmicrgb> makeFilmicrgb()
{
    return std::make_unique<FilmicrgbImpl>();
}

} // namespace flow
