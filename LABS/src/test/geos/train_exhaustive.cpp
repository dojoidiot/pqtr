// train_exhaustive.cpp
// Level 1: Exhaustive optimizer training (time is not a constraint)
//
// Purpose: Find the BEST possible configuration by trying everything.
// This runs as slow as needed to find global optima.
//
// Outputs:
//   etc/cnst.json  - Optimal feature weights
//   etc/prms.json  - Optimal SPSA phase params
//   etc/trace.json - Full convergence traces for analysis
//
// Strategy:
//   1. For each image, run to full convergence (no early stopping)
//   2. Record per-iteration loss and dial movements
//   3. Grid search over feature weights and phase params
//   4. Select configuration that minimizes worst-case loss
//
// Usage:
//   make -f Makefile.tune train-exhaustive
//   (Then go get coffee - this takes hours)

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <chrono>
#include <map>

#include <pipe.hpp>
#include <geos.hpp>
#include "../../../src/main/part/geos/diff.hpp"
#include "../../../src/main/part/geos/spsa.hpp"

namespace fs = std::filesystem;
using namespace geos::internal;

// Configuration being tested
struct TestConfig {
    std::array<float, STYLE_DIM> weights;
    float a0_10d, c0_10d;   // Phase params for BLOCK_10D
    float a0_17d, c0_17d;   // Phase params for BLOCK_17D
};

// Result for one image with one config
struct ImageResult {
    std::string name;
    float finalLoss;
    int iterations;
    float convergenceTime;  // seconds
    std::vector<float> lossTrace;  // Per-iteration loss
    std::array<float, STYLE_DIM> featureErrors;
};

// Result for one config across all images
struct ConfigResult {
    TestConfig config;
    float avgLoss;
    float maxLoss;       // Worst-case (most important!)
    float medianLoss;
    float avgIterations;
    std::vector<ImageResult> images;
};

// Run exhaustive optimization on one image (no early stopping)
ImageResult runExhaustive(
    const std::string& arwPath,
    const TestConfig& testCfg,
    int maxIter = 2000)  // Much higher limit
{
    ImageResult result;
    result.name = fs::path(arwPath).stem().string();

    auto startTime = std::chrono::steady_clock::now();

    // Setup
    pipe::Config pipeConfig;
    pipeConfig.workSize = 1080;  // Full quality for training

    pipe::Body body(arwPath, pipeConfig);
    if (!body.valid()) {
        result.finalLoss = 1.0f;
        return result;
    }

    // Get target
    cv::Mat preview;
    body.preview().copyTo(preview);
    StyleFeatures target = computeStyleFeatures(preview);

    // Run with custom weights (TODO: need to inject weights dynamically)
    // For now, use compiled weights but track full convergence
    geos::Config geosConfig;
    geosConfig.geos_max_iter = maxIter;
    geosConfig.geos_threshold = 0.001f;  // Very tight threshold
    geosConfig.geos_mode = geos::Mode::FULL_35D;  // Full dial space
    geosConfig.skip_lut = true;  // Pure dial optimization

    // Capture per-iteration loss
    auto progressCb = [&result](const geos::Progress& p) -> bool {
        result.lossTrace.push_back(p.loss.spectral);
        return true;  // Continue
    };

    auto task = geos::make(body.preview());
    auto runResult = task->run(body, body.linear(), geosConfig, progressCb);

    // Get final state
    cv::Mat output;
    body.render().copyTo(output);
    StyleFeatures candidate = computeStyleFeatures(output);

    result.finalLoss = runResult.loss.spectral;
    result.iterations = runResult.geos_iterations;
    result.featureErrors = perFeatureError(target, candidate);

    auto endTime = std::chrono::steady_clock::now();
    result.convergenceTime = std::chrono::duration<float>(endTime - startTime).count();

    return result;
}

// Analyze which features drive worst-case losses
void analyzeFeatureDrivers(const std::vector<ConfigResult>& results) {
    std::cout << "\n=== Feature Error Analysis (worst images) ===" << std::endl;

    // Aggregate feature errors from worst-performing images
    std::array<float, STYLE_DIM> totalErrors = {};
    int worstCount = 0;

    for (const auto& cfg : results) {
        // Top 3 worst images per config
        std::vector<ImageResult> sorted = cfg.images;
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.finalLoss > b.finalLoss; });

        for (int i = 0; i < std::min(3, (int)sorted.size()); i++) {
            for (int f = 0; f < STYLE_DIM; f++) {
                totalErrors[f] += sorted[i].featureErrors[f];
            }
            worstCount++;
        }
    }

    // Rank features by error contribution
    std::vector<std::pair<int, float>> ranked;
    for (int f = 0; f < STYLE_DIM; f++) {
        ranked.push_back({f, totalErrors[f] / worstCount});
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::cout << "Features driving worst-case losses:" << std::endl;
    for (int i = 0; i < STYLE_DIM; i++) {
        int f = ranked[i].first;
        float err = ranked[i].second;
        float weight = FEATURE_WEIGHTS[f];
        float suggested = std::min(5.0f, weight * (1.0f + err * 10.0f));

        std::cout << std::setw(12) << FEATURE_NAMES[f] << ": "
                  << std::fixed << std::setprecision(4) << err
                  << " | weight: " << weight
                  << " → " << suggested << std::endl;
    }
}

