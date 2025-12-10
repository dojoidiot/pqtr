// theory.h - VIBE Test
// Industry-standard reference implementations based on ACES/VFX standards
// These MUST match the CV implementations exactly

#pragma once

#include <cmath>
#include <algorithm>
#include <vector>
#include <array>
#include <utility>

namespace theory {

// Image buffer: row-major BGR float [0,1]
struct Image {
    std::vector<float> data;
    int width, height;

    Image(int w, int h) : data(w * h * 3, 0.0f), width(w), height(h) {}

    float& at(int y, int x, int c) { return data[(y * width + x) * 3 + c]; }
    float at(int y, int x, int c) const { return data[(y * width + x) * 3 + c]; }

    void clamp() {
        for (auto& v : data) v = std::clamp(v, 0.0f, 1.0f);
    }
};

//------------------------------------------------------------------------------
// Exposure
// Theory: Linear scaling by 2^ev where ev = (dial - 0.5) * 8
//------------------------------------------------------------------------------
inline void exposure(const Image& in, Image& out, float dial) {
    float ev = (dial - 0.5f) * 8.0f;
    float mult = std::pow(2.0f, ev);

    for (size_t i = 0; i < in.data.size(); i++)
        out.data[i] = in.data[i] * mult;
}

//------------------------------------------------------------------------------
// White Balance (Tanner Helland's Planckian Approximation)
// Same algorithm as CV implementation
//------------------------------------------------------------------------------
inline void kelvin_to_rgb(float kelvin, float& r, float& g, float& b) {
    float temp = kelvin / 100.0f;

    // Red
    if (temp <= 66.0f)
        r = 1.0f;
    else {
        r = temp - 60.0f;
        r = 329.698727446f * std::pow(r, -0.1332047592f);
        r = std::clamp(r / 255.0f, 0.0f, 1.0f);
    }

    // Green
    if (temp <= 66.0f) {
        g = temp;
        g = 99.4708025861f * std::log(g) - 161.1195681661f;
        g = std::clamp(g / 255.0f, 0.0f, 1.0f);
    } else {
        g = temp - 60.0f;
        g = 288.1221695283f * std::pow(g, -0.0755148492f);
        g = std::clamp(g / 255.0f, 0.0f, 1.0f);
    }

    // Blue
    if (temp >= 66.0f)
        b = 1.0f;
    else if (temp <= 19.0f)
        b = 0.0f;
    else {
        b = temp - 10.0f;
        b = 138.5177312231f * std::log(b) - 305.0447927307f;
        b = std::clamp(b / 255.0f, 0.0f, 1.0f);
    }
}

inline void white_balance(const Image& in, Image& out, float temp_dial, float tint_dial) {
    temp_dial = std::clamp(temp_dial, 0.0f, 1.0f);
    tint_dial = std::clamp(tint_dial, 0.0f, 1.0f);

    // Piecewise exponential Kelvin mapping
    float kelvin;
    if (temp_dial < 0.5f) {
        float t = temp_dial * 2.0f;
        kelvin = 2000.0f * std::pow(6500.0f / 2000.0f, t);
    } else {
        float t = (temp_dial - 0.5f) * 2.0f;
        kelvin = 6500.0f * std::pow(12000.0f / 6500.0f, t);
    }

    float tr, tg, tb;
    kelvin_to_rgb(kelvin, tr, tg, tb);

    // Normalize to green
    float norm = 1.0f / std::max(0.001f, tg);
    tr *= norm;
    tg = 1.0f;
    tb *= norm;

    // Tint (green/magenta)
    float tint_shift = (tint_dial - 0.5f) * 0.4f;
    float green_mult = 1.0f - tint_shift;
    float rb_mult = 1.0f + tint_shift * 0.5f;

    // Final multipliers (inverse)
    float r_mult = (1.0f / std::max(0.001f, tr)) * rb_mult;
    float g_mult = (1.0f / std::max(0.001f, tg)) * green_mult;
    float b_mult = (1.0f / std::max(0.001f, tb)) * rb_mult;

    // Normalize to preserve mid-gray
    float avg = (r_mult + g_mult + b_mult) / 3.0f;
    r_mult /= avg;
    g_mult /= avg;
    b_mult /= avg;

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            out.at(y, x, 0) = in.at(y, x, 0) * b_mult;
            out.at(y, x, 1) = in.at(y, x, 1) * g_mult;
            out.at(y, x, 2) = in.at(y, x, 2) * r_mult;
        }
    }
}

//------------------------------------------------------------------------------
// Sigmoid (darktable Generalized Log-Logistic)
// Exact match to CV implementation
//------------------------------------------------------------------------------
static constexpr float MIDDLE_GREY = 0.1845f;

inline float loglogistic_sigmoid(float value, float magnitude, float paper_exp,
                                  float film_fog, float film_power, float paper_power) {
    const float clamped = std::max(value, 0.0f);
    const float film_response = std::pow(film_fog + clamped, film_power);
    const float paper_response = magnitude * std::pow(
        film_response / (paper_exp + film_response), paper_power);
    return std::isnan(paper_response) ? magnitude : paper_response;
}

