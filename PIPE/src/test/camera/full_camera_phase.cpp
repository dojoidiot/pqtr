// full_camera_phase.cpp
// Complete camera phase implementation:
// 1. Base curve (tone mapping)
// 2. HueSatMap (color transforms)
// 3. Local tone mapping (DRO simulation)
//
// Measure error at each stage to see what's working

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

// HueSatMap structure
struct HueSatMap {
    static constexpr int HUE_DIVISIONS = 90;
    static constexpr int SAT_DIVISIONS = 25;

    float delta_hue[HUE_DIVISIONS][SAT_DIVISIONS];
    float scale_sat[HUE_DIVISIONS][SAT_DIVISIONS];
    float scale_val[HUE_DIVISIONS][SAT_DIVISIONS];

    double sum_hue[HUE_DIVISIONS][SAT_DIVISIONS];
    double sum_sat[HUE_DIVISIONS][SAT_DIVISIONS];
    double sum_val[HUE_DIVISIONS][SAT_DIVISIONS];
    int count[HUE_DIVISIONS][SAT_DIVISIONS];

    HueSatMap() { clear(); }

    void clear() {
        for (int h = 0; h < HUE_DIVISIONS; h++) {
            for (int s = 0; s < SAT_DIVISIONS; s++) {
                delta_hue[h][s] = 0.0f;
                scale_sat[h][s] = 1.0f;
                scale_val[h][s] = 1.0f;
                sum_hue[h][s] = sum_sat[h][s] = sum_val[h][s] = 0.0;
                count[h][s] = 0;
            }
        }
    }

    void accumulate(float h_in, float s_in, float v_in,
                   float h_out, float s_out, float v_out) {
        if (s_in < 0.08f || s_out < 0.04f) return;
        if (v_in < 0.05f || v_out < 0.02f) return;

        int h_bin = static_cast<int>(h_in / 4.0f) % HUE_DIVISIONS;
        int s_bin = std::min(SAT_DIVISIONS - 1, static_cast<int>(s_in * SAT_DIVISIONS));

        float dh = h_out - h_in;
        if (dh > 180.0f) dh -= 360.0f;
        if (dh < -180.0f) dh += 360.0f;

        float ss = (s_in > 0.001f) ? s_out / s_in : 1.0f;
        float sv = (v_in > 0.001f) ? v_out / v_in : 1.0f;

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
                    delta_hue[h][s] = 0.0f;
                    scale_sat[h][s] = 1.0f;
                    scale_val[h][s] = 1.0f;
                }
            }
        }
        smooth();
    }

    void smooth() {
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
        for (int h = 0; h < HUE_DIVISIONS; h++) {
            for (int s = 0; s < SAT_DIVISIONS; s++) {
                delta_hue[h][s] = temp_h[h][s];
                scale_sat[h][s] = temp_s[h][s];
                scale_val[h][s] = temp_v[h][s];
            }
        }
    }

    void apply(float& h, float& s, float& v) const {
        if (s < 0.01f) return;
        float h_idx = h / 4.0f;
        float s_idx = s * SAT_DIVISIONS;
        int h0 = static_cast<int>(h_idx) % HUE_DIVISIONS;
        int h1 = (h0 + 1) % HUE_DIVISIONS;
        int s0 = std::min(SAT_DIVISIONS - 1, static_cast<int>(s_idx));
        int s1 = std::min(SAT_DIVISIONS - 1, s0 + 1);
        float hf = h_idx - std::floor(h_idx);
        float sf = s_idx - s0;
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

void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) {
    float max_c = std::max({r, g, b});
    float min_c = std::min({r, g, b});
    float delta = max_c - min_c;
    v = max_c;
    s = (max_c > 0.0001f) ? delta / max_c : 0.0f;
    if (delta < 0.0001f) h = 0.0f;
    else if (max_c == r) h = 60.0f * std::fmod((g - b) / delta + 6.0f, 6.0f);
    else if (max_c == g) h = 60.0f * ((b - r) / delta + 2.0f);
    else h = 60.0f * ((r - g) / delta + 4.0f);
}

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

