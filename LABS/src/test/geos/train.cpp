// train.cpp - Weight training tool
// Runs tune on all images, collects per-feature errors, suggests optimal weights

#include <iostream>
#include <filesystem>
#include <vector>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include "diff.hpp"

namespace fs = std::filesystem;
using namespace geos::internal;

struct TrainingStats {
    std::array<double, STYLE_DIM> sumError{};
    std::array<double, STYLE_DIM> sumErrorSq{};
    std::array<double, STYLE_DIM> maxError{};
    int count = 0;

    void add(const std::array<float, STYLE_DIM>& errors) {
        for (int i = 0; i < STYLE_DIM; i++) {
            sumError[i] += errors[i];
            sumErrorSq[i] += errors[i] * errors[i];
            maxError[i] = std::max(maxError[i], static_cast<double>(errors[i]));
        }
        count++;
    }

    float mean(int i) const { return count > 0 ? sumError[i] / count : 0; }
    float variance(int i) const {
        if (count < 2) return 0;
        double m = mean(i);
        return (sumErrorSq[i] / count) - (m * m);
    }
    float stddev(int i) const { return std::sqrt(std::max(0.0f, variance(i))); }
};

int main(int argc, char** argv)
{
    std::string picDir = "tmp/var/pics";

    // Collect all head.png/tail.png pairs
    std::vector<std::pair<std::string, std::string>> imagePairs;

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

    // Also check single output
    if (fs::exists("tmp/var/tune/head.png") && fs::exists("tmp/var/tune/tail.png")) {
        imagePairs.push_back({"tmp/var/tune/head.png", "tmp/var/tune/tail.png"});
    }

    if (imagePairs.empty()) {
        std::cerr << "No training data found. Run batch tune first.\n";
        return 1;
    }

    std::cout << "=== WEIGHT TRAINING ===" << std::endl;
    std::cout << "Training on " << imagePairs.size() << " image pair(s)\n" << std::endl;

    TrainingStats stats;
    std::vector<float> losses;

    for (const auto& [headPath, tailPath] : imagePairs) {
        std::string name = fs::path(headPath).parent_path().filename().string();

        cv::Mat headMat = cv::imread(headPath);
        cv::Mat tailMat = cv::imread(tailPath);

        if (headMat.empty() || tailMat.empty()) continue;

        cv::UMat headU, tailU;
        headMat.copyTo(headU);
        tailMat.copyTo(tailU);

        StyleFeatures target = extractStyleFromBGR(headU);
        StyleFeatures output = extractStyleFromBGR(tailU);

        auto errors = perFeatureError(target, output);
        stats.add(errors);

        float loss = geodesicLoss(target, output);
        losses.push_back(loss);

        printf("  %s: %.2f%%\n", name.c_str(), loss * 100.0f);
    }

    // Compute optimal weights based on error distribution
    std::cout << "\n=== ERROR ANALYSIS ===" << std::endl;
    std::cout << "Feature      Mean_Err  Max_Err   Curr_W  Optimal_W  Delta" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    std::array<float, STYLE_DIM> optimalWeights;
    float totalError = 0;

    for (int i = 0; i < STYLE_DIM; i++) {
        totalError += stats.mean(i);
    }

    for (int i = 0; i < STYLE_DIM; i++) {
        float meanErr = stats.mean(i);
        float maxErr = stats.maxError[i];
        float currW = FEATURE_WEIGHTS[i];

        // Optimal weight heuristic:
        // - Higher weight for features with higher mean error (they need more attention)
        // - But cap at 5.0 to avoid over-prioritizing single features
        // - Minimum 0.5 to keep all features relevant
        float errorRatio = totalError > 0 ? meanErr / (totalError / STYLE_DIM) : 1.0f;
        float optW = std::max(0.5f, std::min(5.0f, currW * (1.0f + errorRatio)));

        // For features with very low error, reduce weight
        if (meanErr < 0.005f) {
            optW = std::max(0.5f, currW * 0.8f);
        }

        optimalWeights[i] = optW;
        float delta = optW - currW;

        printf("%-12s  %7.4f   %7.4f   %5.2f   %5.2f      %+.2f%s\n",
               FEATURE_NAMES[i], meanErr, maxErr, currW, optW, delta,
               std::abs(delta) > 0.5f ? " *" : "");
    }

    std::cout << "----------------------------------------------------------------" << std::endl;

    // Compute average loss
    float avgLoss = 0;
    for (float l : losses) avgLoss += l;
    avgLoss /= losses.size();
    printf("\nAverage loss: %.2f%% across %zu images\n", avgLoss * 100.0f, losses.size());

    // Output suggested weights as C++ code
    std::cout << "\n=== SUGGESTED WEIGHTS (copy to diff.hpp) ===" << std::endl;
    std::cout << "constexpr std::array<float, STYLE_DIM> FEATURE_WEIGHTS = {\n";
    for (int i = 0; i < STYLE_DIM; i++) {
        printf("    %.1ff%s  // [%d] %s\n",
               optimalWeights[i],
               i < STYLE_DIM - 1 ? "," : "",
               i, FEATURE_NAMES[i]);
    }
    std::cout << "};\n" << std::endl;

    // Save to cnst.json
    std::string cnstPath = "etc/cnst.json";
    std::ofstream cnstFile(cnstPath);
    if (cnstFile.is_open()) {
        cnstFile << "{\n";
        cnstFile << "  \"description\": \"Feature weights for style matching loss\",\n";
        cnstFile << "  \"training_images\": " << stats.count << ",\n";
        cnstFile << "  \"average_loss\": " << std::fixed << std::setprecision(4) << avgLoss << ",\n";
        cnstFile << "  \"weights\": {\n";
        for (int i = 0; i < STYLE_DIM; i++) {
            cnstFile << "    \"" << FEATURE_NAMES[i] << "\": "
                     << std::fixed << std::setprecision(2) << optimalWeights[i];
            if (i < STYLE_DIM - 1) cnstFile << ",";
            cnstFile << "\n";
        }
        cnstFile << "  }\n";
        cnstFile << "}\n";
        cnstFile.close();
        std::cout << "Saved: " << cnstPath << std::endl;
    }

    return 0;
}
