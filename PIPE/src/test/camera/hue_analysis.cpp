// hue_analysis.cpp
// Analyze camera's hue-dependent transforms
// For each input hue, measure:
// - Output hue (hue rotation)
// - Saturation scaling
// - Luminance shift

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

// RGB to HSV
void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v)
{
    float max_c = std::max({r, g, b});
    float min_c = std::min({r, g, b});
    float delta = max_c - min_c;

    v = max_c;

    if (max_c < 0.0001f) {
        h = 0;
        s = 0;
        return;
    }

    s = delta / max_c;

    if (delta < 0.0001f) {
        h = 0;
    } else if (max_c == r) {
        h = 60.0f * fmod((g - b) / delta, 6.0f);
    } else if (max_c == g) {
        h = 60.0f * ((b - r) / delta + 2.0f);
    } else {
        h = 60.0f * ((r - g) / delta + 4.0f);
    }

    if (h < 0) h += 360.0f;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Hue-Dependent Transform Analysis ===" << std::endl;

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

    // Convert scene to gamma
    cv::Mat scene_gamma;
    cv::Mat clamped;
    cv::max(scene_resized, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);
    cv::pow(clamped, 1.0f/2.2f, scene_gamma);

    // Analyze per-hue transforms
    // 36 hue bins (10° each)
    const int HUE_BINS = 36;
    std::vector<double> hue_shift_sum(HUE_BINS, 0.0);
    std::vector<double> sat_scale_sum(HUE_BINS, 0.0);
    std::vector<double> val_scale_sum(HUE_BINS, 0.0);
    std::vector<int> hue_count(HUE_BINS, 0);

    for (int y = 0; y < scene_gamma.rows; y++) {
        const float* s_ptr = scene_gamma.ptr<float>(y);
        const uchar* t_ptr = camera_jpeg.ptr<uchar>(y);

        for (int x = 0; x < scene_gamma.cols; x++) {
            // Input (scene-linear in gamma)
            float sb = s_ptr[x*3 + 0];
            float sg = s_ptr[x*3 + 1];
            float sr = s_ptr[x*3 + 2];

            float h_in, s_in, v_in;
            rgb_to_hsv(sr, sg, sb, h_in, s_in, v_in);

            // Target (camera JPEG)
            float tb = t_ptr[x*3 + 0] / 255.0f;
            float tg = t_ptr[x*3 + 1] / 255.0f;
            float tr = t_ptr[x*3 + 2] / 255.0f;

            float h_out, s_out, v_out;
            rgb_to_hsv(tr, tg, tb, h_out, s_out, v_out);

            // Only use sufficiently saturated pixels
            if (s_in > 0.15f && s_out > 0.05f && v_in > 0.1f) {
                int bin = static_cast<int>(h_in / 10.0f) % HUE_BINS;

                // Hue shift (handle wraparound)
                float h_diff = h_out - h_in;
                if (h_diff > 180.0f) h_diff -= 360.0f;
                if (h_diff < -180.0f) h_diff += 360.0f;

                hue_shift_sum[bin] += h_diff;
                sat_scale_sum[bin] += s_out / s_in;
                val_scale_sum[bin] += v_out / v_in;
                hue_count[bin]++;
            }
        }
    }

    // Print results
    std::cout << "\nHue Bin | Hue Shift | Sat Scale | Val Scale | Samples" << std::endl;
    std::cout << "--------|-----------|-----------|-----------|--------" << std::endl;

    const char* hue_names[] = {
        "Red", "Red-Org", "Orange", "Org-Yel", "Yellow", "Yel-Grn",
        "Green", "Grn-Cya", "Cyan", "Cya-Blu", "Blue", "Blu-Mag",
        "Magenta", "Mag-Red", "Red2", "Red-Org2", "Orange2", "Org-Yel2",
        "Yellow2", "Yel-Grn2", "Green2", "Grn-Cya2", "Cyan2", "Cya-Blu2",
        "Blue2", "Blu-Mag2", "Magenta2", "Mag-Red2", "Red3", "Red-Org3",
        "Orange3", "Org-Yel3", "Yellow3", "Yel-Grn3", "Green3", "Grn-Cya3"
    };

    for (int i = 0; i < HUE_BINS; i++) {
        if (hue_count[i] > 100) {
            float h_shift = hue_shift_sum[i] / hue_count[i];
            float s_scale = sat_scale_sum[i] / hue_count[i];
            float v_scale = val_scale_sum[i] / hue_count[i];

            printf("%3d°-%3d° | %+6.1f° | %5.2f | %5.2f | %d\n",
                   i * 10, (i + 1) * 10, h_shift, s_scale, v_scale, hue_count[i]);
        }
    }

    // Find most significant transforms
    std::cout << "\n=== Key Findings ===" << std::endl;

    float max_hue_shift = 0;
    int max_hue_bin = -1;
    float max_sat_boost = 1.0f;
    int max_sat_bin = -1;

    for (int i = 0; i < HUE_BINS; i++) {
        if (hue_count[i] > 100) {
            float h_shift = std::abs(hue_shift_sum[i] / hue_count[i]);
            float s_scale = sat_scale_sum[i] / hue_count[i];

            if (h_shift > max_hue_shift) {
                max_hue_shift = h_shift;
                max_hue_bin = i;
            }
            if (s_scale > max_sat_boost) {
                max_sat_boost = s_scale;
                max_sat_bin = i;
            }
        }
    }

    if (max_hue_bin >= 0) {
        std::cout << "Max hue shift: " << max_hue_shift << "° at "
                  << max_hue_bin * 10 << "-" << (max_hue_bin + 1) * 10 << "°" << std::endl;
    }
    if (max_sat_bin >= 0) {
        std::cout << "Max sat boost: " << max_sat_boost << "x at "
                  << max_sat_bin * 10 << "-" << (max_sat_bin + 1) * 10 << "°" << std::endl;
    }

    return 0;
}