inline void compute_sigmoid_params(float contrast, float skewness, float white_target, float black_target,
                                    float& film_power, float& paper_power, float& paper_exp, float& film_fog) {
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
    const float temp_paper_exp = std::pow(MIDDLE_GREY, temp_film_power) * temp_white_grey_relation;
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

inline void sigmoid(const Image& in, Image& out,
                    float contrast, float skewness, float white_target, float black_target) {
    contrast = std::clamp(contrast, 0.1f, 10.0f);
    skewness = std::clamp(skewness, -1.0f, 1.0f);
    white_target = std::clamp(white_target, 0.5f, 1.6f);
    black_target = std::clamp(black_target, 0.0f, 0.15f);

    float film_power, paper_power, paper_exp, film_fog;
    compute_sigmoid_params(contrast, skewness, white_target, black_target,
                          film_power, paper_power, paper_exp, film_fog);

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float b = in.at(y, x, 0);
            float g = in.at(y, x, 1);
            float r = in.at(y, x, 2);

            // Desaturate negative values
            const float avg = std::max((r + g + b) / 3.0f, 0.0f);
            const float min_val = std::min({r, g, b});
            if (min_val < 0.0f) {
                float sat_factor = -avg / (min_val - avg);
                r = avg + sat_factor * (r - avg);
                g = avg + sat_factor * (g - avg);
                b = avg + sat_factor * (b - avg);
            }

            // RGB ratio method
            const float luma = (r + g + b) / 3.0f;
            const float mapped_luma = loglogistic_sigmoid(
                luma, white_target, paper_exp, film_fog, film_power, paper_power);

            if (luma > 1e-9f) {
                const float scale = mapped_luma / luma;
                r *= scale;
                g *= scale;
                b *= scale;
            } else {
                r = g = b = mapped_luma;
            }

            // Gamut compression
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

            out.at(y, x, 0) = std::clamp(b, 0.0f, 1.0f);
            out.at(y, x, 1) = std::clamp(g, 0.0f, 1.0f);
            out.at(y, x, 2) = std::clamp(r, 0.0f, 1.0f);
        }
    }
}

//------------------------------------------------------------------------------
// Tone Map (Extended Reinhard + Shadow/Highlight/Contrast)
// Matches CV implementation
//------------------------------------------------------------------------------
inline void tone_map(const Image& in, Image& out,
                     float contrast_dial, float highlights_dial, float shadows_dial,
                     float toe_pivot_dial, float shoulder_pivot_dial,
                     float white_point_dial, float black_point_dial) {
    // Convert dials
    float contrast = 0.5f * std::exp(contrast_dial * 1.792f);
    float highlights = (highlights_dial - 0.5f) * 2.0f;
    float shadows = (shadows_dial - 0.5f) * 2.0f;
    float toe_pivot = 0.1f + toe_pivot_dial * 0.4f;
    float shoulder_pivot = 0.5f + shoulder_pivot_dial * 0.4f;
    bool bypass_reinhard = (white_point_dial > 0.45f && white_point_dial < 0.55f);
    float white_point = 2.0f + white_point_dial * 4.0f;
    float black_point = (black_point_dial - 0.5f) * 0.5f;
    const float mask_steepness = 12.0f;

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float b = in.at(y, x, 0);
            float g = in.at(y, x, 1);
            float r = in.at(y, x, 2);

            // Rec.709 luminance
            float L = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            float L_orig = std::max(L, 0.0001f);

            // Black point
            if (std::abs(black_point) > 0.001f) {
                if (black_point > 0) {
                    L = std::max(L - black_point, 0.0f);
                    L *= 1.0f / (1.0f - black_point);
                } else {
                    float abs_bp = std::abs(black_point);
                    L = L * (1.0f - abs_bp) + abs_bp;
                }
            }

            // Extended Reinhard
            if (!bypass_reinhard) {
                float w2 = white_point * white_point;
                L = (L + L * L / w2) / (1.0f + L);
            }

            // Shadow adjustment
            if (std::abs(shadows) > 0.01f) {
                float shadow_mask = 1.0f / (1.0f + std::exp(mask_steepness * (L - toe_pivot)));
                float gamma = std::clamp(1.0f - shadows * 0.5f, 0.3f, 2.0f);
                float adjusted = std::pow(std::max(L + 0.001f, 0.001f), gamma);
                L = adjusted * shadow_mask + L * (1.0f - shadow_mask);
            }

            // Highlight adjustment
            if (std::abs(highlights) > 0.01f) {
                float highlight_mask = 1.0f / (1.0f + std::exp(-mask_steepness * (L - shoulder_pivot)));
                float gamma = std::clamp(1.0f + highlights * 0.5f, 0.3f, 2.0f);
                float inv_L = std::max(1.0f - L, 0.001f);
                float adjusted = 1.0f - std::pow(inv_L, gamma);
                L = adjusted * highlight_mask + L * (1.0f - highlight_mask);
            }

            // Contrast
            if (std::abs(contrast - 1.0f) > 0.01f) {
                float centered = L - 0.5f;
                float sign = (centered >= 0) ? 1.0f : -1.0f;
                float abs_val = std::abs(centered) * 2.0f + 0.001f;
                float powered = std::pow(abs_val, contrast) * 0.5f;
                L = sign * powered + 0.5f;
            }

            L = std::clamp(L, 0.0f, 1.0f);

            // Scale RGB by luminance ratio
            float scale = L / L_orig;
            out.at(y, x, 0) = std::clamp(b * scale, 0.0f, 1.0f);
            out.at(y, x, 1) = std::clamp(g * scale, 0.0f, 1.0f);
            out.at(y, x, 2) = std::clamp(r * scale, 0.0f, 1.0f);
        }
    }
}

