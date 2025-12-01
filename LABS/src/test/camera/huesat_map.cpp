// huesat_map.cpp
// Estimate a HueSatMap (DCP-style 2.5D polar LUT) from camera JPEG
//
// Structure: 90 hue bins × 25 saturation bins
// Each bin stores: delta_hue, scale_sat, scale_val
//
// This captures the camera's hue-dependent color transforms that
// per-channel curves cannot represent.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

// HueSatMap structure matching DCP spec
struct HueSatMap {
    static constexpr int HUE_DIVISIONS = 90;   // Every 4 degrees
    static constexpr int SAT_DIVISIONS = 25;   // 0.04 increments
    static constexpr int VAL_DIVISIONS = 1;    // Single value layer

    // Storage: [hue][sat] → (delta_h, scale_s, scale_v)
    float delta_hue[HUE_DIVISIONS][SAT_DIVISIONS];
    float scale_sat[HUE_DIVISIONS][SAT_DIVISIONS];
    float scale_val[HUE_DIVISIONS][SAT_DIVISIONS];

    // Accumulation for estimation
    double sum_hue[HUE_DIVISIONS][SAT_DIVISIONS];
    double sum_sat[HUE_DIVISIONS][SAT_DIVISIONS];
    double sum_val[HUE_DIVISIONS][SAT_DIVISIONS];
    int count[HUE_DIVISIONS][SAT_DIVISIONS];

    HueSatMap() {
        clear();
    }

    void clear() {
        for (int h = 0; h < HUE_DIVISIONS; h++) {
            for (int s = 0; s < SAT_DIVISIONS; s++) {
                delta_hue[h][s] = 0.0f;
                scale_sat[h][s] = 1.0f;
                scale_val[h][s] = 1.0f;
                sum_hue[h][s] = 0.0;
                sum_sat[h][s] = 0.0;
                sum_val[h][s] = 0.0;
                count[h][s] = 0;
            }
        }
    }

    void accumulate(float h_in, float s_in, float v_in,
                   float h_out, float s_out, float v_out) {
        // Skip near-neutral colors (no meaningful hue)
        if (s_in < 0.08f || s_out < 0.04f) return;
        if (v_in < 0.05f || v_out < 0.02f) return;

        // Get bins
        int h_bin = static_cast<int>(h_in / 4.0f) % HUE_DIVISIONS;
        int s_bin = std::min(SAT_DIVISIONS - 1, static_cast<int>(s_in * SAT_DIVISIONS));

        // Compute deltas
        float dh = h_out - h_in;
        // Wrap hue difference to [-180, 180]
        if (dh > 180.0f) dh -= 360.0f;
        if (dh < -180.0f) dh += 360.0f;

        float ss = (s_in > 0.001f) ? s_out / s_in : 1.0f;
        float sv = (v_in > 0.001f) ? v_out / v_in : 1.0f;

        // Clamp extreme values
        ss = std::max(0.1f, std::min(3.0f, ss));
        sv = std::max(0.1f, std::min(3.0f, sv));

        sum_hue[h_bin][s_bin] += dh;
        sum_sat[h_bin][s_bin] += ss;
        sum_val[h_bin][s_bin] += sv;
        count[h_bin][s_bin]++;
    }

    void finalize() {
        for (int h = 0; h < HUE_DIVISIONS; h++) {
            for (int s = 0; s < SAT_DIVISIONS; s++) {
                if (count[h][s] > 10) {
                    delta_hue[h][s] = sum_hue[h][s] / count[h][s];
                    scale_sat[h][s] = sum_sat[h][s] / count[h][s];
                    scale_val[h][s] = sum_val[h][s] / count[h][s];
                } else {
                    // Use defaults
                    delta_hue[h][s] = 0.0f;
                    scale_sat[h][s] = 1.0f;
                    scale_val[h][s] = 1.0f;
                }
            }
        }

        // Smooth to fill gaps and reduce noise
        smooth();
    }

