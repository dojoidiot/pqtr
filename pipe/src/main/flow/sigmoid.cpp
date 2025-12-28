// sigmoid.cpp - Scene-referred tone mapping
//
// CLEAN COPY from darktable src/iop/sigmoid.c
// Generalized log-logistic sigmoid for scene-referred display transform.

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <algorithm>

namespace flow
{

// CLEAN COPY from DT sigmoid.c line 37
static constexpr float MIDDLE_GREY = 0.1845f;

// CLEAN COPY from DT sigmoid.c lines 299-316
// _generalized_loglogistic_sigmoid
static inline float generalized_loglogistic_sigmoid(const float value,
                                                     const float magnitude,
                                                     const float paper_exp,
                                                     const float film_fog,
                                                     const float film_power,
                                                     const float paper_power)
{
    const float clamped_value = std::max(value, 0.0f);
    // The following equation can be derived as a model for film + paper but it has a pole at 0
    // magnitude * powf(1.0f + paper_exp * powf(film_fog + value, -film_power), -paper_power);
    // Rewritten on a stable around zero form:
    const float film_response = std::pow(film_fog + clamped_value, film_power);
    const float paper_response = magnitude * std::pow(film_response / (paper_exp + film_response), paper_power);

    // Safety check for very large floats that cause numerical errors
    return std::isnan(paper_response) ? magnitude : paper_response;
}

// CLEAN COPY from DT sigmoid.c lines 494-504
// _desaturate_negative_values
static inline void desaturate_negative_values(const float pix_in[3], float pix_out[3])
{
    const float pixel_average = std::max((pix_in[0] + pix_in[1] + pix_in[2]) / 3.0f, 0.0f);
    const float min_value = std::min({pix_in[0], pix_in[1], pix_in[2]});
    const float saturation_factor = min_value < 0.0f ? -pixel_average / (min_value - pixel_average) : 1.0f;
    for (int c = 0; c < 3; c++)
    {
        pix_out[c] = pixel_average + saturation_factor * (pix_in[c] - pixel_average);
    }
}

// CLEAN COPY from DT sigmoid.c lines 508-514
struct ValueOrder { size_t min, mid, max; };

// CLEAN COPY from DT sigmoid.c lines 516-548
// _pixel_channel_order
static void pixel_channel_order(const float pix_in[3], ValueOrder& order)
{
    if (pix_in[0] >= pix_in[1])
    {
        if (pix_in[1] > pix_in[2])
        {   // Case 1: r >= g > b
            order.max = 0; order.mid = 1; order.min = 2;
        }
        else if (pix_in[2] > pix_in[0])
        {   // Case 2: b > r >= g
            order.max = 2; order.mid = 0; order.min = 1;
        }
        else if (pix_in[2] > pix_in[1])
        {   // Case 3: r >= b > g
            order.max = 0; order.mid = 2; order.min = 1;
        }
        else
        {   // Case 4: r >= g == b
            order.max = 0; order.mid = 1; order.min = 2;
        }
    }
    else
    {
        if (pix_in[0] > pix_in[2])
        {   // Case 5: g > r > b
            order.max = 1; order.mid = 0; order.min = 2;
        }
        else if (pix_in[2] > pix_in[1])
        {   // Case 6: b > g > r
            order.max = 2; order.mid = 1; order.min = 0;
        }
        else
        {   // Case 7: g >= b >= r
            order.max = 1; order.mid = 2; order.min = 0;
        }
    }
}

// CLEAN COPY from DT sigmoid.c lines 657-700
// _preserve_hue_and_energy
static inline void preserve_hue_and_energy(const float pix_in[3],
                                           const float per_channel[3],
                                           float pix_out[3],
                                           const ValueOrder& order,
                                           const float hue_preservation)
{
    // Naive Hue correction of the middle channel
    const float chroma = pix_in[order.max] - pix_in[order.min];
    const float midscale = chroma != 0.f ? (pix_in[order.mid] - pix_in[order.min]) / chroma : 0.f;
    const float full_hue_correction
        = per_channel[order.min] + (per_channel[order.max] - per_channel[order.min]) * midscale;
    const float naive_hue_mid
        = (1.0f - hue_preservation) * per_channel[order.mid] + hue_preservation * full_hue_correction;

    const float per_channel_energy = per_channel[0] + per_channel[1] + per_channel[2];
    const float naive_hue_energy = per_channel[order.min] + naive_hue_mid + per_channel[order.max];
    const float pix_in_min_plus_mid = pix_in[order.min] + pix_in[order.mid];
    const float blend_factor = pix_in_min_plus_mid != 0.f ? 2.0f * pix_in[order.min] / pix_in_min_plus_mid : 0.f;
    const float energy_target = blend_factor * per_channel_energy + (1.0f - blend_factor) * naive_hue_energy;

    // Preserve hue constrained to maintain the same energy as the per channel result
    if (naive_hue_mid <= per_channel[order.mid])
    {
        const float corrected_mid = ((1.0f - hue_preservation) * per_channel[order.mid]
                                     + hue_preservation
                                           * (midscale * per_channel[order.max]
                                              + (1.0f - midscale) * (energy_target - per_channel[order.max])))
                                    / (1.0f + hue_preservation * (1.0f - midscale));
        pix_out[order.min] = energy_target - per_channel[order.max] - corrected_mid;
        pix_out[order.mid] = corrected_mid;
        pix_out[order.max] = per_channel[order.max];
    }
    else
    {
        const float corrected_mid = ((1.0f - hue_preservation) * per_channel[order.mid]
                                     + hue_preservation
                                           * (per_channel[order.min] * (1.0f - midscale)
                                              + midscale * (energy_target - per_channel[order.min])))
                                    / (1.0f + hue_preservation * midscale);
        pix_out[order.min] = per_channel[order.min];
        pix_out[order.mid] = corrected_mid;
        pix_out[order.max] = energy_target - per_channel[order.min] - corrected_mid;
    }
}

class SigmoidImpl : public Sigmoid
{
    // User params (from XMP)
    float contrast_ = 1.5f;           // middle_grey_contrast
    float skewness_ = 0.0f;           // contrast_skewness
    float white_target_ = 1.0f;       // display_white_target (normalized)
    float black_target_ = 0.0152f;    // display_black_target
    float hue_preservation_ = 1.0f;   // hue_preservation (0-1, default 100%)