//------------------------------------------------------------------------------
// Global Color (Lab-based Vibrance/Saturation)
// Note: CV uses 8-bit Lab conversion which causes quantization
//------------------------------------------------------------------------------
inline void rgb_to_lab(float r, float g, float b, float& L, float& a, float& lab_b) {
    // Gamma encode
    r = std::pow(std::clamp(r, 0.0f, 1.0f), 1.0f/2.2f);
    g = std::pow(std::clamp(g, 0.0f, 1.0f), 1.0f/2.2f);
    b = std::pow(std::clamp(b, 0.0f, 1.0f), 1.0f/2.2f);

    // sRGB to XYZ (D65)
    float X = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
    float Y = 0.2126729f * r + 0.7151522f * g + 0.0721750f * b;
    float Z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;

    // D65 reference white
    X /= 0.95047f;
    Y /= 1.00000f;
    Z /= 1.08883f;

    auto f = [](float t) {
        return (t > 0.008856f) ? std::cbrt(t) : (7.787f * t + 16.0f/116.0f);
    };

    float fx = f(X), fy = f(Y), fz = f(Z);
    L = 116.0f * fy - 16.0f;
    a = 500.0f * (fx - fy);
    lab_b = 200.0f * (fy - fz);
}

inline void lab_to_rgb(float L, float a, float lab_b, float& r, float& g, float& b) {
    float fy = (L + 16.0f) / 116.0f;
    float fx = a / 500.0f + fy;
    float fz = fy - lab_b / 200.0f;

    auto f_inv = [](float t) {
        return (t > 0.206893f) ? t * t * t : (t - 16.0f/116.0f) / 7.787f;
    };

    float X = f_inv(fx) * 0.95047f;
    float Y = f_inv(fy) * 1.00000f;
    float Z = f_inv(fz) * 1.08883f;

    // XYZ to sRGB
    r =  3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
    g = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
    b =  0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;

    // Gamma decode
    r = std::pow(std::clamp(r, 0.0f, 1.0f), 2.2f);
    g = std::pow(std::clamp(g, 0.0f, 1.0f), 2.2f);
    b = std::pow(std::clamp(b, 0.0f, 1.0f), 2.2f);
}

inline void global_color(const Image& in, Image& out,
                         float vibrance_dial, float saturation_dial, float density_dial) {
    float vibrance = (vibrance_dial - 0.5f) * 2.0f;
    float saturation = saturation_dial * 2.0f;
    float color_density = 0.5f + density_dial;

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float r = in.at(y, x, 2);
            float g = in.at(y, x, 1);
            float b = in.at(y, x, 0);

            float L, a, lab_b;
            rgb_to_lab(r, g, b, L, a, lab_b);

            // Vibrance with skin protection
            if (std::abs(vibrance) > 0.01f) {
                float C = std::sqrt(a * a + lab_b * lab_b);
                float hue = std::atan2(lab_b, a) * 180.0f / 3.14159265f;
                if (hue < 0) hue += 360.0f;

                // Skin hue ~45 degrees in Lab
                float skin_dist = hue - 45.0f;
                float skin_mask = std::exp(-(skin_dist * skin_dist) / 450.0f);

                float vib_weight = std::clamp(1.0f - C / 100.0f, 0.0f, 1.0f);

                if (vibrance > 0) {
                    float prot = (1.0f - skin_mask) * 0.7f + 0.3f;
                    vib_weight *= prot;
                }

                float boost = 1.0f + vib_weight * vibrance;
                a *= boost;
                lab_b *= boost;
            }

            // Saturation
            if (std::abs(saturation - 1.0f) > 0.01f) {
                a *= saturation;
                lab_b *= saturation;
            }

            // Color density
            if (std::abs(color_density - 1.0f) > 0.01f) {
                a *= color_density;
                lab_b *= color_density;
                float l_contrast = 1.0f + (color_density - 1.0f) * 0.3f;
                L = (L - 50.0f) * l_contrast + 50.0f;
            }

            L = std::clamp(L, 0.0f, 100.0f);

            lab_to_rgb(L, a, lab_b, r, g, b);
            out.at(y, x, 0) = std::clamp(b, 0.0f, 1.0f);
            out.at(y, x, 1) = std::clamp(g, 0.0f, 1.0f);
            out.at(y, x, 2) = std::clamp(r, 0.0f, 1.0f);
        }
    }
}

//------------------------------------------------------------------------------
// Geometric (Sequential crop → zoom → rotate)
// CV changes output dimensions to the cropped size!
//------------------------------------------------------------------------------
inline std::pair<int,int> geometric_output_size(int in_w, int in_h,
    float crop_top, float crop_right, float crop_bottom, float crop_left) {
    float ct = crop_top * 0.5f;
    float cr = crop_right * 0.5f;
    float cb = crop_bottom * 0.5f;
    float cl = crop_left * 0.5f;

    int w = std::max(1, in_w - int(in_w * (cl + cr)));
    int h = std::max(1, in_h - int(in_h * (ct + cb)));
    return {w, h};
}