// Simple local tone mapping (DRO-style)
cv::Mat apply_local_tone_map(const cv::Mat& img, float strength) {
    // Compute luminance
    cv::Mat gray(img.size(), CV_32FC1);
    for (int y = 0; y < img.rows; y++) {
        const float* src = img.ptr<float>(y);
        float* dst = gray.ptr<float>(y);
        for (int x = 0; x < img.cols; x++) {
            dst[x] = 0.0722f * src[x*3+0] + 0.7152f * src[x*3+1] + 0.2126f * src[x*3+2];
        }
    }

    // Build Gaussian pyramid (5 levels)
    std::vector<cv::Mat> pyramid(5);
    pyramid[0] = gray.clone();
    for (int i = 1; i < 5; i++) {
        cv::pyrDown(pyramid[i-1], pyramid[i]);
    }

    // Use level 3 as local average (8x8 blur)
    cv::Mat local_avg;
    cv::resize(pyramid[3], local_avg, gray.size(), 0, 0, cv::INTER_LINEAR);

    // Apply Reinhard local operator
    cv::Mat result(img.size(), CV_32FC3);
    for (int y = 0; y < img.rows; y++) {
        const float* src = img.ptr<float>(y);
        const float* lum = gray.ptr<float>(y);
        const float* local = local_avg.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < img.cols; x++) {
            float L = lum[x];
            float L_local = local[x];

            float scale = 1.0f;
            if (L > 0.001f && L_local > 0.001f) {
                float L_new = L / (1.0f + strength * L_local);
                L_new = L_new * (1.0f + strength * 0.18f);
                scale = L_new / L;
            }

            for (int c = 0; c < 3; c++) {
                dst[x*3+c] = std::max(0.0f, std::min(1.0f, src[x*3+c] * scale));
            }
        }
    }

    return result;
}