    // Computed params (from commit_params)
    float paper_power_ = 1.0f;
    float film_power_ = 1.5f;
    float paper_exposure_ = 1.0f;
    float film_fog_ = 0.0f;

    void compute_params()
    {
        // CLEAN COPY from DT sigmoid.c commit_params lines 318-392
        // Calculate actual skew log logistic parameters to fulfill the following:
        // f(scene_zero) = display_black_target
        // f(scene_grey) = MIDDLE_GREY
        // f(scene_inf)  = display_white_target
        // Slope at scene_grey independent of skewness i.e. only changed by the contrast parameter.

        // Calculate a reference slope for no skew and a normalized display
        const float ref_film_power = contrast_;
        const float ref_paper_power = 1.0f;
        const float ref_magnitude = 1.0f;
        const float ref_film_fog = 0.0f;
        const float ref_paper_exposure
            = std::pow(ref_film_fog + MIDDLE_GREY, ref_film_power) * ((ref_magnitude / MIDDLE_GREY) - 1.0f);
        const float delta = 1e-6f;
        const float ref_slope
            = (generalized_loglogistic_sigmoid(MIDDLE_GREY + delta, ref_magnitude, ref_paper_exposure, ref_film_fog,
                                               ref_film_power, ref_paper_power)
               - generalized_loglogistic_sigmoid(MIDDLE_GREY - delta, ref_magnitude, ref_paper_exposure, ref_film_fog,
                                                 ref_film_power, ref_paper_power))
              / 2.0f / delta;

        // Add skew
        paper_power_ = std::pow(5.0f, -skewness_);

        // Slope at low film power
        const float temp_film_power = 1.0f;
        const float temp_white_target = white_target_;
        const float temp_white_grey_relation
            = std::pow(temp_white_target / MIDDLE_GREY, 1.0f / paper_power_) - 1.0f;
        const float temp_paper_exposure = std::pow(MIDDLE_GREY, temp_film_power) * temp_white_grey_relation;
        const float temp_slope
            = (generalized_loglogistic_sigmoid(MIDDLE_GREY + delta, temp_white_target, temp_paper_exposure,
                                               ref_film_fog, temp_film_power, paper_power_)
               - generalized_loglogistic_sigmoid(MIDDLE_GREY - delta, temp_white_target, temp_paper_exposure,
                                                 ref_film_fog, temp_film_power, paper_power_))
              / 2.0f / delta;

        // Figure out what film power fulfills the target slope
        // (linear when assuming display_black = 0.0)
        film_power_ = ref_slope / temp_slope;

        // Calculate the other parameters now that both film and paper power is known
        const float white_grey_relation
            = std::pow(white_target_ / MIDDLE_GREY, 1.0f / paper_power_) - 1.0f;
        const float white_black_relation
            = std::pow(black_target_ / white_target_, -1.0f / paper_power_) - 1.0f;

        film_fog_ = MIDDLE_GREY * std::pow(white_grey_relation, 1.0f / film_power_)
                    / (std::pow(white_black_relation, 1.0f / film_power_)
                       - std::pow(white_grey_relation, 1.0f / film_power_));
        paper_exposure_
            = std::pow(film_fog_ + MIDDLE_GREY, film_power_) * white_grey_relation;
    }

public:
    std::string name() const override { return "sigmoid"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setParams(float contrast, float white_target, float black_target, float hue_preservation = 100.0f) override
    {
        contrast_ = contrast;
        skewness_ = 0.0f;  // Default, could be extended
        white_target_ = white_target / 100.0f;  // Convert from percentage
        black_target_ = black_target;
        hue_preservation_ = std::min(std::max(0.01f * hue_preservation, 0.0f), 1.0f);  // Convert % to 0-1
        compute_params();
    }

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();

        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());
        size_t npixels = static_cast<size_t>(width) * height;

