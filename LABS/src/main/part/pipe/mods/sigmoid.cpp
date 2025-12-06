// sigmoid.cpp
// Darktable-compatible sigmoid tone mapping
// Implements the generalized log-logistic sigmoid from darktable's scene-referred workflow
//
// Reference: darktable/src/iop/sigmoid.c (GPL v3)
// Algorithm: Film + paper response model with configurable contrast and skew
//
// The sigmoid maps scene-linear values to display-referred values:
//   - Middle grey (0.1845) maps to middle grey
//   - Smooth roll-off in highlights (no hard clipping)
//   - Controlled lift in shadows
//   - Hue-preserving via luminance-ratio scaling

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace pipe
{
namespace mods
{

// Darktable's middle grey reference
static constexpr float MIDDLE_GREY = 0.1845f;

// Generalized log-logistic sigmoid (darktable's core function)
// Models film + paper response for natural tone compression
static inline float loglogistic_sigmoid(
    float value,
    float magnitude,
    float paper_exp,
    float film_fog,
    float film_power,
    float paper_power)
{
    const float clamped = std::max(value, 0.0f);
    const float film_response = std::pow(film_fog + clamped, film_power);
    const float paper_response = magnitude * std::pow(
        film_response / (paper_exp + film_response), paper_power);

    // Safety check for numerical errors
    return std::isnan(paper_response) ? magnitude : paper_response;
}

// Calculate sigmoid parameters from user-friendly inputs
// This matches darktable's commit_params() logic
static void compute_sigmoid_params(
    float contrast,        // 0.1 - 10.0, default 1.5
    float skewness,        // -1.0 - 1.0, default 0.0
    float white_target,    // 0.0 - 1.0, default 1.0 (100%)
    float black_target,    // 0.0 - 0.15, default 0.000152 (0.0152%)
    float& film_power,
    float& paper_power,
    float& paper_exp,
    float& film_fog)
{
    // Calculate reference slope for no skew, normalized display
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

    // Add skew via paper_power
    paper_power = std::pow(5.0f, -skewness);

    // Compute slope at low film power
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

    // Film power to match target slope
    film_power = ref_slope / temp_slope;

    // Calculate remaining parameters
    const float white_grey_relation = std::pow(white_target / MIDDLE_GREY,
                                               1.0f / paper_power) - 1.0f;
    const float white_black_relation = std::pow(black_target / white_target,
                                                -1.0f / paper_power) - 1.0f;

    film_fog = MIDDLE_GREY * std::pow(white_grey_relation, 1.0f / film_power)
               / (std::pow(white_black_relation, 1.0f / film_power)
                  - std::pow(white_grey_relation, 1.0f / film_power));

    paper_exp = std::pow(film_fog + MIDDLE_GREY, film_power) * white_grey_relation;
}

// Apply sigmoid tone mapping (darktable scene-referred default)
// Input:  CV_32FC3 scene-linear RGB
// Output: CV_32FC3 display-referred RGB (before gamma)
//
// Uses RGB ratio method (hue-preserving):
//   1. Compute average luminance
//   2. Apply sigmoid to luminance
//   3. Scale RGB proportionally
//
// Parameters (matching darktable defaults):
//   contrast:     1.5 (controls curve steepness)
//   skewness:     0.0 (shifts contrast to shadows/highlights)
//   white_target: 1.0 (display white level)
//   black_target: 0.000152 (display black level, 0.0152%)
bool sigmoid(
    const cv::UMat& input,
    cv::UMat& output,
    float contrast,
    float skewness,
    float white_target,
    float black_target)
{
    if (input.empty() || input.type() != CV_32FC3)
    {
        std::cerr << "[Sigmoid] Error: Input must be non-empty CV_32FC3\n";
        return false;
    }

    // Clamp parameters to valid ranges
    contrast = std::clamp(contrast, 0.1f, 10.0f);
    skewness = std::clamp(skewness, -1.0f, 1.0f);
    white_target = std::clamp(white_target, 0.5f, 1.6f);
    black_target = std::clamp(black_target, 0.0f, 0.15f);

    // Compute sigmoid curve parameters
    float film_power, paper_power, paper_exp, film_fog;
    compute_sigmoid_params(contrast, skewness, white_target, black_target,
                          film_power, paper_power, paper_exp, film_fog);

    try
    {
        cv::Mat cpu;
        input.copyTo(cpu);

        const int rows = cpu.rows;
        const int cols = cpu.cols;

        // Process each pixel
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
                float sat_factor = 1.0f;
                if (min_val < 0.0f)
                {
                    sat_factor = -avg / (min_val - avg);
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
                // Uses hyperbolic compression to preserve hue
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

                // Clamp to display range
                ptr[x * 3 + 0] = std::clamp(b, 0.0f, 1.0f);
                ptr[x * 3 + 1] = std::clamp(g, 0.0f, 1.0f);
                ptr[x * 3 + 2] = std::clamp(r, 0.0f, 1.0f);
            }
        }

        cpu.copyTo(output);
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Sigmoid] Error: " << e.what() << "\n";
        return false;
    }
}

// Convenience: apply with darktable defaults
// Parameters decoded from darktable XMP (scene-referred workflow)
bool sigmoid_default(const cv::UMat& input, cv::UMat& output)
{
    return sigmoid(input, output,
                   1.5f,      // contrast (darktable default)
                   0.0f,      // skewness (neutral)
                   1.0f,      // white_target (100%)
                   0.0152f    // black_target (1.52% - decoded from darktable XMP)
    );
}

} // namespace mods
} // namespace pipe
