// bounds.cpp
// Diagnostic: Find achievable feature bounds for each dial configuration
//
// For each feature, finds:
//   - MIN achievable value (all dials at extremes)
//   - MAX achievable value (all dials at extremes)
//   - Range = MAX - MIN
//
// If target feature is outside [MIN, MAX], no optimizer can reach it.
// This identifies pipeline limitations vs optimizer limitations.
//
// Strategy:
//   1. For key contrast dials (contrast, highlights, shadows, toe, shoulder)
//   2. Try extreme combinations (0,0,0...) and (1,1,1...)
//   3. Record min/max for each feature
//   4. Compare to target features
//
// This tells us: "Is the target even reachable?"

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <geos.hpp>
#include "../../../src/main/part/geos/diff.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <cmath>
#include <filesystem>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;
using namespace geos::internal;

struct DialConfig {
    std::string name;
    std::array<float, 7> values;  // contrast, highlights, shadows, toe, shoulder, black, white
};

// Set tone mapping dials on a link
void setToneDials(pipe::Body::Link& link, const std::array<float, 7>& vals) {
    auto& tm = link.toneMapping();
    tm.contrast().set(vals[0]);
    tm.curveAdjustment().highlights().set(vals[1]);
    tm.curveAdjustment().shadows().set(vals[2]);
    tm.curveAdjustment().toePivot().set(vals[3]);
    tm.curveAdjustment().shoulderPivot().set(vals[4]);
    tm.clippingPoint().black().set(vals[5]);
    tm.clippingPoint().white().set(vals[6]);
}