        float* rgb = flow.rgb();

        // CLEAN COPY from DT sigmoid.c process_loglogistic_per_channel lines 727-760
        // With hue preservation from lines 750-756
        // Simplified: no primaries adjustment (base_primaries = WORK_PROFILE with no inset/rotation)
        for (size_t k = 0; k < npixels; k++)
        {
            size_t idx = k * 4;
            float pix_in[3] = {rgb[idx + 0], rgb[idx + 1], rgb[idx + 2]};
            float pix_strict_positive[3];

            // Force negative values to zero
            desaturate_negative_values(pix_in, pix_strict_positive);

            // Apply per-channel sigmoid (in "rendering" space, but with default params it's identity)
            float per_channel[3];
            for (int c = 0; c < 3; c++)
            {
                per_channel[c] = generalized_loglogistic_sigmoid(
                    pix_strict_positive[c], white_target_, paper_exposure_,
                    film_fog_, film_power_, paper_power_);
            }

            // Hue correction by scaling the middle value relative to max and min
            ValueOrder order;
            pixel_channel_order(pix_strict_positive, order);
            float pix_out[3];
            preserve_hue_and_energy(pix_strict_positive, per_channel, pix_out, order, hue_preservation_);

            rgb[idx + 0] = pix_out[0];
            rgb[idx + 1] = pix_out[1];
            rgb[idx + 2] = pix_out[2];
        }
    }
};

std::unique_ptr<Sigmoid> makeSigmoid()
{
    return std::make_unique<SigmoidImpl>();
}

} // namespace flow
