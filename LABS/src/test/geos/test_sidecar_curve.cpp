// test_sidecar_curve.cpp
// Compare base curves from embedded preview vs full-res sidecar JPG
// Uses RAWS to decode RAW, then estimates curves from both sources

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include "raws.hpp"
#include "tool.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

// Estimate per-channel curves (same as RAWS)
static int g_debug_counts[3][256];

void estimateCurve(const cv::UMat& data, const cv::Mat& reference, float* curve, bool debug = false)
{
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = i / 255.0f;

    cv::Mat data_cpu;
    data.copyTo(data_cpu);

    cv::Mat data_resized;
    cv::resize(data_cpu, data_resized, reference.size(), 0, 0, cv::INTER_AREA);

    cv::Mat data_clamped;
    cv::max(data_resized, 0.0f, data_clamped);
    cv::min(data_clamped, 1.0f, data_clamped);

    cv::Mat data_gamma;
    cv::pow(data_clamped, 1.0f / 2.2f, data_gamma);

    cv::Mat data_8u;
    data_gamma.convertTo(data_8u, CV_8UC3, 255.0);

    if (debug) {
        // Check distribution of data_8u values
        double minVal, maxVal;
        cv::minMaxLoc(data_8u.reshape(1), &minVal, &maxVal);
        std::cout << "\n[DEBUG] data_8u range: " << minVal << " - " << maxVal << "\n";
    }

    std::vector<double> sum[3], count[3];
    for (int c = 0; c < 3; c++) {
        sum[c].resize(256, 0.0);
        count[c].resize(256, 0.0);
    }

    for (int y = 0; y < data_8u.rows; y++) {
        const uchar* d = data_8u.ptr<uchar>(y);
        const uchar* r = reference.ptr<uchar>(y);
        for (int x = 0; x < data_8u.cols; x++) {
            for (int c = 0; c < 3; c++) {
                int bin = d[x * 3 + c];
                sum[c][bin] += r[x * 3 + c];
                count[c][bin] += 1.0;
            }
        }
    }

    if (debug) {
        for (int c = 0; c < 3; c++)
            for (int i = 0; i < 256; i++)
                g_debug_counts[c][i] = (int)count[c][i];
        std::cout << "[DEBUG] Green channel counts: bin0=" << (int)count[1][0]
                  << " bin128=" << (int)count[1][128]
                  << " bin254=" << (int)count[1][254]
                  << " bin255=" << (int)count[1][255] << "\n";
    }

    for (int c = 0; c < 3; c++) {
        for (int i = 0; i < 256; i++)
            if (count[c][i] > 0)
                curve[c * 256 + i] = float(sum[c][i] / count[c][i]) / 255.0f;
        for (int i = 1; i < 256; i++)
            if (curve[c * 256 + i] < curve[c * 256 + i - 1])
                curve[c * 256 + i] = curve[c * 256 + i - 1];
        std::vector<float> sm(256);
        sm[0] = curve[c * 256]; sm[255] = curve[c * 256 + 255];
        for (int i = 1; i < 255; i++)
            sm[i] = 0.25f * curve[c*256+i-1] + 0.5f * curve[c*256+i] + 0.25f * curve[c*256+i+1];
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = sm[i];
    }
}

float curveL2(const float* a, const float* b) {
    float sum = 0;
    for (int i = 0; i < 768; i++) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return std::sqrt(sum / 768);
}

