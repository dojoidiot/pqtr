// compare_sources.cpp
// Compare 23D features from embedded preview vs sidecar JPG
// Tests whether resolution affects feature extraction

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <array>

// Feature extraction (simplified from diff.cpp)
constexpr int STYLE_DIM = 23;

struct Features {
    std::array<float, STYLE_DIM> v;
};

// Extract features from BGR image
Features extractFeatures(const cv::UMat& bgr) {
    Features f;
    f.v.fill(0.0f);

    cv::Mat img;
    bgr.copyTo(img);

    // Convert to Lab for color features
    cv::Mat lab;
    cv::cvtColor(img, lab, cv::COLOR_BGR2Lab);

    // Convert to grayscale for luminance
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    gray.convertTo(gray, CV_32F, 1.0/255.0);

    // Split Lab channels
    std::vector<cv::Mat> labChannels;
    cv::split(lab, labChannels);
    cv::Mat L = labChannels[0];
    cv::Mat a = labChannels[1];
    cv::Mat b = labChannels[2];
    L.convertTo(L, CV_32F, 1.0/255.0);
    a.convertTo(a, CV_32F, 1.0/255.0);
    b.convertTo(b, CV_32F, 1.0/255.0);

    // Compute chroma
    cv::Mat C;
    cv::sqrt((a - 0.5).mul(a - 0.5) + (b - 0.5).mul(b - 0.5), C);
    C *= 2.0; // Scale to ~[0,1]

    // SVD on reshaped image (simplified)
    cv::Mat flat;
    img.reshape(1, img.rows * img.cols).convertTo(flat, CV_32F, 1.0/255.0);
    cv::SVD svd(flat, cv::SVD::NO_UV);
    f.v[0] = svd.w.at<float>(0) / (img.rows * img.cols);
    f.v[1] = svd.w.at<float>(1) / (img.rows * img.cols);
    f.v[2] = svd.w.at<float>(2) / (img.rows * img.cols);

    // Mean L, C
    f.v[3] = cv::mean(L)[0];  // mu_L
    f.v[4] = cv::mean(C)[0];  // mu_C

    // Std L, C
    cv::Scalar meanL, stdL, meanC, stdC;
    cv::meanStdDev(L, meanL, stdL);
    cv::meanStdDev(C, meanC, stdC);
    f.v[5] = stdL[0];  // std_L
    f.v[6] = stdC[0];  // std_C

    // Skewness L (simplified)
    cv::Mat Lcentered = L - meanL[0];
    cv::Mat Lcubed = Lcentered.mul(Lcentered).mul(Lcentered);
    float skew = cv::mean(Lcubed)[0] / (stdL[0] * stdL[0] * stdL[0] + 1e-6f);
    f.v[7] = skew;

    // Covariances (simplified - just correlation proxy)
    f.v[8] = 0.0f;  // cov_LC placeholder
    f.v[9] = 0.0f;  // cov_HC placeholder

    // Mean a, b (color cast)
    f.v[10] = cv::mean(a)[0];  // mu_a
    f.v[11] = cv::mean(b)[0];  // mu_b

    // Luminance percentiles
    std::vector<float> Lvec;
    L.reshape(1, 1).copyTo(Lvec);
    std::sort(Lvec.begin(), Lvec.end());
    int n = Lvec.size();
    f.v[12] = Lvec[n * 10 / 100];   // L_p10
    f.v[13] = Lvec[n * 25 / 100];   // L_p25
    f.v[14] = Lvec[n * 75 / 100];   // L_p75
    f.v[15] = Lvec[n * 90 / 100];   // L_p90

    // Chroma percentiles
    std::vector<float> Cvec;
    C.reshape(1, 1).copyTo(Cvec);
    std::sort(Cvec.begin(), Cvec.end());
    f.v[16] = Cvec[n * 50 / 100];   // C_p50
    f.v[17] = Cvec[n * 90 / 100];   // C_p90

    // Shadow chroma (mean C where L < L_p25)
    float L_p25 = f.v[13];
    cv::Mat shadowMask = L < L_p25;
    f.v[18] = cv::mean(C, shadowMask)[0];  // C_shadow

    // Shadow color
    f.v[19] = cv::mean(a, shadowMask)[0];  // a_shadow
    f.v[20] = cv::mean(b, shadowMask)[0];  // b_shadow

    // Highlight color (L > L_p75)
    float L_p75 = f.v[14];
    cv::Mat highlightMask = L > L_p75;
    f.v[21] = cv::mean(a, highlightMask)[0];  // a_highlight
    f.v[22] = cv::mean(b, highlightMask)[0];  // b_highlight

    return f;
}

