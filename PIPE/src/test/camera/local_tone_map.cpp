// local_tone_map.cpp
// Implement Iridix-style local tone mapping for camera phase
//
// Based on the Reinhard local operator:
//   L_out = L / (1 + L_local)
//
// Where L_local is computed from a Gaussian pyramid at multiple scales.
// We select the scale that best preserves local contrast without causing halos.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

// Compute luminance from BGR
cv::Mat compute_luminance(const cv::Mat& bgr)
{
    cv::Mat lum(bgr.size(), CV_32FC1);

    for (int y = 0; y < bgr.rows; y++) {
        const float* src = bgr.ptr<float>(y);
        float* dst = lum.ptr<float>(y);

        for (int x = 0; x < bgr.cols; x++) {
            float b = src[x*3 + 0];
            float g = src[x*3 + 1];
            float r = src[x*3 + 2];
            dst[x] = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        }
    }

    return lum;
}

// Build Gaussian pyramid
std::vector<cv::Mat> build_pyramid(const cv::Mat& img, int levels)
{
    std::vector<cv::Mat> pyramid(levels);
    pyramid[0] = img.clone();

    for (int i = 1; i < levels; i++) {
        cv::pyrDown(pyramid[i-1], pyramid[i]);
    }

    return pyramid;
}

// Upsample pyramid level to original size
cv::Mat upsample_to_size(const cv::Mat& small, cv::Size target_size)
{
    cv::Mat result;
    cv::resize(small, result, target_size, 0, 0, cv::INTER_LINEAR);
    return result;
}

// Select best scale per-pixel to avoid halos
// Heuristic: use larger scale (more local) unless there's a large gradient
cv::Mat select_local_average(const std::vector<cv::Mat>& pyramid, const cv::Mat& lum)
{
    cv::Size target_size = lum.size();
    int levels = pyramid.size();

    // Upsample all pyramid levels
    std::vector<cv::Mat> upsampled(levels);
    for (int i = 0; i < levels; i++) {
        upsampled[i] = upsample_to_size(pyramid[i], target_size);
    }

    // Start with a moderate scale (level 3 is typical - ~8x8 pixel average)
    int default_level = std::min(3, levels - 1);

    // For simplicity, use fixed scale for now
    // More sophisticated: detect edges and use coarser scale near them
    return upsampled[default_level];
}

// Apply local tone mapping
// Formula: L_out = L / (1 + s * L_local)
// where s controls strength (lower = more boost in shadows)
cv::Mat apply_local_tone_map(const cv::Mat& bgr, float strength = 0.5f)
{
    // Extract luminance
    cv::Mat lum = compute_luminance(bgr);

    // Build pyramid (5 levels: 1x, 2x, 4x, 8x, 16x blur)
    std::vector<cv::Mat> pyramid = build_pyramid(lum, 5);

    // Select local average per pixel
    cv::Mat L_local = select_local_average(pyramid, lum);

    // Also compute global average for weighting
    cv::Scalar mean_lum = cv::mean(lum);
    float L_global = mean_lum[0];

    // Weight between local and global
    float alpha = 0.5f;  // 0 = pure local, 1 = pure global

    // Apply tone mapping: scale luminance
    cv::Mat result(bgr.size(), CV_32FC3);

    for (int y = 0; y < bgr.rows; y++) {
        const float* src = bgr.ptr<float>(y);
        const float* lum_ptr = lum.ptr<float>(y);
        const float* local_ptr = L_local.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < bgr.cols; x++) {
            float L = lum_ptr[x];
            float L_avg = alpha * L_global + (1.0f - alpha) * local_ptr[x];

            // Reinhard local operator
            // L_new = L / (1 + s * L_avg)
            // For DRO, we want to lift shadows more
            float denominator = 1.0f + strength * L_avg;
            float scale = 1.0f;

            if (L > 0.001f && denominator > 0.001f) {
                float L_new = L / denominator;
                // Re-normalize to preserve original range roughly
                L_new = L_new * (1.0f + strength * 0.18f);  // 0.18 is middle gray
                scale = L_new / L;
            }

            // Scale RGB to preserve color
            float b = src[x*3 + 0];
            float g = src[x*3 + 1];
            float r = src[x*3 + 2];

            dst[x*3 + 0] = std::max(0.0f, std::min(1.0f, b * scale));
            dst[x*3 + 1] = std::max(0.0f, std::min(1.0f, g * scale));
            dst[x*3 + 2] = std::max(0.0f, std::min(1.0f, r * scale));
        }
    }

    return result;
}

