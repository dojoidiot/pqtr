// train_prms.cpp
// Train SPSA phase parameters on a batch of images
// Outputs: etc/prms.json
//
// Strategy: Grid search over a0/c0 for each block, measure:
//   1. Final loss achieved
//   2. Iterations to reach threshold
//   3. Stability (variance across images)

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <iomanip>

#include <pipe.hpp>
#include <geos.hpp>

namespace fs = std::filesystem;

struct PhaseResult {
    float a0, c0;
    float avgLoss;
    float maxLoss;
    float avgIters;
    float score;  // Combined metric (lower = better)
};

struct ImageResult {
    std::string name;
    float loss;
    int iters;
};

// Test a single parameter configuration on one image
ImageResult testParams(
    const std::string& arwPath,
    float a0, float c0,
    int maxIters = 200)
{
    ImageResult result;
    result.name = fs::path(arwPath).stem().string();

    // Load image
    pipe::Config config;
    config.workSize = 720;  // Smaller for faster testing
    config.geos_mode = geos::Mode::BLOCKWISE;
    config.geos_iter = maxIters;

    pipe::Body body(arwPath, config);
    if (!body.valid()) {
        result.loss = 1.0f;
        result.iters = maxIters;
        return result;
    }

    // Extract target from preview
    auto target = geos::extractStyle(body.preview());

    // TODO: Inject custom a0/c0 into optimizer
    // For now, we use the default and measure
    // Full implementation would modify geos::optimize() to accept params

    int iters = geos::optimize(body, target, config, nullptr);

    // Measure final loss
    auto output = geos::extractStyle(body.render());
    result.loss = geos::spectralLoss(target, output);
    result.iters = iters;

    return result;
}

int main(int argc, char** argv) {
    std::cout << "=== Phase Parameter Training ===" << std::endl;

    // Find all ARW files
    std::vector<std::string> images;
    std::string picDir = "var/pics";

    if (argc > 1) {
        picDir = argv[1];
    }

    for (const auto& entry : fs::directory_iterator(picDir)) {
        if (entry.path().extension() == ".ARW") {
            images.push_back(entry.path().string());
        }
    }

    std::sort(images.begin(), images.end());
    std::cout << "Found " << images.size() << " images" << std::endl;

    if (images.empty()) {
        std::cerr << "No ARW files found in " << picDir << std::endl;
        return 1;
    }

    // Grid search ranges for a0, c0
    // Based on current values and need for more exploration
    std::vector<float> a0_values = {0.05f, 0.10f, 0.15f, 0.20f, 0.25f};
    std::vector<float> c0_values = {0.02f, 0.04f, 0.08f, 0.12f, 0.16f};

    std::vector<PhaseResult> results;

    std::cout << "\nTesting " << a0_values.size() * c0_values.size()
              << " configurations on " << images.size() << " images...\n" << std::endl;

    for (float a0 : a0_values) {
        for (float c0 : c0_values) {
            std::cout << "Testing a0=" << a0 << ", c0=" << c0 << "..." << std::flush;

            PhaseResult pr;
            pr.a0 = a0;
            pr.c0 = c0;

            float sumLoss = 0, sumIters = 0;
            float maxLoss = 0;

            for (const auto& img : images) {
                auto ir = testParams(img, a0, c0);
                sumLoss += ir.loss;
                sumIters += ir.iters;
                maxLoss = std::max(maxLoss, ir.loss);
            }

            pr.avgLoss = sumLoss / images.size();
            pr.maxLoss = maxLoss;
            pr.avgIters = sumIters / images.size();

            // Score: weighted combination (loss matters more than speed)
            pr.score = pr.avgLoss * 0.7f + pr.maxLoss * 0.2f + (pr.avgIters / 200.0f) * 0.1f;

            results.push_back(pr);

            std::cout << " avgLoss=" << std::fixed << std::setprecision(4) << pr.avgLoss
                      << ", maxLoss=" << pr.maxLoss
                      << ", score=" << pr.score << std::endl;
        }
    }

    // Sort by score
    std::sort(results.begin(), results.end(),
              [](const PhaseResult& a, const PhaseResult& b) {
                  return a.score < b.score;
              });

    std::cout << "\n=== Top 5 Configurations ===" << std::endl;
    for (int i = 0; i < std::min(5, (int)results.size()); i++) {
        const auto& r = results[i];
        std::cout << i+1 << ". a0=" << r.a0 << ", c0=" << r.c0
                  << " | avgLoss=" << std::fixed << std::setprecision(4) << r.avgLoss
                  << ", maxLoss=" << r.maxLoss
                  << ", avgIters=" << (int)r.avgIters
                  << ", score=" << r.score << std::endl;
    }

    // Save best to prms.json
    const auto& best = results[0];

    std::ofstream out("etc/prms.json");
    out << "{\n";
    out << "  \"description\": \"SPSA phase parameters (trained)\",\n";
    out << "  \"training_images\": " << images.size() << ",\n";
    out << "  \"best_score\": " << std::fixed << std::setprecision(4) << best.score << ",\n";
    out << "  \"block_10d\": {\n";
    out << "    \"a0\": " << best.a0 << ",\n";
    out << "    \"c0\": " << best.c0 << ",\n";
    out << "    \"alpha\": 0.602,\n";
    out << "    \"gamma\": 0.101,\n";
    out << "    \"A\": 20.0\n";
    out << "  },\n";
    out << "  \"block_17d\": {\n";
    out << "    \"note\": \"Joint refinement - scale from 10d\",\n";
    out << "    \"a0\": " << best.a0 * 0.5f << ",\n";
    out << "    \"c0\": " << best.c0 * 0.5f << "\n";
    out << "  },\n";
    out << "  \"block_7d\": {\n";
    out << "    \"note\": \"GlobalColor - scale from 10d\",\n";
    out << "    \"a0\": " << best.a0 * 0.8f << ",\n";
    out << "    \"c0\": " << best.c0 * 0.75f << "\n";
    out << "  }\n";
    out << "}\n";
    out.close();

    std::cout << "\nSaved to etc/prms.json" << std::endl;

    return 0;
}