inline void geometric(const Image& in, Image& out,
                      float crop_top, float crop_right, float crop_bottom, float crop_left,
                      float zoom_dial, float tilt_dial) {
    float ct = crop_top * 0.5f;
    float cr = crop_right * 0.5f;
    float cb = crop_bottom * 0.5f;
    float cl = crop_left * 0.5f;

    float zoom = std::pow(4.0f, zoom_dial - 0.5f);
    float angle = (tilt_dial - 0.5f) * 90.0f * 3.14159265f / 180.0f;
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);

    // Cropped region
    int cx = int(in.width * cl);
    int cy = int(in.height * ct);
    int cw = std::max(1, in.width - int(in.width * (cl + cr)));
    int ch = std::max(1, in.height - int(in.height * (ct + cb)));

    // Create intermediate cropped image
    Image cropped(cw, ch);
    for (int y = 0; y < ch; y++) {
        for (int x = 0; x < cw; x++) {
            int sy = std::clamp(cy + y, 0, in.height - 1);
            int sx = std::clamp(cx + x, 0, in.width - 1);
            cropped.at(y, x, 0) = in.at(sy, sx, 0);
            cropped.at(y, x, 1) = in.at(sy, sx, 1);
            cropped.at(y, x, 2) = in.at(sy, sx, 2);
        }
    }

    // Zoom
    Image zoomed(cw, ch);
    if (zoom > 1.0f) {
        // Crop center and resize up
        int nw = std::max(1, int(cw / zoom));
        int nh = std::max(1, int(ch / zoom));
        int ox = (cw - nw) / 2;
        int oy = (ch - nh) / 2;

        for (int y = 0; y < ch; y++) {
            for (int x = 0; x < cw; x++) {
                float sx = ox + float(x) * nw / cw;
                float sy = oy + float(y) * nh / ch;
                int x0 = std::clamp(int(sx), 0, cw - 2);
                int y0 = std::clamp(int(sy), 0, ch - 2);
                float fx = sx - x0, fy = sy - y0;

                for (int c = 0; c < 3; c++) {
                    float v00 = cropped.at(y0, x0, c);
                    float v01 = cropped.at(y0, std::min(x0+1, cw-1), c);
                    float v10 = cropped.at(std::min(y0+1, ch-1), x0, c);
                    float v11 = cropped.at(std::min(y0+1, ch-1), std::min(x0+1, cw-1), c);
                    zoomed.at(y, x, c) = (1-fx)*(1-fy)*v00 + fx*(1-fy)*v01 +
                                         (1-fx)*fy*v10 + fx*fy*v11;
                }
            }
        }
    } else {
        // Scale down and center on black (INTER_AREA style)
        int nw = std::max(1, int(cw * zoom));
        int nh = std::max(1, int(ch * zoom));
        int ox = (cw - nw) / 2;
        int oy = (ch - nh) / 2;

        for (int y = 0; y < ch; y++) {
            for (int x = 0; x < cw; x++) {
                if (x >= ox && x < ox + nw && y >= oy && y < oy + nh) {
                    float sx = float(x - ox) * cw / nw;
                    float sy = float(y - oy) * ch / nh;
                    int x0 = std::clamp(int(sx), 0, cw - 2);
                    int y0 = std::clamp(int(sy), 0, ch - 2);
                    float fx = sx - x0, fy = sy - y0;

                    for (int c = 0; c < 3; c++) {
                        float v00 = cropped.at(y0, x0, c);
                        float v01 = cropped.at(y0, std::min(x0+1, cw-1), c);
                        float v10 = cropped.at(std::min(y0+1, ch-1), x0, c);
                        float v11 = cropped.at(std::min(y0+1, ch-1), std::min(x0+1, cw-1), c);
                        zoomed.at(y, x, c) = (1-fx)*(1-fy)*v00 + fx*(1-fy)*v01 +
                                             (1-fx)*fy*v10 + fx*fy*v11;
                    }
                } else {
                    zoomed.at(y, x, 0) = zoomed.at(y, x, 1) = zoomed.at(y, x, 2) = 0;
                }
            }
        }
    }

    // Rotate (BORDER_REPLICATE) - output is same size as cropped
    float center_x = cw * 0.5f;
    float center_y = ch * 0.5f;

    for (int y = 0; y < out.height; y++) {
        for (int x = 0; x < out.width; x++) {
            float dx = float(x) - center_x;
            float dy = float(y) - center_y;

            // Inverse rotation
            float sx = dx * cos_a + dy * sin_a + center_x;
            float sy = -dx * sin_a + dy * cos_a + center_y;

            // Clamp (BORDER_REPLICATE)
            sx = std::clamp(sx, 0.0f, float(cw - 1));
            sy = std::clamp(sy, 0.0f, float(ch - 1));

            int x0 = std::clamp(int(sx), 0, cw - 2);
            int y0 = std::clamp(int(sy), 0, ch - 2);
            float fx = sx - x0, fy = sy - y0;

            for (int c = 0; c < 3; c++) {
                float v00 = zoomed.at(y0, x0, c);
                float v01 = zoomed.at(y0, std::min(x0+1, cw-1), c);
                float v10 = zoomed.at(std::min(y0+1, ch-1), x0, c);
                float v11 = zoomed.at(std::min(y0+1, ch-1), std::min(x0+1, cw-1), c);
                out.at(y, x, c) = (1-fx)*(1-fy)*v00 + fx*(1-fy)*v01 +
                                  (1-fx)*fy*v10 + fx*fy*v11;
            }
        }
    }
}

//------------------------------------------------------------------------------
// Selective Color (8 Hue Bands with Cosine Weighting)
//------------------------------------------------------------------------------
static const std::array<float, 8> HUE_CENTERS = {0.0f, 45.0f, 90.0f, 150.0f, 195.0f, 240.0f, 285.0f, 315.0f};
static const float HUE_RANGE = 45.0f;

inline float hue_weight(float pixel_hue, float target) {
    while (pixel_hue < 0) pixel_hue += 360.0f;
    while (pixel_hue >= 360) pixel_hue -= 360.0f;

    float diff = std::abs(pixel_hue - target);
    if (diff > 180.0f) diff = 360.0f - diff;
    if (diff > HUE_RANGE) return 0.0f;
    return 0.5f * (1.0f + std::cos(3.14159265f * diff / HUE_RANGE));
}