float compute_error(const cv::Mat& a, const cv::Mat& b) {
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    cv::Scalar mean = cv::mean(diff);
    return (mean[0] + mean[1] + mean[2]) / 3.0f * 100.0f;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Full Camera Phase Test ===" << std::endl;
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

    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    const float* baseCurve = head->baseCurve();

    cv::Mat target_f;
    camera_jpeg.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);

    // Stage 0: Simple gamma
    cv::Mat stage0;
    cv::max(scene_resized, 0.0f, stage0);
    cv::min(stage0, 1.0f, stage0);
    cv::pow(stage0, 1.0f / 2.2f, stage0);

    std::cout << "\n=== Error at Each Stage ===" << std::endl;
    std::cout << "Stage 0 (Gamma 2.2):      " << compute_error(stage0, target_f) << "%" << std::endl;

    // Stage 1: Base curve
    cv::Mat stage1(stage0.size(), CV_32FC3);
    for (int y = 0; y < stage0.rows; y++) {
        const float* src = stage0.ptr<float>(y);
        float* dst = stage1.ptr<float>(y);
        for (int x = 0; x < stage0.cols; x++) {
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
    std::cout << "Stage 1 (Base Curve):     " << compute_error(stage1, target_f) << "%" << std::endl;

    // Stage 2: HueSatMap
    HueSatMap hsm;
    for (int y = 0; y < stage1.rows; y++) {
        const float* src = stage1.ptr<float>(y);
        const float* tgt = target_f.ptr<float>(y);
        for (int x = 0; x < stage1.cols; x++) {
            float h_in, s_in, v_in, h_out, s_out, v_out;
            rgb_to_hsv(src[x*3+2], src[x*3+1], src[x*3+0], h_in, s_in, v_in);
            rgb_to_hsv(tgt[x*3+2], tgt[x*3+1], tgt[x*3+0], h_out, s_out, v_out);
            hsm.accumulate(h_in, s_in, v_in, h_out, s_out, v_out);
        }
    }
    hsm.finalize();

    cv::Mat stage2(stage1.size(), CV_32FC3);
    for (int y = 0; y < stage1.rows; y++) {
        const float* src = stage1.ptr<float>(y);
        float* dst = stage2.ptr<float>(y);
        for (int x = 0; x < stage1.cols; x++) {
            float h, s, v;
            rgb_to_hsv(src[x*3+2], src[x*3+1], src[x*3+0], h, s, v);
            hsm.apply(h, s, v);
            hsv_to_rgb(h, s, v, dst[x*3+2], dst[x*3+1], dst[x*3+0]);
        }
    }
    std::cout << "Stage 2 (+ HueSatMap):    " << compute_error(stage2, target_f) << "%" << std::endl;

    // Stage 3: Local tone mapping
    // Search for optimal DRO strength
    float best_strength = 0.0f;
    float best_error = 1e9f;
    for (float s = 0.0f; s <= 2.0f; s += 0.1f) {
        cv::Mat ltm = apply_local_tone_map(stage2, s);
        float err = compute_error(ltm, target_f);
        if (err < best_error) {
            best_error = err;
            best_strength = s;
        }
    }

    cv::Mat stage3 = apply_local_tone_map(stage2, best_strength);
    std::cout << "Stage 3 (+ LTM s=" << best_strength << "): " << compute_error(stage3, target_f) << "%" << std::endl;

    // Stage 4: Final per-channel curve refinement
    std::vector<double> sum[3];
    std::vector<double> count[3];
    for (int c = 0; c < 3; c++) {
        sum[c].resize(256, 0.0);
        count[c].resize(256, 0.0);
    }

    for (int y = 0; y < stage3.rows; y++) {
        const float* src = stage3.ptr<float>(y);
        const float* tgt = target_f.ptr<float>(y);
        for (int x = 0; x < stage3.cols; x++) {
            for (int c = 0; c < 3; c++) {
                int bin = std::min(255, static_cast<int>(src[x * 3 + c] * 255.0f));
                sum[c][bin] += tgt[x * 3 + c];
                count[c][bin] += 1.0;
            }
        }
    }

    float final_curve[768];
    for (int c = 0; c < 3; c++) {
        for (int i = 0; i < 256; i++) {
            if (count[c][i] > 0)
                final_curve[c * 256 + i] = sum[c][i] / count[c][i];
            else
                final_curve[c * 256 + i] = i / 255.0f;
        }
        for (int i = 1; i < 256; i++) {
            if (final_curve[c * 256 + i] < final_curve[c * 256 + i - 1])
                final_curve[c * 256 + i] = final_curve[c * 256 + i - 1];
        }
    }

    cv::Mat stage4(stage3.size(), CV_32FC3);
    for (int y = 0; y < stage3.rows; y++) {
        const float* src = stage3.ptr<float>(y);
        float* dst = stage4.ptr<float>(y);
        for (int x = 0; x < stage3.cols; x++) {
            for (int c = 0; c < 3; c++) {
                float v = src[x * 3 + c];
                int bin = std::min(255, static_cast<int>(v * 255.0f));
                float frac = v * 255.0f - bin;
                int next_bin = std::min(255, bin + 1);
                dst[x * 3 + c] = final_curve[c * 256 + bin] * (1 - frac)
                               + final_curve[c * 256 + next_bin] * frac;
            }
        }
    }
    std::cout << "Stage 4 (+ Final Curve):  " << compute_error(stage4, target_f) << "%" << std::endl;

    // Save comparison
    cv::Mat stage4_8u;
    stage4.convertTo(stage4_8u, CV_8UC3, 255.0);

    cv::Mat comparison;
    cv::hconcat(camera_jpeg, stage4_8u, comparison);
    cv::imwrite("tmp/var/tune/full_camera_phase.png", comparison);
    std::cout << "\nSaved: tmp/var/tune/full_camera_phase.png (left: camera, right: our result)" << std::endl;

    return 0;
}