    void smooth() {
        // Simple box filter smoothing
        float temp_h[HUE_DIVISIONS][SAT_DIVISIONS];
        float temp_s[HUE_DIVISIONS][SAT_DIVISIONS];
        float temp_v[HUE_DIVISIONS][SAT_DIVISIONS];

        for (int h = 0; h < HUE_DIVISIONS; h++) {
            for (int s = 0; s < SAT_DIVISIONS; s++) {
                float sum_h = 0, sum_s = 0, sum_v = 0;
                int n = 0;

                for (int dh = -2; dh <= 2; dh++) {
                    for (int ds = -1; ds <= 1; ds++) {
                        int hh = (h + dh + HUE_DIVISIONS) % HUE_DIVISIONS;
                        int ss = std::max(0, std::min(SAT_DIVISIONS - 1, s + ds));

                        if (count[hh][ss] > 0) {
                            sum_h += delta_hue[hh][ss];
                            sum_s += scale_sat[hh][ss];
                            sum_v += scale_val[hh][ss];
                            n++;
                        }
                    }
                }

                if (n > 0) {
                    temp_h[h][s] = sum_h / n;
                    temp_s[h][s] = sum_s / n;
                    temp_v[h][s] = sum_v / n;
                } else {
                    temp_h[h][s] = 0.0f;
                    temp_s[h][s] = 1.0f;
                    temp_v[h][s] = 1.0f;
                }
            }
        }

        // Copy back
        for (int h = 0; h < HUE_DIVISIONS; h++) {
            for (int s = 0; s < SAT_DIVISIONS; s++) {
                delta_hue[h][s] = temp_h[h][s];
                scale_sat[h][s] = temp_s[h][s];
                scale_val[h][s] = temp_v[h][s];
            }
        }
    }

    // Apply to a pixel (bilinear interpolation)
    void apply(float& h, float& s, float& v) const {
        if (s < 0.01f) return;

        // Get fractional bin indices
        float h_idx = h / 4.0f;
        float s_idx = s * SAT_DIVISIONS;

        int h0 = static_cast<int>(h_idx) % HUE_DIVISIONS;
        int h1 = (h0 + 1) % HUE_DIVISIONS;
        int s0 = std::min(SAT_DIVISIONS - 1, static_cast<int>(s_idx));
        int s1 = std::min(SAT_DIVISIONS - 1, s0 + 1);

        float hf = h_idx - std::floor(h_idx);
        float sf = s_idx - s0;

        // Bilinear interpolation
        auto lerp = [](float a, float b, float t) { return a + t * (b - a); };

        float dh = lerp(lerp(delta_hue[h0][s0], delta_hue[h0][s1], sf),
                       lerp(delta_hue[h1][s0], delta_hue[h1][s1], sf), hf);
        float ss = lerp(lerp(scale_sat[h0][s0], scale_sat[h0][s1], sf),
                       lerp(scale_sat[h1][s0], scale_sat[h1][s1], sf), hf);
        float sv = lerp(lerp(scale_val[h0][s0], scale_val[h0][s1], sf),
                       lerp(scale_val[h1][s0], scale_val[h1][s1], sf), hf);

        h = std::fmod(h + dh + 360.0f, 360.0f);
        s = std::max(0.0f, std::min(1.0f, s * ss));
        v = std::max(0.0f, std::min(1.0f, v * sv));
    }
};

// RGB to HSV (0-360, 0-1, 0-1)
void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) {
    float max_c = std::max({r, g, b});
    float min_c = std::min({r, g, b});
    float delta = max_c - min_c;

    v = max_c;
    s = (max_c > 0.0001f) ? delta / max_c : 0.0f;

    if (delta < 0.0001f) {
        h = 0.0f;
    } else if (max_c == r) {
        h = 60.0f * std::fmod((g - b) / delta + 6.0f, 6.0f);
    } else if (max_c == g) {
        h = 60.0f * ((b - r) / delta + 2.0f);
    } else {
        h = 60.0f * ((r - g) / delta + 4.0f);
    }
}