inline void rgb_to_hls(float r, float g, float b, float& h, float& l, float& s) {
    float max_c = std::max({r, g, b});
    float min_c = std::min({r, g, b});
    l = (max_c + min_c) * 0.5f;

    if (max_c == min_c) {
        h = s = 0;
        return;
    }

    float d = max_c - min_c;
    s = (l > 0.5f) ? d / (2.0f - max_c - min_c) : d / (max_c + min_c);

    if (max_c == r) h = 60.0f * std::fmod((g - b) / d + 6.0f, 6.0f);
    else if (max_c == g) h = 60.0f * ((b - r) / d + 2.0f);
    else h = 60.0f * ((r - g) / d + 4.0f);
}

inline void hls_to_rgb(float h, float l, float s, float& r, float& g, float& b) {
    if (s == 0) { r = g = b = l; return; }

    auto hue2rgb = [](float p, float q, float t) {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0f/6) return p + (q - p) * 6 * t;
        if (t < 0.5f) return q;
        if (t < 2.0f/3) return p + (q - p) * (2.0f/3 - t) * 6;
        return p;
    };

    float q = (l < 0.5f) ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;
    float hn = h / 360.0f;
    r = hue2rgb(p, q, hn + 1.0f/3);
    g = hue2rgb(p, q, hn);
    b = hue2rgb(p, q, hn - 1.0f/3);
}

inline void selective_color(const Image& in, Image& out,
                            const float hue_dials[8], const float sat_dials[8], const float lum_dials[8]) {
    std::array<float, 8> hue_adj, sat_adj, lum_adj;

    for (int i = 0; i < 8; i++) {
        hue_adj[i] = (std::clamp(hue_dials[i], 0.0f, 1.0f) - 0.5f) * 60.0f;
        sat_adj[i] = (std::clamp(sat_dials[i], 0.0f, 1.0f) - 0.5f) * 2.0f;
        lum_adj[i] = (std::clamp(lum_dials[i], 0.0f, 1.0f) - 0.5f) * 2.0f;
    }

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            // Linear to gamma
            float r = std::pow(std::clamp(in.at(y, x, 2), 0.0f, 1.0f), 1.0f/2.2f);
            float g = std::pow(std::clamp(in.at(y, x, 1), 0.0f, 1.0f), 1.0f/2.2f);
            float b = std::pow(std::clamp(in.at(y, x, 0), 0.0f, 1.0f), 1.0f/2.2f);

            float h, l, s;
            rgb_to_hls(r, g, b, h, l, s);

            float total_h = 0, total_s = 0, total_l = 0, total_w = 0;

            for (int band = 0; band < 8; band++) {
                float w = hue_weight(h, HUE_CENTERS[band]);
                if (w > 0.001f) {
                    total_h += w * hue_adj[band];
                    total_s += w * sat_adj[band];
                    total_l += w * lum_adj[band];
                    total_w += w;
                }
            }

            if (total_w > 0.001f) {
                float norm = 1.0f / total_w;
                total_h *= norm;
                total_s *= norm;
                total_l *= norm;

                h += total_h;
                while (h < 0) h += 360.0f;
                while (h >= 360) h -= 360.0f;

                if (total_s > 0) s = s + (1.0f - s) * total_s;
                else s = s * (1.0f + total_s);

                if (total_l > 0) l = l + (1.0f - l) * total_l * 0.5f;
                else l = l * (1.0f + total_l * 0.5f);
            }

            s = std::clamp(s, 0.0f, 1.0f);
            l = std::clamp(l, 0.0f, 1.0f);

            hls_to_rgb(h, l, s, r, g, b);

            // Gamma to linear
            out.at(y, x, 2) = std::pow(std::clamp(r, 0.0f, 1.0f), 2.2f);
            out.at(y, x, 1) = std::pow(std::clamp(g, 0.0f, 1.0f), 2.2f);
            out.at(y, x, 0) = std::pow(std::clamp(b, 0.0f, 1.0f), 2.2f);
        }
    }
}

//------------------------------------------------------------------------------
// Split Tone
//------------------------------------------------------------------------------
inline void split_tone(const Image& in, Image& out,
                       float shadow_temp, float shadow_tint,
                       float highlight_temp, float highlight_tint) {
    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float b = in.at(y, x, 0);
            float g = in.at(y, x, 1);
            float r = in.at(y, x, 2);

            float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;

            float shadow_w = 1.0f - std::clamp(lum * 2.0f, 0.0f, 1.0f);
            float highlight_w = std::clamp((lum - 0.5f) * 2.0f, 0.0f, 1.0f);

            float sr = (shadow_temp - 0.5f) * 0.2f;
            float sb = -(shadow_temp - 0.5f) * 0.2f;
            float sg = (shadow_tint - 0.5f) * 0.1f;

            float hr = (highlight_temp - 0.5f) * 0.2f;
            float hb = -(highlight_temp - 0.5f) * 0.2f;
            float hg = (highlight_tint - 0.5f) * 0.1f;

            out.at(y, x, 2) = r + shadow_w * sr + highlight_w * hr;
            out.at(y, x, 1) = g + shadow_w * sg + highlight_w * hg;
            out.at(y, x, 0) = b + shadow_w * sb + highlight_w * hb;
        }
    }
}