const char* FEATURE_NAMES[STYLE_DIM] = {
    "sigma1", "sigma2", "sigma3",
    "mu_L", "mu_C", "std_L", "std_C", "skew_L",
    "cov_LC", "cov_HC",
    "mu_a", "mu_b",
    "L_p10", "L_p25", "L_p75", "L_p90",
    "C_p50", "C_p90", "C_shadow",
    "a_shadow", "b_shadow", "a_highlight", "b_highlight"
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <embedded_preview.jpg> <sidecar.jpg> [working_size]\n";
        std::cerr << "\nExtracts 23D features from both images and compares them.\n";
        std::cerr << "Use exiftool to extract embedded preview first:\n";
        std::cerr << "  exiftool -PreviewImage -b image.ARW > preview.jpg\n";
        return 1;
    }

    int workingSize = (argc > 3) ? std::atoi(argv[3]) : 1080;

    // Load images
    cv::Mat preview = cv::imread(argv[1]);
    cv::Mat sidecar = cv::imread(argv[2]);

    if (preview.empty() || sidecar.empty()) {
        std::cerr << "Error loading images\n";
        return 1;
    }

    std::cout << "Embedded preview: " << preview.cols << "x" << preview.rows << "\n";
    std::cout << "Sidecar JPG: " << sidecar.cols << "x" << sidecar.rows << "\n";
    std::cout << "Working size: " << workingSize << "\n\n";

    // Resize both to working size (maintaining aspect)
    auto resizeTo = [](const cv::Mat& img, int maxDim) {
        float scale = float(maxDim) / std::max(img.cols, img.rows);
        cv::Mat resized;
        cv::resize(img, resized, cv::Size(), scale, scale, cv::INTER_AREA);
        return resized;
    };

    cv::Mat previewResized = resizeTo(preview, workingSize);
    cv::Mat sidecarResized = resizeTo(sidecar, workingSize);
    cv::Mat sidecarFull = resizeTo(sidecar, 2048);  // Higher res comparison

    std::cout << "After resize to " << workingSize << ":\n";
    std::cout << "  Preview: " << previewResized.cols << "x" << previewResized.rows << "\n";
    std::cout << "  Sidecar: " << sidecarResized.cols << "x" << sidecarResized.rows << "\n";
    std::cout << "  Sidecar@2048: " << sidecarFull.cols << "x" << sidecarFull.rows << "\n\n";

    // Convert to UMat and extract features
    cv::UMat previewU, sidecarU, sidecarFullU;
    previewResized.copyTo(previewU);
    sidecarResized.copyTo(sidecarU);
    sidecarFull.copyTo(sidecarFullU);

    Features fPreview = extractFeatures(previewU);
    Features fSidecar = extractFeatures(sidecarU);
    Features fSidecarFull = extractFeatures(sidecarFullU);

    // Compare
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Feature comparison (all at " << workingSize << "px working size):\n";
    std::cout << "─────────────────────────────────────────────────────────────────\n";
    std::cout << std::setw(14) << "Feature"
              << std::setw(10) << "Preview"
              << std::setw(10) << "Sidecar"
              << std::setw(10) << "Side@2K"
              << std::setw(10) << "Δ(P-S)"
              << std::setw(10) << "Δ(P-S2K)"
              << "\n";
    std::cout << "─────────────────────────────────────────────────────────────────\n";

    float totalDiff = 0.0f;
    float totalDiff2K = 0.0f;

    for (int i = 0; i < STYLE_DIM; i++) {
        float diff = fPreview.v[i] - fSidecar.v[i];
        float diff2K = fPreview.v[i] - fSidecarFull.v[i];
        totalDiff += diff * diff;
        totalDiff2K += diff2K * diff2K;

        std::cout << std::setw(14) << FEATURE_NAMES[i]
                  << std::setw(10) << fPreview.v[i]
                  << std::setw(10) << fSidecar.v[i]
                  << std::setw(10) << fSidecarFull.v[i]
                  << std::setw(10) << diff
                  << std::setw(10) << diff2K
                  << (std::abs(diff) > 0.01 ? " *" : "")
                  << "\n";
    }

    std::cout << "─────────────────────────────────────────────────────────────────\n";
    std::cout << "L2 distance (Preview vs Sidecar@" << workingSize << "): " << std::sqrt(totalDiff) << "\n";
    std::cout << "L2 distance (Preview vs Sidecar@2048): " << std::sqrt(totalDiff2K) << "\n";

    return 0;
}
