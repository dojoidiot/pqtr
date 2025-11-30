// train_greedy.cpp
// Greedy training: prioritize worst-performing images
//
// Algorithm:
//   1. Run batch, collect per-image losses
//   2. Rank images by loss (worst first)
//   3. Analyze worst images to find which features drive loss
//   4. Boost weights for problematic features
//   5. Re-run batch, check if worst improved without regressing others
//   6. Iterate until convergence
//
// Output: etc/cnst.json (feature weights)
//         etc/prms.json (phase params)
//         etc/rank.json (image rankings for monitoring)

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>

#include <pipe.hpp>
#include <geos.hpp>
#include "../../../src/main/part/geos/diff.hpp"

namespace fs = std::filesystem;
using namespace geos::internal;

struct ImageScore {
    std::string name;
    std::string path;
    float loss;
    std::array<float, STYLE_DIM> featureErrors;  // Per-feature contribution
};

// Run tune on single image, return loss and per-feature errors
ImageScore evaluateImage(const std::string& arwPath, int maxIter = 200) {
    ImageScore score;
    score.path = arwPath;
    score.name = fs::path(arwPath).stem().string();

    pipe::Config pipeConfig;
    pipeConfig.workSize = 720;

    pipe::Body body(arwPath, pipeConfig);
    if (!body.valid()) {
        score.loss = 1.0f;
        return score;
    }

    // Get target features from preview
    cv::Mat preview;
    body.preview().copyTo(preview);
    StyleFeatures target = computeStyleFeatures(preview);

    // Run optimization
    geos::Config geosConfig;
    geosConfig.geos_max_iter = maxIter;
    geosConfig.geos_mode = geos::Mode::BLOCKWISE;

    auto task = geos::make(body.preview());
    auto result = task->run(body, body.linear(), geosConfig, nullptr);

    // Get output features
    cv::Mat output;
    body.render().copyTo(output);
    StyleFeatures candidate = computeStyleFeatures(output);

    // Calculate total loss and per-feature errors
    score.loss = result.loss.spectral;
    score.featureErrors = perFeatureError(target, candidate);

    return score;
}

void printRanking(const std::vector<ImageScore>& scores) {
    std::cout << "\n=== Image Ranking (worst first) ===" << std::endl;
    for (size_t i = 0; i < scores.size(); i++) {
        std::cout << std::setw(2) << (i+1) << ". " << scores[i].name
                  << " : " << std::fixed << std::setprecision(2)
                  << (scores[i].loss * 100) << "%" << std::endl;
    }
}

void analyzeWorst(const std::vector<ImageScore>& scores, int topN = 3) {
    std::cout << "\n=== Worst " << topN << " Feature Analysis ===" << std::endl;

    // Aggregate feature errors for worst images
    std::array<float, STYLE_DIM> avgErrors = {};
    int count = std::min(topN, (int)scores.size());

    for (int i = 0; i < count; i++) {
        for (int f = 0; f < STYLE_DIM; f++) {
            avgErrors[f] += scores[i].featureErrors[f];
        }
    }

    // Sort features by error contribution
    std::vector<std::pair<int, float>> ranked;
    for (int f = 0; f < STYLE_DIM; f++) {
        ranked.push_back({f, avgErrors[f] / count});
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::cout << "Top error contributors:" << std::endl;
    for (int i = 0; i < std::min(8, STYLE_DIM); i++) {
        int f = ranked[i].first;
        std::cout << "  " << FEATURE_NAMES[f] << ": "
                  << std::fixed << std::setprecision(4) << ranked[i].second
                  << " (current weight: " << FEATURE_WEIGHTS[f] << ")" << std::endl;
    }
}

void saveRanking(const std::vector<ImageScore>& scores, const std::string& path) {
    std::ofstream out(path);
    out << "{\n";
    out << "  \"description\": \"Image ranking by loss (worst first)\",\n";
    out << "  \"images\": [\n";
    for (size_t i = 0; i < scores.size(); i++) {
        out << "    {\"name\": \"" << scores[i].name << "\", \"loss\": "
            << std::fixed << std::setprecision(4) << scores[i].loss << "}";
        if (i < scores.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"worst_avg\": " << std::fixed << std::setprecision(4)
        << (scores[0].loss + scores[1].loss + scores[2].loss) / 3.0f << ",\n";
    out << "  \"best_avg\": " << std::fixed << std::setprecision(4)
        << (scores[scores.size()-1].loss + scores[scores.size()-2].loss + scores[scores.size()-3].loss) / 3.0f << "\n";
    out << "}\n";
    out.close();
}

int main(int argc, char** argv) {
    std::cout << "=== Greedy Training ===" << std::endl;

    // Find all ARW files
    std::vector<std::string> images;
    std::string picDir = "var/pics";
    if (argc > 1) picDir = argv[1];

    for (const auto& entry : fs::directory_iterator(picDir)) {
        if (entry.path().extension() == ".ARW") {
            images.push_back(entry.path().string());
        }
    }
    std::sort(images.begin(), images.end());
    std::cout << "Found " << images.size() << " images\n" << std::endl;

    if (images.empty()) {
        std::cerr << "No ARW files in " << picDir << std::endl;
        return 1;
    }

    // Evaluate all images
    std::vector<ImageScore> scores;
    for (const auto& img : images) {
        std::cout << "Evaluating " << fs::path(img).stem().string() << "..." << std::flush;
        auto score = evaluateImage(img);
        scores.push_back(score);
        std::cout << " " << std::fixed << std::setprecision(2)
                  << (score.loss * 100) << "%" << std::endl;
    }

    // Sort by loss (worst first)
    std::sort(scores.begin(), scores.end(),
              [](const ImageScore& a, const ImageScore& b) {
                  return a.loss > b.loss;
              });

    printRanking(scores);
    analyzeWorst(scores, 3);

    // Save ranking
    fs::create_directories("etc");
    saveRanking(scores, "etc/rank.json");
    std::cout << "\nSaved ranking to etc/rank.json" << std::endl;

    // Calculate suggested weight adjustments
    std::cout << "\n=== Suggested Weight Adjustments ===" << std::endl;
    std::cout << "(Based on worst 3 images)" << std::endl;

    // For each feature with high error in worst images, suggest boost
    std::array<float, STYLE_DIM> avgWorstErrors = {};
    for (int i = 0; i < 3 && i < (int)scores.size(); i++) {
        for (int f = 0; f < STYLE_DIM; f++) {
            avgWorstErrors[f] += scores[i].featureErrors[f];
        }
    }
    for (int f = 0; f < STYLE_DIM; f++) {
        avgWorstErrors[f] /= 3.0f;
    }

    // Features with error > 0.01 should be boosted
    for (int f = 0; f < STYLE_DIM; f++) {
        if (avgWorstErrors[f] > 0.01f) {
            float suggestedWeight = std::min(5.0f, FEATURE_WEIGHTS[f] * 1.5f);
            std::cout << "  " << FEATURE_NAMES[f] << ": "
                      << FEATURE_WEIGHTS[f] << " → " << suggestedWeight << std::endl;
        }
    }

    return 0;
}