//------------------------------------------------------------------------------
// Detail (Unsharp Mask)
//------------------------------------------------------------------------------
inline void detail(const Image& in, Image& out,
                   float sharpen_amount, float sharpen_radius,
                   float, float) {  // denoise params unused in simplified version
    float amount = sharpen_amount * 2.0f;
    int radius = 1 + int(sharpen_radius * 2.0f);

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            for (int c = 0; c < 3; c++) {
                float center = in.at(y, x, c);
                float sum = 0;
                int count = 0;

                for (int dy = -radius; dy <= radius; dy++) {
                    for (int dx = -radius; dx <= radius; dx++) {
                        int ny = std::clamp(y + dy, 0, in.height - 1);
                        int nx = std::clamp(x + dx, 0, in.width - 1);
                        sum += in.at(ny, nx, c);
                        count++;
                    }
                }

                float blur = sum / count;
                float detail_val = center - blur;
                out.at(y, x, c) = center + detail_val * amount;
            }
        }
    }
}

//------------------------------------------------------------------------------
// Baseline (Highlight Recovery + Exposure)
//------------------------------------------------------------------------------
inline void baseline(const Image& in, Image& out, float ev, float clip_threshold) {
    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float b = in.at(y, x, 0);
            float g = in.at(y, x, 1);
            float r = in.at(y, x, 2);

            float max_c = std::max({r, g, b});
            if (max_c > clip_threshold) {
                int clipped = (r > clip_threshold) + (g > clip_threshold) + (b > clip_threshold);
                if (clipped < 3) {
                    float unclipped_sum = 0;
                    int unclipped_count = 0;
                    if (r <= clip_threshold) { unclipped_sum += r; unclipped_count++; }
                    if (g <= clip_threshold) { unclipped_sum += g; unclipped_count++; }
                    if (b <= clip_threshold) { unclipped_sum += b; unclipped_count++; }

                    float avg = unclipped_sum / unclipped_count;
                    if (r > clip_threshold) r = avg;
                    if (g > clip_threshold) g = avg;
                    if (b > clip_threshold) b = avg;
                }
            }

            float mult = std::pow(2.0f, ev);
            out.at(y, x, 0) = b * mult;
            out.at(y, x, 1) = g * mult;
            out.at(y, x, 2) = r * mult;
        }
    }
}

//------------------------------------------------------------------------------
// Base Curve (Per-Channel LUT with sRGB)
//------------------------------------------------------------------------------
inline void base_curve(const Image& in, Image& out, const float* curve) {
    auto linear_to_srgb = [](float v) -> float {
        v = std::clamp(v, 0.0f, 1.0f);
        return (v <= 0.0031308f) ? v * 12.92f : 1.055f * std::pow(v, 1.0f/2.4f) - 0.055f;
    };

    auto srgb_to_linear = [](float v) -> float {
        return (v <= 0.04045f) ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
    };

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            for (int c = 0; c < 3; c++) {
                float lin = in.at(y, x, c);
                float srgb = linear_to_srgb(lin);

                float pos = srgb * 255.0f;
                int idx0 = std::clamp(int(pos), 0, 254);
                int idx1 = idx0 + 1;
                float frac = pos - idx0;

                int base = c * 256;
                float out_srgb = curve[base + idx0] + frac * (curve[base + idx1] - curve[base + idx0]);

                out.at(y, x, c) = srgb_to_linear(out_srgb);
            }
        }
    }
}

//------------------------------------------------------------------------------
// Color Matrix (3x3 Linear Transform)
//------------------------------------------------------------------------------
inline void color_matrix(const Image& in, Image& out, const float matrix[9]) {
    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float b = in.at(y, x, 0);
            float g = in.at(y, x, 1);
            float r = in.at(y, x, 2);

            out.at(y, x, 2) = r * matrix[0] + g * matrix[1] + b * matrix[2];
            out.at(y, x, 1) = r * matrix[3] + g * matrix[4] + b * matrix[5];
            out.at(y, x, 0) = r * matrix[6] + g * matrix[7] + b * matrix[8];
        }
    }
}

//------------------------------------------------------------------------------
// LUT Curve (Per-Channel 1D LUT through 8-bit)
// Note: CV goes through 8-bit which causes quantization
//------------------------------------------------------------------------------
inline void lut_curve(const Image& in, Image& out, const float* lut, int lut_size) {
    std::vector<float> full_r(256), full_g(256), full_b(256);
    float step = 1.0f / (lut_size - 1);

    const float* lut_r = lut;
    const float* lut_g = lut + lut_size;
    const float* lut_b = lut + 2 * lut_size;

    for (int i = 0; i < 256; i++) {
        float val = i / 255.0f;
        float pos = val / step;
        int idx0 = std::min(int(pos), lut_size - 1);
        int idx1 = std::min(idx0 + 1, lut_size - 1);
        float frac = pos - idx0;

        full_r[i] = lut_r[idx0] + frac * (lut_r[idx1] - lut_r[idx0]);
        full_g[i] = lut_g[idx0] + frac * (lut_g[idx1] - lut_g[idx0]);
        full_b[i] = lut_b[idx0] + frac * (lut_b[idx1] - lut_b[idx0]);
    }

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            // Linear to gamma, quantize to 8-bit
            float r = std::pow(std::clamp(in.at(y, x, 2), 0.0f, 1.0f), 1.0f/2.2f);
            float g = std::pow(std::clamp(in.at(y, x, 1), 0.0f, 1.0f), 1.0f/2.2f);
            float b = std::pow(std::clamp(in.at(y, x, 0), 0.0f, 1.0f), 1.0f/2.2f);

            int ri = std::clamp(int(r * 255.0f + 0.5f), 0, 255);
            int gi = std::clamp(int(g * 255.0f + 0.5f), 0, 255);
            int bi = std::clamp(int(b * 255.0f + 0.5f), 0, 255);

            // Apply LUT
            float ro = full_r[ri];
            float go = full_g[gi];
            float bo = full_b[bi];

            // Back to linear
            out.at(y, x, 2) = std::pow(ro, 2.2f);
            out.at(y, x, 1) = std::pow(go, 2.2f);
            out.at(y, x, 0) = std::pow(bo, 2.2f);
        }
    }
}