// HSV to RGB
void hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b) {
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    int sector = static_cast<int>(h / 60.0f) % 6;
    switch (sector) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        case 5: r = c; g = 0; b = x; break;
    }
    r += m; g += m; b += m;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== HueSatMap Estimation ===" << std::endl;
    std::cout << "File: " << raw_path << std::endl;

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head) {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    cv::Mat scene_linear;
    head->data().view().copyTo(scene_linear);

    cv::Mat camera_jpeg;
    head->view().view().copyTo(camera_jpeg);

    // Resize to match
    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    // Apply base curve from RAWS
    const float* baseCurve = head->baseCurve();

    cv::Mat scene_clamped;
    cv::max(scene_resized, 0.0f, scene_clamped);
    cv::min(scene_clamped, 1.0f, scene_clamped);
    cv::pow(scene_clamped, 1.0f / 2.2f, scene_clamped);

    cv::Mat scene_curved(scene_clamped.size(), CV_32FC3);
    for (int y = 0; y < scene_clamped.rows; y++) {
        const float* src = scene_clamped.ptr<float>(y);
        float* dst = scene_curved.ptr<float>(y);

        for (int x = 0; x < scene_clamped.cols; x++) {
            for (int c = 0; c < 3; c++) {
                float v = src[x * 3 + c];
                int bin = std::min(255, static_cast<int>(v * 255.0f));
                float frac = v * 255.0f - bin;
                int next_bin = std::min(255, bin + 1);
                dst[x * 3 + c] = baseCurve[c * 256 + bin] * (1 - frac)
                               + baseCurve[c * 256 + next_bin] * frac;
            }
        }
    }

    // Build HueSatMap
    HueSatMap hsm;
    cv::Mat target_f;
    camera_jpeg.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);

    std::cout << "\nEstimating HueSatMap from " << scene_curved.rows * scene_curved.cols
              << " pixels..." << std::endl;

    for (int y = 0; y < scene_curved.rows; y++) {
        const float* src = scene_curved.ptr<float>(y);
        const float* tgt = target_f.ptr<float>(y);

        for (int x = 0; x < scene_curved.cols; x++) {
            // Our output (after base curve)
            float h_in, s_in, v_in;
            rgb_to_hsv(src[x*3+2], src[x*3+1], src[x*3+0], h_in, s_in, v_in);

            // Camera target
            float h_out, s_out, v_out;
            rgb_to_hsv(tgt[x*3+2], tgt[x*3+1], tgt[x*3+0], h_out, s_out, v_out);

            hsm.accumulate(h_in, s_in, v_in, h_out, s_out, v_out);
        }
    }

    hsm.finalize();

    // Print summary
    std::cout << "\n=== HueSatMap Summary ===" << std::endl;
    std::cout << "Hue bins with significant shifts:" << std::endl;

    for (int h = 0; h < HueSatMap::HUE_DIVISIONS; h += 5) {
        // Find max delta in this hue range
        float max_dh = 0, max_ss = 1.0f;
        int samples = 0;
        for (int dh = 0; dh < 5; dh++) {
            int hh = (h + dh) % HueSatMap::HUE_DIVISIONS;
            for (int s = 5; s < HueSatMap::SAT_DIVISIONS; s++) {
                if (std::abs(hsm.delta_hue[hh][s]) > std::abs(max_dh))
                    max_dh = hsm.delta_hue[hh][s];
                if (std::abs(hsm.scale_sat[hh][s] - 1.0f) > std::abs(max_ss - 1.0f))
                    max_ss = hsm.scale_sat[hh][s];
                samples += hsm.count[hh][s];
            }
        }

        if (samples > 100 && (std::abs(max_dh) > 2.0f || std::abs(max_ss - 1.0f) > 0.1f)) {
            printf("  Hue %3d°-%3d°: ΔH=%+5.1f°, S×%.2f (%d samples)\n",
                   h * 4, (h + 5) * 4, max_dh, max_ss, samples);
        }
    }

    // Apply HueSatMap and measure error
    std::cout << "\n=== Applying HueSatMap ===" << std::endl;

    cv::Mat result(scene_curved.size(), CV_32FC3);
    for (int y = 0; y < scene_curved.rows; y++) {
        const float* src = scene_curved.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < scene_curved.cols; x++) {
            float h, s, v;
            rgb_to_hsv(src[x*3+2], src[x*3+1], src[x*3+0], h, s, v);
            hsm.apply(h, s, v);
            hsv_to_rgb(h, s, v, dst[x*3+2], dst[x*3+1], dst[x*3+0]);
        }
    }

    // Measure error before and after HueSatMap
    cv::Mat diff_before, diff_after;
    cv::absdiff(scene_curved, target_f, diff_before);
    cv::absdiff(result, target_f, diff_after);

    cv::Scalar mean_before = cv::mean(diff_before);
    cv::Scalar mean_after = cv::mean(diff_after);

    float error_before = (mean_before[0] + mean_before[1] + mean_before[2]) / 3.0f * 100.0f;
    float error_after = (mean_after[0] + mean_after[1] + mean_after[2]) / 3.0f * 100.0f;

    std::cout << "Error before HueSatMap: " << error_before << "%" << std::endl;
    std::cout << "Error after HueSatMap:  " << error_after << "%" << std::endl;
    std::cout << "Improvement: " << (error_before - error_after) << " percentage points" << std::endl;

    // Save comparison
    cv::Mat result_8u;
    result.convertTo(result_8u, CV_8UC3, 255.0);

    cv::Mat comparison;
    cv::hconcat(camera_jpeg, result_8u, comparison);
    cv::imwrite("tmp/var/tune/huesat_compare.png", comparison);
    std::cout << "\nSaved: tmp/var/tune/huesat_compare.png (left: camera, right: HSM result)" << std::endl;

    return 0;
}