float curveMaxDiff(const float* a, const float* b) {
    float m = 0;
    for (int i = 0; i < 768; i++)
        m = std::max(m, std::abs(a[i] - b[i]));
    return m;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <image.ARW> <image.JPG>\n";
        return 1;
    }

    // Decode RAW
    std::cout << "Decoding " << argv[1] << "...\n";
    pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(argv[1]));
    if (!sink) {
        std::cerr << "Failed to read RAW file\n";
        return 1;
    }
    raws::Result res = raws::decode(*sink);
    if (!res.success) {
        std::cerr << "Failed to decode RAW\n";
        return 1;
    }

    // Get embedded preview
    cv::Mat preview;
    res.preview.copyTo(preview);
    std::cout << "Embedded preview: " << preview.cols << "x" << preview.rows;
    std::cout << " type=" << preview.type() << " (CV_8UC3=" << CV_8UC3 << ")\n";

    // Check preview pixel values at bright area
    cv::Scalar prevMean = cv::mean(preview);
    std::cout << "Preview mean BGR: " << prevMean[0] << "," << prevMean[1] << "," << prevMean[2] << "\n";

    // Check data values
    cv::Mat data_cpu;
    res.data.copyTo(data_cpu);
    cv::Scalar dataMean = cv::mean(data_cpu);
    std::cout << "Data mean BGR: " << dataMean[0] << "," << dataMean[1] << "," << dataMean[2] << "\n";
    std::cout << "Scene-linear data: " << res.data.cols << "x" << res.data.rows << "\n";

    // Load sidecar JPG
    cv::Mat sidecar = cv::imread(argv[2]);
    if (sidecar.empty()) {
        std::cerr << "Failed to load sidecar JPG\n";
        return 1;
    }
    std::cout << "Sidecar JPG: " << sidecar.cols << "x" << sidecar.rows << "\n\n";

    // Curves from RAWS (embedded preview)
    float curve_raws[768];
    std::copy(res.baseCurve, res.baseCurve + 768, curve_raws);

    // Direct debug of RAWS result
    std::cout << "\n[DEBUG] Direct from res.baseCurve:\n";
    std::cout << "  res.baseCurve[0]=" << res.baseCurve[0] << "\n";
    std::cout << "  res.baseCurve[255]=" << res.baseCurve[255] << "\n";
    std::cout << "  res.baseCurve[256]=" << res.baseCurve[256] << "\n";
    std::cout << "  res.baseCurve[511]=" << res.baseCurve[511] << "\n";
    std::cout << "  res.baseCurve[767]=" << res.baseCurve[767] << "\n";
    std::cout << "  res.hasBaseCurve=" << res.hasBaseCurve << "\n";

    // Re-estimate from preview (should match RAWS)
    float curve_preview[768];
    estimateCurve(res.data, preview, curve_preview, true);  // debug=true

    // Estimate from sidecar at preview size
    cv::Mat sidecar_small;
    cv::resize(sidecar, sidecar_small, preview.size(), 0, 0, cv::INTER_AREA);
    float curve_sidecar_small[768];
    estimateCurve(res.data, sidecar_small, curve_sidecar_small);

    // Estimate from sidecar at full resolution
    float curve_sidecar_full[768];
    estimateCurve(res.data, sidecar, curve_sidecar_full);

    // Compare (preview as baseline, ignore buggy RAWS curve)
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Curve comparison (Preview as baseline):\n";
    std::cout << "─────────────────────────────────────────────\n";
    std::cout << "Source                    L2 vs Preview  MaxDiff\n";
    std::cout << "─────────────────────────────────────────────\n";
    std::cout << "Sidecar@preview           " << curveL2(curve_preview, curve_sidecar_small)
              << "         " << curveMaxDiff(curve_preview, curve_sidecar_small) << "\n";
    std::cout << "Sidecar@full              " << curveL2(curve_preview, curve_sidecar_full)
              << "         " << curveMaxDiff(curve_preview, curve_sidecar_full) << "\n";
    std::cout << "─────────────────────────────────────────────\n";

    // Debug: print raw index layout
    std::cout << "\nRAWS curve array (raw indices):\n";
    std::cout << "curve[0]=" << curve_raws[0] << " curve[255]=" << curve_raws[255] << " (Blue)\n";
    std::cout << "curve[256]=" << curve_raws[256] << " curve[511]=" << curve_raws[511] << " (Green)\n";
    std::cout << "curve[512]=" << curve_raws[512] << " curve[767]=" << curve_raws[767] << " (Red)\n";

    // Check if Result struct has the right size
    std::cout << "\nResult.baseCurve size check: " << sizeof(res.baseCurve) << " bytes = "
              << sizeof(res.baseCurve)/sizeof(float) << " floats\n";

    // Print full green channel curve
    std::cout << "\nRAWS Green channel curve (every 32 values):\n";
    for (int i = 0; i <= 255; i += 32) {
        std::cout << "G[" << i << "]=" << std::setprecision(3) << curve_raws[256+i] << "  ";
    }
    std::cout << "\n";

    std::cout << "\nRe-estimated Green channel curve (every 32 values):\n";
    for (int i = 0; i <= 255; i += 32) {
        std::cout << "G[" << i << "]=" << std::setprecision(3) << curve_preview[256+i] << "  ";
    }
    std::cout << "\n";

    // Show sample curve differences (preview vs sidecar)
    std::cout << "\nCurve samples (green channel, input→output):\n";
    std::cout << "Input   Preview  Side@Prev  Side@Full  Diff(P-SF)\n";
    for (int i : {0, 32, 64, 96, 128, 160, 192, 224, 255}) {
        float p = curve_preview[256 + i];
        float ss = curve_sidecar_small[256 + i];
        float sf = curve_sidecar_full[256 + i];
        std::cout << std::setw(5) << i << "   "
                  << std::setw(7) << p << "  "
                  << std::setw(7) << ss << "  "
                  << std::setw(7) << sf << "    "
                  << std::setw(7) << (p - sf)
                  << (std::abs(p - sf) > 0.01 ? " ***" : "")
                  << "\n";
    }

    return 0;
}