//------------------------------------------------------------------------------
// 3D LUT (Trilinear Interpolation)
//------------------------------------------------------------------------------
inline void lut3d(const Image& in, Image& out, const float* lut, int grid_size) {
    auto linear_to_gamma = [](float v) -> float {
        return std::pow(std::clamp(v, 0.0f, 1.0f), 1.0f/2.2f);
    };

    auto gamma_to_linear = [](float v) -> float {
        return std::pow(std::clamp(v, 0.0f, 1.0f), 2.2f);
    };

    auto idx3d = [grid_size](int r, int g, int b, int ch) {
        return ((r * grid_size + g) * grid_size + b) * 3 + ch;
    };

    float scale = float(grid_size - 1);

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float rg = linear_to_gamma(in.at(y, x, 2));
            float gg = linear_to_gamma(in.at(y, x, 1));
            float bg = linear_to_gamma(in.at(y, x, 0));

            float rp = rg * scale, gp = gg * scale, bp = bg * scale;

            int r0 = std::clamp(int(rp), 0, grid_size - 2);
            int g0 = std::clamp(int(gp), 0, grid_size - 2);
            int b0 = std::clamp(int(bp), 0, grid_size - 2);
            int r1 = r0 + 1, g1 = g0 + 1, b1 = b0 + 1;

            float rf = rp - r0, gf = gp - g0, bf = bp - b0;

            for (int ch = 0; ch < 3; ch++) {
                float c000 = lut[idx3d(r0, g0, b0, ch)];
                float c001 = lut[idx3d(r0, g0, b1, ch)];
                float c010 = lut[idx3d(r0, g1, b0, ch)];
                float c011 = lut[idx3d(r0, g1, b1, ch)];
                float c100 = lut[idx3d(r1, g0, b0, ch)];
                float c101 = lut[idx3d(r1, g0, b1, ch)];
                float c110 = lut[idx3d(r1, g1, b0, ch)];
                float c111 = lut[idx3d(r1, g1, b1, ch)];

                float c00 = c000 * (1-bf) + c001 * bf;
                float c01 = c010 * (1-bf) + c011 * bf;
                float c10 = c100 * (1-bf) + c101 * bf;
                float c11 = c110 * (1-bf) + c111 * bf;

                float c0 = c00 * (1-gf) + c01 * gf;
                float c1 = c10 * (1-gf) + c11 * gf;

                float val = c0 * (1-rf) + c1 * rf;

                int out_ch = (ch == 0) ? 2 : (ch == 1) ? 1 : 0;
                out.at(y, x, out_ch) = gamma_to_linear(val);
            }
        }
    }
}

//------------------------------------------------------------------------------
// HSV LUT (Delta Adjustments)
//------------------------------------------------------------------------------
inline void hsv_lut(const Image& in, Image& out, const float* lut,
                    int h_bins, int s_bins) {
    auto linear_to_gamma = [](float v) -> float {
        return std::pow(std::clamp(v, 0.0f, 1.0f), 1.0f/2.2f);
    };

    auto gamma_to_linear = [](float v) -> float {
        return std::pow(std::clamp(v, 0.0f, 1.0f), 2.2f);
    };

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float r = linear_to_gamma(in.at(y, x, 2));
            float g = linear_to_gamma(in.at(y, x, 1));
            float b = linear_to_gamma(in.at(y, x, 0));

            float max_c = std::max({r, g, b});
            float min_c = std::min({r, g, b});
            float v = max_c;
            float s = (max_c > 0.001f) ? (max_c - min_c) / max_c : 0.0f;
            float h = 0;

            if (max_c != min_c) {
                float d = max_c - min_c;
                if (max_c == r) h = 60.0f * std::fmod((g - b) / d + 6.0f, 6.0f);
                else if (max_c == g) h = 60.0f * ((b - r) / d + 2.0f);
                else h = 60.0f * ((r - g) / d + 4.0f);
            }

            float h_pos = (h / 360.0f) * h_bins;
            float s_pos = s * (s_bins - 1);

            int h0 = int(h_pos) % h_bins;
            int h1 = (h0 + 1) % h_bins;
            int s0 = std::clamp(int(s_pos), 0, s_bins - 1);
            int s1 = std::min(s0 + 1, s_bins - 1);

            float hf = h_pos - int(h_pos);
            float sf = s_pos - s0;

            auto lookup = [lut, h_bins, s_bins](int hi, int si) {
                return &lut[(hi * s_bins + si) * 3];
            };

            const float* c00 = lookup(h0, s0);
            const float* c01 = lookup(h0, s1);
            const float* c10 = lookup(h1, s0);
            const float* c11 = lookup(h1, s1);

            float dh = (1-hf)*(1-sf)*c00[0] + (1-hf)*sf*c01[0] + hf*(1-sf)*c10[0] + hf*sf*c11[0];
            float ds = (1-hf)*(1-sf)*c00[1] + (1-hf)*sf*c01[1] + hf*(1-sf)*c10[1] + hf*sf*c11[1];
            float dv = (1-hf)*(1-sf)*c00[2] + (1-hf)*sf*c01[2] + hf*(1-sf)*c10[2] + hf*sf*c11[2];

            h = std::fmod(h + dh * 0.5f + 360.0f, 360.0f);
            s = std::clamp(s + ds, 0.0f, 1.0f);
            v = std::clamp(v + dv, 0.0f, 1.0f);

            float c = v * s;
            float hp = h / 60.0f;
            float x_val = c * (1 - std::abs(std::fmod(hp, 2.0f) - 1));
            float m = v - c;

            if (hp < 1) { r = c; g = x_val; b = 0; }
            else if (hp < 2) { r = x_val; g = c; b = 0; }
            else if (hp < 3) { r = 0; g = c; b = x_val; }
            else if (hp < 4) { r = 0; g = x_val; b = c; }
            else if (hp < 5) { r = x_val; g = 0; b = c; }
            else { r = c; g = 0; b = x_val; }

            out.at(y, x, 2) = gamma_to_linear(r + m);
            out.at(y, x, 1) = gamma_to_linear(g + m);
            out.at(y, x, 0) = gamma_to_linear(b + m);
        }
    }
}