int main(int argc, char** argv) {
    std::cout << "=== Feature Bounds Diagnostic ===" << std::endl;
    std::cout << "(Find achievable min/max for each feature)" << std::endl;

    // Use the worst washed-out image
    std::string arwPath = "var/pics/DSC01531.ARW";
    if (argc > 1) arwPath = argv[1];
    std::cout << "\nImage: " << arwPath << std::endl;

    // Setup
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(arwPath));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
    if (!head) {
        std::cerr << "Failed to open " << arwPath << std::endl;
        return 1;
    }

    pipe::Body& body = head->body(720);
    pipe::Body::Link& link = body.add("bounds");

    // Apply base curve from RAW decoder
    if (head->hasBaseCurve()) {
        link.baseCurve().setCurve(head->baseCurve());
        std::cout << "[bounds] Base curve applied from RAW decoder" << std::endl;
    }

    // Get target features
    cv::UMat targetView = head->view().view();
    cv::Mat targetMat;
    targetView.copyTo(targetMat);
    int targetH = 720 * targetMat.rows / targetMat.cols;
    cv::resize(targetMat, targetMat, cv::Size(720, targetH), 0, 0, cv::INTER_AREA);
    cv::UMat targetU;
    targetMat.copyTo(targetU);
    StyleFeatures target = extractStyleFromBGR(targetU);

    std::cout << "\nTarget feature values:" << std::endl;
    for (int i = 0; i < STYLE_DIM; i++) {
        std::cout << "  " << std::setw(12) << FEATURE_NAMES[i] << ": "
                  << std::fixed << std::setprecision(4) << target.v[i] << std::endl;
    }

    // Define extreme configurations
    // Format: {contrast, highlights, shadows, toe, shoulder, black, white}
    // All dials: 0.5 = neutral
    // contrast: 1.0 = 3x S-curve boost
    // highlights: 1.0 = expand brights
    // shadows: 0.0 = crush darks (lower gamma)
    // black: 0.0 = lift black point
    // white: 1.0 = expand white point
    std::vector<DialConfig> configs = {
        {"neutral",       {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},
        {"contrast_max",  {1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},  // Just S-curve
        {"contrast_min",  {0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},  // Flat S-curve
        {"crush_shad",    {0.5f, 0.5f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f}},  // Crush shadows only
        {"lift_shad",     {0.5f, 0.5f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f}},  // Lift shadows only
        {"expand_hi",     {0.5f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},  // Expand highlights
        {"compress_hi",   {0.5f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}},  // Compress highlights
        {"black_lift",    {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f}},  // Lift black point
        {"black_crush",   {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f}},  // Crush black point
        {"white_expand",  {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f}},  // Expand whites
        {"white_crush",   {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f}},  // Crush whites
        {"s_curve_max",   {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}},  // Max S-curve effect
        {"spread_max",    {1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}},  // Try to spread histogram
        {"all_extreme_1", {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}},  // All max
        {"all_extreme_0", {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},  // All min
    };

    // Track min/max for each feature
    std::array<float, STYLE_DIM> featureMin, featureMax;
    featureMin.fill(1.0f);
    featureMax.fill(0.0f);

    std::cout << "\nTesting " << configs.size() << " dial configurations..." << std::endl;
    std::cout << std::setw(15) << "config" << " | "
              << std::setw(8) << "std_L" << " | "
              << std::setw(8) << "L_p10" << " | "
              << std::setw(8) << "L_p75" << " | "
              << std::setw(8) << "loss" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    pqtr::Hold<geos::Task> task = geos::make(targetU);

    for (const auto& cfg : configs) {
        // Apply dial config
        setToneDials(link, cfg.values);

        // Render
        cv::UMat rendered = body.view();
        cv::Mat renderedMat;
        rendered.copyTo(renderedMat);
        cv::UMat renderedU;
        renderedMat.copyTo(renderedU);

        // Extract features
        StyleFeatures features = extractStyleFromBGR(renderedU);

        // Update min/max
        for (int i = 0; i < STYLE_DIM; i++) {
            featureMin[i] = std::min(featureMin[i], features.v[i]);
            featureMax[i] = std::max(featureMax[i], features.v[i]);
        }

        // Compute loss
        geos::Data loss = task->diff(renderedU);

        std::cout << std::setw(15) << cfg.name << " | "
                  << std::setw(8) << std::fixed << std::setprecision(4) << features.v[IDX_STD_L] << " | "
                  << std::setw(8) << features.v[IDX_L_P10] << " | "
                  << std::setw(8) << features.v[IDX_L_P75] << " | "
                  << std::setw(8) << (loss.spectral * 100) << "%" << std::endl;
    }

    // Reset to neutral
    setToneDials(link, {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f});

    // Print achievable bounds vs target
    std::cout << "\n=== Feature Achievability Analysis ===" << std::endl;
    std::cout << std::setw(12) << "feature" << " | "
              << std::setw(8) << "min" << " | "
              << std::setw(8) << "max" << " | "
              << std::setw(8) << "target" << " | "
              << std::setw(10) << "status" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    int unreachable = 0;
    for (int i = 0; i < STYLE_DIM; i++) {
        bool inRange = (target.v[i] >= featureMin[i] - 0.01f) &&
                       (target.v[i] <= featureMax[i] + 0.01f);
        std::string status = inRange ? "OK" : "UNREACHABLE";
        if (!inRange) unreachable++;

        std::cout << std::setw(12) << FEATURE_NAMES[i] << " | "
                  << std::setw(8) << std::fixed << std::setprecision(4) << featureMin[i] << " | "
                  << std::setw(8) << featureMax[i] << " | "
                  << std::setw(8) << target.v[i] << " | "
                  << std::setw(10) << status << std::endl;
    }

    std::cout << "\nUnreachable features: " << unreachable << "/" << STYLE_DIM << std::endl;

    if (unreachable > 0) {
        std::cout << "\n⚠️  Some target features are outside achievable range!" << std::endl;
        std::cout << "This is a PIPELINE limitation, not an optimizer limitation." << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  1. Reduce weight of unreachable features in cnst.json" << std::endl;
        std::cout << "  2. Add more dials to expand achievable range" << std::endl;
        std::cout << "  3. Accept that perfect match is not possible for this image" << std::endl;
    } else {
        std::cout << "\n✓ All target features are achievable!" << std::endl;
        std::cout << "If optimizer isn't reaching them, it's a prms/cvar problem." << std::endl;
    }

    return 0;
}
