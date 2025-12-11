// diag.cpp - Feature loss diagnostic tool
// Analyzes per-feature errors between target and optimized outputs

#include <iostream>
#include <filesystem>
#include <vector>
#include <array>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "diff.hpp"

namespace fs = std::filesystem;
using namespace geos::internal;

// Accumulate errors across all images
struct ErrorStats {
    std::array<double, STYLE_DIM> sumError{};
    std::array<double, STYLE_DIM> sumErrorSq{};
    std::array<double, STYLE_DIM> sumTarget{};
    std::array<double, STYLE_DIM> sumCandidate{};
    int count = 0;

    void add(const StyleFeatures& target, const StyleFeatures& candidate) {
        auto errors = perFeatureError(target, candidate);
        for (int i = 0; i < STYLE_DIM; i++) {
            sumError[i] += errors[i];
            sumErrorSq[i] += errors[i] * errors[i];
            sumTarget[i] += target.v[i];
            sumCandidate[i] += candidate.v[i];
        }
        count++;
    }

    float meanError(int i) const { return count > 0 ? sumError[i] / count : 0; }
    float meanTarget(int i) const { return count > 0 ? sumTarget[i] / count : 0; }
    float meanCandidate(int i) const { return count > 0 ? sumCandidate[i] / count : 0; }
};

int main(int argc, char** argv)
{
    std::string picDir = "tmp/var/pics";
    if (argc > 1) picDir = argv[1];

    // Check if we have batch outputs or just single output
    std::vector<std::pair<std::string, std::string>> imagePairs;  // (head.png, tail.png)

    // First check for batch outputs in tmp/var/pics/<name>/
    if (fs::exists(picDir) && fs::is_directory(picDir)) {
        for (const auto& entry : fs::directory_iterator(picDir)) {
            if (entry.is_directory()) {
                std::string headPath = entry.path().string() + "/head.png";
                std::string tailPath = entry.path().string() + "/tail.png";
                if (fs::exists(headPath) && fs::exists(tailPath)) {
                    imagePairs.push_back({headPath, tailPath});
                }
            }
        }
    }

    // Also check single output in tmp/var/tune/
    std::string singleHead = "tmp/var/tune/head.png";
    std::string singleTail = "tmp/var/tune/tail.png";
    if (fs::exists(singleHead) && fs::exists(singleTail)) {
        imagePairs.push_back({singleHead, singleTail});
    }

    if (imagePairs.empty()) {
        std::cerr << "No head.png/tail.png pairs found.\n";
        std::cerr << "Run tune first to generate outputs.\n";
        return 1;
    }

    std::cout << "=== FEATURE LOSS DIAGNOSTIC ===" << std::endl;
    std::cout << "Analyzing " << imagePairs.size() << " image pair(s)\n" << std::endl;

    ErrorStats stats;

    for (const auto& [headPath, tailPath] : imagePairs) {
        std::string name = fs::path(headPath).parent_path().filename().string();
        std::cout << "Processing " << name << "..." << std::endl;

        cv::Mat headMat = cv::imread(headPath);
        cv::Mat tailMat = cv::imread(tailPath);

        if (headMat.empty() || tailMat.empty()) {
            std::cerr << "  Failed to load images" << std::endl;
            continue;
        }

        // Extract features
        cv::UMat headU, tailU;
        headMat.copyTo(headU);
        tailMat.copyTo(tailU);

        StyleFeatures targetFeat = extractStyleFromBGR(headU);
        StyleFeatures outputFeat = extractStyleFromBGR(tailU);

        // Add to stats
        stats.add(targetFeat, outputFeat);

        // Print per-image analysis
        float loss = geodesicLoss(targetFeat, outputFeat);
        printf("  %s: loss=%.4f (%.2f%%)\n", name.c_str(), loss, loss * 100.0f);

        // Print feature breakdown for this image
        printFeatureAnalysis(targetFeat, outputFeat);
    }

    // Print aggregate analysis
    if (stats.count > 1) {
        std::cout << "\n=== AGGREGATE ANALYSIS (" << stats.count << " images) ===" << std::endl;
        std::cout << "Feature      Avg_Target  Avg_Output  Avg_Error   Curr_W  Need_Boost?" << std::endl;
        std::cout << "------------------------------------------------------------------------" << std::endl;

        for (int i = 0; i < STYLE_DIM; i++) {
            float avgErr = stats.meanError(i);
            float currW = FEATURE_WEIGHTS[i];

            // Heuristic: if avg error > 0.02 and weight < 3, might need boost
            bool needBoost = (avgErr > 0.02f && currW < 3.0f);

            printf("%-12s  %7.4f     %7.4f     %7.4f     %5.2f   %s\n",
                   FEATURE_NAMES[i],
                   stats.meanTarget(i),
                   stats.meanCandidate(i),
                   avgErr,
                   currW,
                   needBoost ? "YES" : "");
        }
        std::cout << "------------------------------------------------------------------------" << std::endl;
    }

    std::cout << "\nNote: High error = optimizer failing to match this feature" << std::endl;
    std::cout << "Consider increasing weights for features marked 'YES'.\n" << std::endl;

    return 0;
}