// Estimate optimal DRO strength by comparing with target
float estimate_dro_strength(const cv::Mat& scene_gamma, const cv::Mat& target)
{
    cv::Mat target_f;
    target.convertTo(target_f, CV_32FC3, 1.0/255.0);

    float best_strength = 0.0f;
    float best_error = 1e9f;

    // Search for optimal strength
    for (float s = 0.0f; s <= 2.0f; s += 0.1f) {
        cv::Mat result = apply_local_tone_map(scene_gamma, s);

        cv::Mat diff;
        cv::absdiff(result, target_f, diff);
        cv::Scalar mean_diff = cv::mean(diff);
        float error = (mean_diff[0] + mean_diff[1] + mean_diff[2]) / 3.0f;

        if (error < best_error) {
            best_error = error;
            best_strength = s;
        }
    }

    return best_strength;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Local Tone Mapping (DRO Simulation) ===" << std::endl;

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

    // Convert to gamma
    cv::Mat scene_gamma;
    cv::max(scene_resized, 0.0f, scene_gamma);
    cv::min(scene_gamma, 1.0f, scene_gamma);
    cv::pow(scene_gamma, 1.0f/2.2f, scene_gamma);

    std::cout << "Image size: " << scene_gamma.cols << "x" << scene_gamma.rows << std::endl;

    // Estimate optimal DRO strength
    std::cout << "\nSearching for optimal DRO strength..." << std::endl;
    float optimal_strength = estimate_dro_strength(scene_gamma, camera_jpeg);
    std::cout << "Optimal DRO strength: " << optimal_strength << std::endl;

    // Apply local tone mapping
    cv::Mat ltm_result = apply_local_tone_map(scene_gamma, optimal_strength);

    // Compute error BEFORE any curves
    cv::Mat target_f;
    camera_jpeg.convertTo(target_f, CV_32FC3, 1.0/255.0);

    cv::Mat diff;
    cv::absdiff(ltm_result, target_f, diff);
    cv::Scalar mean_diff = cv::mean(diff);
    float ltm_error = (mean_diff[0] + mean_diff[1] + mean_diff[2]) / 3.0f;

    std::cout << "\nLocal tone map only error: " << (ltm_error * 100.0f) << "%" << std::endl;

    // Now add global tone curve estimation on LTM result
    std::cout << "\n--- Adding Global Tone Curve ---" << std::endl;

    // Estimate per-channel curve from LTM result to target
    std::vector<double> sum[3];
    std::vector<int> count[3];
    for (int c = 0; c < 3; c++) {
        sum[c].resize(256, 0.0);
        count[c].resize(256, 0.0);
    }

    for (int y = 0; y < ltm_result.rows; y++) {
        const float* l_ptr = ltm_result.ptr<float>(y);
        const uchar* t_ptr = camera_jpeg.ptr<uchar>(y);

        for (int x = 0; x < ltm_result.cols; x++) {
            for (int c = 0; c < 3; c++) {
                int bin = std::min(255, static_cast<int>(l_ptr[x*3 + c] * 255.0f));
                sum[c][bin] += t_ptr[x*3 + c] / 255.0f;
                count[c][bin] += 1.0;
            }
        }
    }

    // Build curves
    std::vector<float> curves[3];
    for (int c = 0; c < 3; c++) {
        curves[c].resize(256);
        for (int i = 0; i < 256; i++) {
            if (count[c][i] > 0)
                curves[c][i] = sum[c][i] / count[c][i];
            else
                curves[c][i] = i / 255.0f;
        }

        // Ensure monotonicity
        for (int i = 1; i < 256; i++) {
            if (curves[c][i] < curves[c][i-1])
                curves[c][i] = curves[c][i-1];
        }
    }

    // Apply curves
    cv::Mat final_result(ltm_result.size(), CV_32FC3);

    for (int y = 0; y < ltm_result.rows; y++) {
        const float* l_ptr = ltm_result.ptr<float>(y);
        float* f_ptr = final_result.ptr<float>(y);

        for (int x = 0; x < ltm_result.cols; x++) {
            for (int c = 0; c < 3; c++) {
                float v = l_ptr[x*3 + c];
                int bin = std::min(255, static_cast<int>(v * 255.0f));
                float frac = v * 255.0f - bin;
                int next_bin = std::min(255, bin + 1);

                // Linear interpolation
                f_ptr[x*3 + c] = curves[c][bin] * (1 - frac) + curves[c][next_bin] * frac;
            }
        }
    }

    // Final error
    cv::Mat final_diff;
    cv::absdiff(final_result, target_f, final_diff);
    cv::Scalar final_mean = cv::mean(final_diff);
    float final_error = (final_mean[0] + final_mean[1] + final_mean[2]) / 3.0f;

    std::cout << "Local TM + Global Curve error: " << (final_error * 100.0f) << "%" << std::endl;

    // Save comparison
    cv::Mat result_8u, target_8u;
    final_result.convertTo(result_8u, CV_8UC3, 255.0);
    camera_jpeg.convertTo(target_8u, CV_8UC3);

    cv::Mat comparison;
    cv::hconcat(target_8u, result_8u, comparison);
    cv::imwrite("tmp/var/tune/ltm_compare.png", comparison);
    std::cout << "\nSaved: tmp/var/tune/ltm_compare.png" << std::endl;

    return 0;
}
