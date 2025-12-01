// compare_curves.cpp
// Compare base curves estimated from:
//   1. Embedded preview (1616×1080)
//   2. Sidecar JPG at preview size (downsampled)
//   3. Sidecar JPG at higher resolution
//
// Tests whether resolution affects curve estimation

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

// Estimate per-channel curves from data→reference mapping
// Same algorithm as RAWS but extracted for testing
void estimateCurve(const cv::Mat& data_linear, const cv::Mat& reference, float* curve)
{
    // Initialize to identity
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = i / 255.0f;

    // Resize data to match reference size
    cv::Mat data_resized;
    cv::resize(data_linear, data_resized, reference.size(), 0, 0, cv::INTER_AREA);

    // Convert data to 8-bit gamma for binning
    cv::Mat data_clamped;
    cv::max(data_resized, 0.0f, data_clamped);
    cv::min(data_clamped, 1.0f, data_clamped);

    cv::Mat data_gamma;
    cv::pow(data_clamped, 1.0f / 2.2f, data_gamma);

    cv::Mat data_8u;
    data_gamma.convertTo(data_8u, CV_8UC3, 255.0);

    // Accumulate per-channel bins
    std::vector<double> sum[3];
    std::vector<double> count[3];
    for (int c = 0; c < 3; c++) {
        sum[c].resize(256, 0.0);
        count[c].resize(256, 0.0);
    }

    for (int y = 0; y < data_8u.rows; y++) {
        const uchar* d_ptr = data_8u.ptr<uchar>(y);
        const uchar* r_ptr = reference.ptr<uchar>(y);
        for (int x = 0; x < data_8u.cols; x++) {
            for (int c = 0; c < 3; c++) {
                int bin = d_ptr[x * 3 + c];
                sum[c][bin] += r_ptr[x * 3 + c];
                count[c][bin] += 1.0;
            }
        }
    }

    // Compute curve values
    for (int c = 0; c < 3; c++) {
        for (int i = 0; i < 256; i++) {
            if (count[c][i] > 0)
                curve[c * 256 + i] = static_cast<float>(sum[c][i] / count[c][i]) / 255.0f;
        }
        // Monotonicity
        for (int i = 1; i < 256; i++) {
            if (curve[c * 256 + i] < curve[c * 256 + i - 1])
                curve[c * 256 + i] = curve[c * 256 + i - 1];
        }
        // Smoothing
        std::vector<float> smoothed(256);
        smoothed[0] = curve[c * 256 + 0];
        smoothed[255] = curve[c * 256 + 255];
        for (int i = 1; i < 255; i++)
            smoothed[i] = 0.25f * curve[c * 256 + i - 1] + 0.5f * curve[c * 256 + i] + 0.25f * curve[c * 256 + i + 1];
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = smoothed[i];
    }
}

float curveMSE(const float* a, const float* b, int channel) {
    float sum = 0.0f;
    for (int i = 0; i < 256; i++) {
        float diff = a[channel * 256 + i] - b[channel * 256 + i];
        sum += diff * diff;
    }
    return sum / 256.0f;
}

