// sigmoid.cpp - VIBE
// Darktable-compatible sigmoid tone mapping
// Generalized log-logistic sigmoid from darktable's scene-referred workflow

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace vibe
{
namespace mods
{

static constexpr float MIDDLE_GREY = 0.1845f;

// Generalized log-logistic sigmoid (darktable's core function)
static inline float loglogistic_sigmoid(
    float value, float magnitude, float paper_exp,
    float film_fog, float film_power, float paper_power)
{
    const float clamped = std::max(value, 0.0f);
    const float film_response = std::pow(film_fog + clamped, film_power);
    const float paper_response = magnitude * std::pow(
        film_response / (paper_exp + film_response), paper_power);
    return std::isnan(paper_response) ? magnitude : paper_response;
}

// Calculate sigmoid parameters from user-friendly inputs
static void compute_sigmoid_params(
    float contrast, float skewness, float white_target, float black_target,
    float& film_power, float& paper_power, float& paper_exp, float& film_fog)
{
    const float ref_film_power = contrast;
    const float ref_paper_power = 1.0f;
    const float ref_magnitude = 1.0f;
    const float ref_film_fog = 0.0f;
    const float ref_paper_exp = std::pow(ref_film_fog + MIDDLE_GREY, ref_film_power)
                                 * ((ref_magnitude / MIDDLE_GREY) - 1.0f);

    const float delta = 1e-6f;
    const float ref_slope = (
        loglogistic_sigmoid(MIDDLE_GREY + delta, ref_magnitude, ref_paper_exp,
                           ref_film_fog, ref_film_power, ref_paper_power) -
        loglogistic_sigmoid(MIDDLE_GREY - delta, ref_magnitude, ref_paper_exp,
                           ref_film_fog, ref_film_power, ref_paper_power)
    ) / (2.0f * delta);

    paper_power = std::pow(5.0f, -skewness);

    const float temp_film_power = 1.0f;
    const float temp_white_grey_relation = std::pow(white_target / MIDDLE_GREY,
                                                     1.0f / paper_power) - 1.0f;
    const float temp_paper_exp = std::pow(MIDDLE_GREY, temp_film_power)
                                  * temp_white_grey_relation;
    const float temp_slope = (
        loglogistic_sigmoid(MIDDLE_GREY + delta, white_target, temp_paper_exp,
                           ref_film_fog, temp_film_power, paper_power) -
        loglogistic_sigmoid(MIDDLE_GREY - delta, white_target, temp_paper_exp,
                           ref_film_fog, temp_film_power, paper_power)
    ) / (2.0f * delta);

    film_power = ref_slope / temp_slope;

    const float white_grey_relation = std::pow(white_target / MIDDLE_GREY,
                                               1.0f / paper_power) - 1.0f;
    const float white_black_relation = std::pow(black_target / white_target,
                                                -1.0f / paper_power) - 1.0f;

    film_fog = MIDDLE_GREY * std::pow(white_grey_relation, 1.0f / film_power)
               / (std::pow(white_black_relation, 1.0f / film_power)
                  - std::pow(white_grey_relation, 1.0f / film_power));

    paper_exp = std::pow(film_fog + MIDDLE_GREY, film_power) * white_grey_relation;
}

bool sigmoid(const View& in, View& out,
    float contrast, float skewness, float white_target, float black_target)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::sigmoid] invalid input\n";
        return false;
    }

    contrast = std::clamp(contrast, 0.1f, 10.0f);
    skewness = std::clamp(skewness, -1.0f, 1.0f);
    white_target = std::clamp(white_target, 0.5f, 1.6f);
    black_target = std::clamp(black_target, 0.0f, 0.15f);

    float film_power, paper_power, paper_exp, film_fog;
    compute_sigmoid_params(contrast, skewness, white_target, black_target,
                          film_power, paper_power, paper_exp, film_fog);

    cv::Mat cpu;
    in.copyTo(cpu);

    const int rows = cpu.rows;
    const int cols = cpu.cols;

    for (int y = 0; y < rows; y++)
    {
        float* ptr = cpu.ptr<float>(y);
        for (int x = 0; x < cols; x++)
        {
            float b = ptr[x * 3 + 0];
            float g = ptr[x * 3 + 1];
            float r = ptr[x * 3 + 2];

            // Desaturate negative values toward average
            const float avg = std::max((r + g + b) / 3.0f, 0.0f);
            const float min_val = std::min({r, g, b});
            if (min_val < 0.0f)
            {
                float sat_factor = -avg / (min_val - avg);
                r = avg + sat_factor * (r - avg);
                g = avg + sat_factor * (g - avg);
                b = avg + sat_factor * (b - avg);
            }

            // RGB ratio method: apply sigmoid to average, scale channels
            const float luma = (r + g + b) / 3.0f;
            const float mapped_luma = loglogistic_sigmoid(
                luma, white_target, paper_exp, film_fog, film_power, paper_power);

            if (luma > 1e-9f)
            {
                const float scale = mapped_luma / luma;
                r *= scale;
                g *= scale;
                b *= scale;
            }
            else
            {
                r = g = b = mapped_luma;
            }

            // Gamut compression for out-of-range values
            const float pixel_min = std::min({r, g, b});
            const float pixel_max = std::max({r, g, b});

            const float epsilon = 1e-6f;
            const float display_border_white = (white_target - mapped_luma)
                / (pixel_max - mapped_luma + epsilon);
            const float display_border_black = (black_target - mapped_luma)
                / (pixel_min - mapped_luma - epsilon);
            const float display_border = std::min(display_border_white, display_border_black);
            const float chroma_border = (mapped_luma - pixel_min) / (mapped_luma + epsilon);

            const float chroma_adj = 1.0f / (chroma_border * display_border + epsilon);
            const float hyper_chroma = 2.0f * chroma_border
                / (1.0f - chroma_border * chroma_border + epsilon) * chroma_adj;

            const float hyper_z = std::sqrt(hyper_chroma * hyper_chroma + 1.0f);
            const float chroma_factor = hyper_chroma / (1.0f + hyper_z) * display_border;

            r = mapped_luma + chroma_factor * (r - mapped_luma);
            g = mapped_luma + chroma_factor * (g - mapped_luma);
            b = mapped_luma + chroma_factor * (b - mapped_luma);

            ptr[x * 3 + 0] = std::clamp(b, 0.0f, 1.0f);
            ptr[x * 3 + 1] = std::clamp(g, 0.0f, 1.0f);
            ptr[x * 3 + 2] = std::clamp(r, 0.0f, 1.0f);
        }
    }

    cpu.copyTo(out);
    return true;
}

bool sigmoid_default(const View& in, View& out)
{
    return sigmoid(in, out, 1.5f, 0.0f, 1.0f, 0.0152f);
}

} // namespace mods
} // namespace vibe
