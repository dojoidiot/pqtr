// camera_phase_diag.cpp
// Diagnose camera phase: what's the error AFTER applying the base curve from RAWS?
// This tells us how much work the camera phase dials need to do.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Camera Phase Diagnostic ===" << std::endl;
    std::cout << "File: " << raw_path << std::endl;

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head) {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    // Get scene-linear data
    cv::Mat scene_linear;
    head->data().view().copyTo(scene_linear);

    // Get camera preview (target)
    cv::Mat camera_jpeg;
    head->view().view().copyTo(camera_jpeg);

    // Get base curve from RAWS
    const float* baseCurve = head->baseCurve();
    bool hasCurve = head->hasBaseCurve();

    std::cout << "\nBase curve available: " << (hasCurve ? "YES" : "NO") << std::endl;

    // Print some curve values to verify
    if (hasCurve) {
        std::cout << "\nBase curve samples (input → output):" << std::endl;
        std::cout << "  B: 0→" << baseCurve[0] << ", 64→" << baseCurve[64]
                  << ", 128→" << baseCurve[128] << ", 192→" << baseCurve[192]
                  << ", 255→" << baseCurve[255] << std::endl;
        std::cout << "  G: 0→" << baseCurve[256] << ", 64→" << baseCurve[256+64]
                  << ", 128→" << baseCurve[256+128] << ", 192→" << baseCurve[256+192]
                  << ", 255→" << baseCurve[256+255] << std::endl;
        std::cout << "  R: 0→" << baseCurve[512] << ", 64→" << baseCurve[512+64]
                  << ", 128→" << baseCurve[512+128] << ", 192→" << baseCurve[512+192]
                  << ", 255→" << baseCurve[512+255] << std::endl;
    }

    // Resize scene-linear to match preview
    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    // === Method 1: Simple gamma 2.2 ===
    cv::Mat scene_gamma;
    cv::max(scene_resized, 0.0f, scene_gamma);
    cv::min(scene_gamma, 1.0f, scene_gamma);
    cv::pow(scene_gamma, 1.0f / 2.2f, scene_gamma);

    cv::Mat scene_8u_gamma;
    scene_gamma.convertTo(scene_8u_gamma, CV_8UC3, 255.0);

    cv::Mat diff_gamma;
    cv::absdiff(scene_8u_gamma, camera_jpeg, diff_gamma);
    cv::Scalar mean_gamma = cv::mean(diff_gamma);
    float error_gamma = (mean_gamma[0] + mean_gamma[1] + mean_gamma[2]) / 3.0f / 255.0f * 100.0f;

    std::cout << "\n=== Error Analysis ===" << std::endl;
    std::cout << "Method 1 - Simple gamma 2.2: " << error_gamma << "%" << std::endl;

    // === Method 2: Apply base curve from RAWS ===
    if (hasCurve) {
        // First apply gamma to get to 8-bit space
        cv::Mat scene_clamped;
        cv::max(scene_resized, 0.0f, scene_clamped);
        cv::min(scene_clamped, 1.0f, scene_clamped);
        cv::pow(scene_clamped, 1.0f / 2.2f, scene_clamped);

        // Apply base curve (per channel)
        cv::Mat scene_curved(scene_clamped.size(), CV_32FC3);
        for (int y = 0; y < scene_clamped.rows; y++) {
            const float* src = scene_clamped.ptr<float>(y);
            float* dst = scene_curved.ptr<float>(y);

            for (int x = 0; x < scene_clamped.cols; x++) {
                for (int c = 0; c < 3; c++) {  // B, G, R
                    float v = src[x * 3 + c];
                    int bin = std::min(255, static_cast<int>(v * 255.0f));
                    float frac = v * 255.0f - bin;
                    int next_bin = std::min(255, bin + 1);

                    // Interpolate from base curve
                    float curve_val = baseCurve[c * 256 + bin] * (1 - frac)
                                    + baseCurve[c * 256 + next_bin] * frac;
                    dst[x * 3 + c] = curve_val;
                }
            }
        }

        cv::Mat scene_8u_curved;
        scene_curved.convertTo(scene_8u_curved, CV_8UC3, 255.0);

        cv::Mat diff_curved;
        cv::absdiff(scene_8u_curved, camera_jpeg, diff_curved);
        cv::Scalar mean_curved = cv::mean(diff_curved);
        float error_curved = (mean_curved[0] + mean_curved[1] + mean_curved[2]) / 3.0f / 255.0f * 100.0f;

        std::cout << "Method 2 - Base curve from RAWS: " << error_curved << "%" << std::endl;

        // === Method 3: Oracle curve (estimated fresh from this image pair) ===
        // This shows the theoretical minimum with per-channel curves
        std::vector<double> sum[3];
        std::vector<double> count[3];
        for (int c = 0; c < 3; c++) {
            sum[c].resize(256, 0.0);
            count[c].resize(256, 0.0);
        }

        cv::Mat target_f;
        camera_jpeg.convertTo(target_f, CV_32FC3, 1.0/255.0);

        cv::Mat base_gamma;
        cv::max(scene_resized, 0.0f, base_gamma);
        cv::min(base_gamma, 1.0f, base_gamma);
        cv::pow(base_gamma, 1.0f / 2.2f, base_gamma);

        cv::Mat base_8u;
        base_gamma.convertTo(base_8u, CV_8UC3, 255.0);

        for (int y = 0; y < base_8u.rows; y++) {
            const uchar* b_ptr = base_8u.ptr<uchar>(y);
            const float* t_ptr = target_f.ptr<float>(y);

            for (int x = 0; x < base_8u.cols; x++) {
                for (int c = 0; c < 3; c++) {
                    int bin = b_ptr[x * 3 + c];
                    sum[c][bin] += t_ptr[x * 3 + c];
                    count[c][bin] += 1.0;
                }
            }
        }

        // Build oracle curves
        float oracle_curve[768];
        for (int c = 0; c < 3; c++) {
            for (int i = 0; i < 256; i++) {
                if (count[c][i] > 0)
                    oracle_curve[c * 256 + i] = sum[c][i] / count[c][i];
                else
                    oracle_curve[c * 256 + i] = i / 255.0f;
            }
            // Monotonicity
            for (int i = 1; i < 256; i++) {
                if (oracle_curve[c * 256 + i] < oracle_curve[c * 256 + i - 1])
                    oracle_curve[c * 256 + i] = oracle_curve[c * 256 + i - 1];
            }
        }

        // Apply oracle curve
        cv::Mat scene_oracle(base_gamma.size(), CV_32FC3);
        for (int y = 0; y < base_gamma.rows; y++) {
            const float* src = base_gamma.ptr<float>(y);
            float* dst = scene_oracle.ptr<float>(y);

            for (int x = 0; x < base_gamma.cols; x++) {
                for (int c = 0; c < 3; c++) {
                    float v = src[x * 3 + c];
                    int bin = std::min(255, static_cast<int>(v * 255.0f));
                    float frac = v * 255.0f - bin;
                    int next_bin = std::min(255, bin + 1);
                    float curve_val = oracle_curve[c * 256 + bin] * (1 - frac)
                                    + oracle_curve[c * 256 + next_bin] * frac;
                    dst[x * 3 + c] = curve_val;
                }
            }
        }

        cv::Mat scene_8u_oracle;
        scene_oracle.convertTo(scene_8u_oracle, CV_8UC3, 255.0);

        cv::Mat diff_oracle;
        cv::absdiff(scene_8u_oracle, camera_jpeg, diff_oracle);
        cv::Scalar mean_oracle = cv::mean(diff_oracle);
        float error_oracle = (mean_oracle[0] + mean_oracle[1] + mean_oracle[2]) / 3.0f / 255.0f * 100.0f;

        std::cout << "Method 3 - Oracle curve (per-image): " << error_oracle << "%" << std::endl;
        std::cout << "\n→ Oracle shows minimum achievable with per-channel curves" << std::endl;
        std::cout << "  Remaining " << error_oracle << "% is from: DRO, saturation, hue shifts" << std::endl;

        // Save comparison image
        cv::Mat row1, row2, comparison;
        cv::hconcat(camera_jpeg, scene_8u_gamma, row1);
        cv::hconcat(scene_8u_curved, scene_8u_oracle, row2);
        cv::vconcat(row1, row2, comparison);

        // Add labels
        cv::putText(comparison, "Camera JPEG", cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 2);
        cv::putText(comparison, "Gamma 2.2", cv::Point(camera_jpeg.cols + 10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 2);
        cv::putText(comparison, "Base Curve", cv::Point(10, camera_jpeg.rows + 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 2);
        cv::putText(comparison, "Oracle Curve", cv::Point(camera_jpeg.cols + 10, camera_jpeg.rows + 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255,255,255), 2);

        cv::imwrite("tmp/var/tune/camera_phase_diag.png", comparison);
        std::cout << "\nSaved: tmp/var/tune/camera_phase_diag.png" << std::endl;
        std::cout << "  Top: Camera JPEG | Gamma 2.2" << std::endl;
        std::cout << "  Bottom: Base Curve | Oracle Curve" << std::endl;

        // === Analyze the gap between RAWS curve and oracle ===
        std::cout << "\n=== Curve Comparison (RAWS vs Oracle) ===" << std::endl;
        float max_diff = 0;
        int max_diff_channel = 0;
        int max_diff_bin = 0;

        for (int c = 0; c < 3; c++) {
            for (int i = 0; i < 256; i++) {
                float diff = std::abs(baseCurve[c * 256 + i] - oracle_curve[c * 256 + i]);
                if (diff > max_diff) {
                    max_diff = diff;
                    max_diff_channel = c;
                    max_diff_bin = i;
                }
            }
        }
        const char* channel_names[] = {"Blue", "Green", "Red"};
        std::cout << "Max curve difference: " << (max_diff * 100) << "% at "
                  << channel_names[max_diff_channel] << " bin " << max_diff_bin << std::endl;
    }

    return 0;
}