float curveMaxDiff(const float* a, const float* b, int channel) {
    float maxDiff = 0.0f;
    for (int i = 0; i < 256; i++) {
        float diff = std::abs(a[channel * 256 + i] - b[channel * 256 + i]);
        if (diff > maxDiff) maxDiff = diff;
    }
    return maxDiff;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <data_linear.exr> <preview.jpg> <sidecar.jpg>\n";
        std::cerr << "\nCompares curves from preview vs sidecar at various resolutions.\n";
        std::cerr << "\nTo get data_linear.exr, use labs to save scene-linear stage:\n";
        std::cerr << "  (need to add --save-linear option)\n";
        return 1;
    }

    // Load images
    cv::Mat data = cv::imread(argv[1], cv::IMREAD_UNCHANGED);
    cv::Mat preview = cv::imread(argv[2]);
    cv::Mat sidecar = cv::imread(argv[3]);

    if (data.empty() || preview.empty() || sidecar.empty()) {
        std::cerr << "Error loading images\n";
        std::cerr << "  data: " << (data.empty() ? "FAILED" : "OK") << "\n";
        std::cerr << "  preview: " << (preview.empty() ? "FAILED" : "OK") << "\n";
        std::cerr << "  sidecar: " << (sidecar.empty() ? "FAILED" : "OK") << "\n";
        return 1;
    }

    // Convert data to float if needed
    if (data.type() != CV_32FC3) {
        data.convertTo(data, CV_32FC3, 1.0/255.0);
    }

    std::cout << "Data (linear): " << data.cols << "x" << data.rows << "\n";
    std::cout << "Preview: " << preview.cols << "x" << preview.rows << "\n";
    std::cout << "Sidecar: " << sidecar.cols << "x" << sidecar.rows << "\n\n";

    // Estimate curves at different resolutions
    float curve_preview[768];
    float curve_sidecar_small[768];
    float curve_sidecar_medium[768];
    float curve_sidecar_full[768];

    // 1. Using embedded preview (current method)
    std::cout << "Estimating curve from preview (" << preview.cols << "x" << preview.rows << ")...\n";
    estimateCurve(data, preview, curve_preview);

    // 2. Using sidecar downsampled to preview size
    cv::Mat sidecar_small;
    cv::resize(sidecar, sidecar_small, preview.size(), 0, 0, cv::INTER_AREA);
    std::cout << "Estimating curve from sidecar@preview (" << sidecar_small.cols << "x" << sidecar_small.rows << ")...\n";
    estimateCurve(data, sidecar_small, curve_sidecar_small);

    // 3. Using sidecar at medium resolution
    cv::Mat sidecar_medium;
    float scale = 2048.0f / std::max(sidecar.cols, sidecar.rows);
    cv::resize(sidecar, sidecar_medium, cv::Size(), scale, scale, cv::INTER_AREA);
    std::cout << "Estimating curve from sidecar@2048 (" << sidecar_medium.cols << "x" << sidecar_medium.rows << ")...\n";
    estimateCurve(data, sidecar_medium, curve_sidecar_medium);

    // 4. Using sidecar at full resolution
    std::cout << "Estimating curve from sidecar@full (" << sidecar.cols << "x" << sidecar.rows << ")...\n";
    estimateCurve(data, sidecar, curve_sidecar_full);

    // Compare curves
    std::cout << "\n";
    std::cout << "Curve comparison (MSE / MaxDiff vs preview-based curve):\n";
    std::cout << "─────────────────────────────────────────────────────────\n";
    std::cout << std::fixed << std::setprecision(6);

    const char* channels[] = {"Blue", "Green", "Red"};
    const char* sources[] = {"Sidecar@preview", "Sidecar@2048", "Sidecar@full"};
    float* curves[] = {curve_sidecar_small, curve_sidecar_medium, curve_sidecar_full};

    for (int s = 0; s < 3; s++) {
        std::cout << "\n" << sources[s] << " vs Preview:\n";
        for (int c = 0; c < 3; c++) {
            float mse = curveMSE(curve_preview, curves[s], c);
            float maxd = curveMaxDiff(curve_preview, curves[s], c);
            std::cout << "  " << std::setw(6) << channels[c]
                      << "  MSE=" << std::setw(10) << mse
                      << "  MaxDiff=" << std::setw(8) << maxd
                      << (maxd > 0.01 ? " ***" : (maxd > 0.005 ? " *" : ""))
                      << "\n";
        }
    }

    // Print sample curve points for visual inspection
    std::cout << "\n\nSample curve points (input → output):\n";
    std::cout << "─────────────────────────────────────────────────────────\n";
    int samples[] = {0, 32, 64, 128, 192, 224, 255};
    for (int i : samples) {
        std::cout << "Input " << std::setw(3) << i << ":  ";
        std::cout << "Preview=[" << std::setprecision(3)
                  << curve_preview[i] << "," << curve_preview[256+i] << "," << curve_preview[512+i] << "]  ";
        std::cout << "Sidecar@full=["
                  << curve_sidecar_full[i] << "," << curve_sidecar_full[256+i] << "," << curve_sidecar_full[512+i] << "]\n";
    }

    return 0;
}