//------------------------------------------------------------------------------
// Polynomial Color Transform
//------------------------------------------------------------------------------
inline void poly_color(const Image& in, Image& out, const float* coeffs) {
    auto linear_to_gamma = [](float v) -> float {
        return std::pow(std::clamp(v, 0.0f, 1.0f), 1.0f/2.2f);
    };

    auto gamma_to_linear = [](float v) -> float {
        return std::pow(std::clamp(v, 0.0f, 1.0f), 2.2f);
    };

    const float* cr = coeffs;
    const float* cg = coeffs + 10;
    const float* cb = coeffs + 20;

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float b = linear_to_gamma(in.at(y, x, 0));
            float g = linear_to_gamma(in.at(y, x, 1));
            float r = linear_to_gamma(in.at(y, x, 2));

            float terms[10] = {
                1.0f, r, g, b,
                r*r, g*g, b*b,
                r*g, r*b, g*b
            };

            float ro = 0, go = 0, bo = 0;
            for (int i = 0; i < 10; i++) {
                ro += cr[i] * terms[i];
                go += cg[i] * terms[i];
                bo += cb[i] * terms[i];
            }

            out.at(y, x, 0) = gamma_to_linear(std::clamp(bo, 0.0f, 1.0f));
            out.at(y, x, 1) = gamma_to_linear(std::clamp(go, 0.0f, 1.0f));
            out.at(y, x, 2) = gamma_to_linear(std::clamp(ro, 0.0f, 1.0f));
        }
    }
}

//------------------------------------------------------------------------------
// Local Tone Mapping (Simplified Bilateral-style)
//------------------------------------------------------------------------------
inline void local_tone(const Image& in, Image& out,
                       float strength, float delta, float window_scale) {
    int window = std::max(5, int(std::min(in.width, in.height) * window_scale)) | 1;
    int half_w = window / 2;

    std::vector<float> lum(in.width * in.height);
    std::vector<float> local_mean(in.width * in.height);

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            lum[y * in.width + x] = 0.2126f * in.at(y, x, 2) +
                                    0.7152f * in.at(y, x, 1) +
                                    0.0722f * in.at(y, x, 0);
        }
    }

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float sum = 0;
            int count = 0;
            for (int dy = -half_w; dy <= half_w; dy++) {
                for (int dx = -half_w; dx <= half_w; dx++) {
                    int ny = std::clamp(y + dy, 0, in.height - 1);
                    int nx = std::clamp(x + dx, 0, in.width - 1);
                    sum += lum[ny * in.width + nx];
                    count++;
                }
            }
            local_mean[y * in.width + x] = sum / count;
        }
    }

    auto asymmetric_weight = [delta](float intensity) {
        float num = std::log(intensity + delta) - std::log(delta);
        float den = std::log(1.0f + delta) - std::log(delta);
        return num / den;
    };

    auto transform_strength = [&asymmetric_weight](float intensity) {
        float f = asymmetric_weight(intensity);
        return 0.5f - 0.5f * std::tanh(4.0f * f - 2.0f);
    };

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            float local_lum = std::max(0.001f, local_mean[y * in.width + x]);
            float pixel_lum = std::max(0.001f, lum[y * in.width + x]);

            float alpha = transform_strength(local_lum);
            float lift_target = 0.18f + (1.0f - 0.18f) * asymmetric_weight(local_lum);
            float target_lum = local_lum + strength * alpha * (lift_target - local_lum);

            float scale = target_lum / local_lum;

            if (pixel_lum > 0.7f) {
                float suppress = std::max(0.0f, 1.0f - (pixel_lum - 0.7f) / 0.3f);
                scale = 1.0f + (scale - 1.0f) * suppress;
            }

            scale = std::min(scale, 2.0f);

            out.at(y, x, 0) = std::clamp(in.at(y, x, 0) * scale, 0.0f, 1.0f);
            out.at(y, x, 1) = std::clamp(in.at(y, x, 1) * scale, 0.0f, 1.0f);
            out.at(y, x, 2) = std::clamp(in.at(y, x, 2) * scale, 0.0f, 1.0f);
        }
    }
}

} // namespace theory