void saveResults(const ConfigResult& best, const std::string& etcDir) {
    fs::create_directories(etcDir);

    // Save cnst.json
    {
        std::ofstream out(etcDir + "/cnst.json");
        out << "{\n";
        out << "  \"description\": \"Feature weights (exhaustive training)\",\n";
        out << "  \"training_mode\": \"exhaustive\",\n";
        out << "  \"avg_loss\": " << std::fixed << std::setprecision(4) << best.avgLoss << ",\n";
        out << "  \"max_loss\": " << best.maxLoss << ",\n";
        out << "  \"weights\": {\n";
        for (int i = 0; i < STYLE_DIM; i++) {
            out << "    \"" << FEATURE_NAMES[i] << "\": " << best.config.weights[i];
            if (i < STYLE_DIM - 1) out << ",";
            out << "\n";
        }
        out << "  }\n";
        out << "}\n";
        out.close();
        std::cout << "Saved: " << etcDir << "/cnst.json" << std::endl;
    }

    // Save prms.json
    {
        std::ofstream out(etcDir + "/prms.json");
        out << "{\n";
        out << "  \"description\": \"SPSA phase params (exhaustive training)\",\n";
        out << "  \"block_10d\": {\n";
        out << "    \"a0\": " << best.config.a0_10d << ",\n";
        out << "    \"c0\": " << best.config.c0_10d << "\n";
        out << "  },\n";
        out << "  \"block_17d\": {\n";
        out << "    \"a0\": " << best.config.a0_17d << ",\n";
        out << "    \"c0\": " << best.config.c0_17d << "\n";
        out << "  }\n";
        out << "}\n";
        out.close();
        std::cout << "Saved: " << etcDir << "/prms.json" << std::endl;
    }

    // Save per-image traces
    {
        std::ofstream out(etcDir + "/trace.json");
        out << "{\n";
        out << "  \"images\": [\n";
        for (size_t i = 0; i < best.images.size(); i++) {
            const auto& img = best.images[i];
            out << "    {\n";
            out << "      \"name\": \"" << img.name << "\",\n";
            out << "      \"final_loss\": " << img.finalLoss << ",\n";
            out << "      \"iterations\": " << img.iterations << ",\n";
            out << "      \"time_seconds\": " << img.convergenceTime << ",\n";
            out << "      \"loss_trace\": [";
            // Sample trace (every 10th point to keep file size reasonable)
            for (size_t j = 0; j < img.lossTrace.size(); j += 10) {
                if (j > 0) out << ", ";
                out << std::fixed << std::setprecision(4) << img.lossTrace[j];
            }
            out << "]\n";
            out << "    }";
            if (i < best.images.size() - 1) out << ",";
            out << "\n";
        }
        out << "  ]\n";
        out << "}\n";
        out.close();
        std::cout << "Saved: " << etcDir << "/trace.json" << std::endl;
    }
}

int main(int argc, char** argv) {
    std::cout << "=== Exhaustive Optimizer Training ===" << std::endl;
    std::cout << "(This will take a very long time - that's intentional)" << std::endl;
    std::cout << std::endl;

    // Find images
    std::vector<std::string> images;
    std::string picDir = "var/pics";
    if (argc > 1) picDir = argv[1];

    for (const auto& entry : fs::directory_iterator(picDir)) {
        if (entry.path().extension() == ".ARW") {
            images.push_back(entry.path().string());
        }
    }
    std::sort(images.begin(), images.end());
    std::cout << "Training set: " << images.size() << " images" << std::endl;

    if (images.empty()) {
        std::cerr << "No ARW files in " << picDir << std::endl;
        return 1;
    }

    // Phase 1: Baseline - run with current config to establish reference
    std::cout << "\n=== Phase 1: Baseline Evaluation ===" << std::endl;

    TestConfig baseline;
    baseline.weights = FEATURE_WEIGHTS;
    baseline.a0_10d = DEFAULT_10D.a0;
    baseline.c0_10d = DEFAULT_10D.c0;
    baseline.a0_17d = DEFAULT_17D.a0;
    baseline.c0_17d = DEFAULT_17D.c0;

    ConfigResult baselineResult;
    baselineResult.config = baseline;

    for (const auto& img : images) {
        std::string name = fs::path(img).stem().string();
        std::cout << "Processing " << name << "..." << std::flush;

        auto result = runExhaustive(img, baseline);
        baselineResult.images.push_back(result);

        std::cout << " loss=" << std::fixed << std::setprecision(2)
                  << (result.finalLoss * 100) << "%, "
                  << result.iterations << " iters, "
                  << std::setprecision(1) << result.convergenceTime << "s" << std::endl;
    }

    // Calculate baseline stats
    float sumLoss = 0, maxLoss = 0;
    std::vector<float> losses;
    for (const auto& img : baselineResult.images) {
        sumLoss += img.finalLoss;
        maxLoss = std::max(maxLoss, img.finalLoss);
        losses.push_back(img.finalLoss);
    }
    std::sort(losses.begin(), losses.end());
    baselineResult.avgLoss = sumLoss / images.size();
    baselineResult.maxLoss = maxLoss;
    baselineResult.medianLoss = losses[losses.size() / 2];

    std::cout << "\nBaseline results:" << std::endl;
    std::cout << "  Average loss: " << std::fixed << std::setprecision(2)
              << (baselineResult.avgLoss * 100) << "%" << std::endl;
    std::cout << "  Worst case:   " << (baselineResult.maxLoss * 100) << "%" << std::endl;
    std::cout << "  Median:       " << (baselineResult.medianLoss * 100) << "%" << std::endl;

    // Analyze what's driving worst cases
    std::vector<ConfigResult> allResults = {baselineResult};
    analyzeFeatureDrivers(allResults);

    // For now, save baseline as best (Phase 2 would iterate on weights)
    // TODO: Implement grid search over weights and phase params
    saveResults(baselineResult, "etc");

    std::cout << "\n=== Training Complete ===" << std::endl;
    std::cout << "Next: Review trace.json, adjust weights, re-run" << std::endl;

    return 0;
}
