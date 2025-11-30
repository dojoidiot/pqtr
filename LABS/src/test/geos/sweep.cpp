// sweep.cpp
// Diagnostic: Sweep individual dials to understand loss landscape
//
// For a "washed out" image, this answers:
//   - Does the loss actually decrease when contrast increases?
//   - Is there a clear gradient toward the optimal value?
//   - Are there local minima trapping the optimizer?
//
// Usage: make -f Makefile.tune sweep
//        (Outputs to tmp/var/sweep/)

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>

#include <pipe.hpp>
#include <geos.hpp>
#include "../../../src/main/part/geos/diff.hpp"

namespace fs = std::filesystem;
using namespace geos::internal;

// Dial indices (from pipe/link.hpp)
constexpr int DIAL_EXPOSURE = 0;
constexpr int DIAL_TEMPERATURE = 1;
constexpr int DIAL_TINT = 2;
constexpr int DIAL_CONTRAST = 3;
constexpr int DIAL_HIGHLIGHTS = 4;
constexpr int DIAL_SHADOWS = 5;
constexpr int DIAL_TOE = 6;
constexpr int DIAL_SHOULDER = 7;
constexpr int DIAL_BLACK = 8;
constexpr int DIAL_WHITE = 9;
constexpr int DIAL_VIBRANCE = 10;
constexpr int DIAL_SATURATION = 11;

const char* DIAL_NAMES[] = {
    "exposure", "temperature", "tint",
    "contrast", "highlights", "shadows", "toe", "shoulder",
    "black", "white", "vibrance", "saturation"
};

struct SweepPoint {
    float dialValue;
    float loss;
    float std_L;    // For diagnosing contrast
    float L_p10;    // For diagnosing blacks
    float L_p75;    // For diagnosing highlights
};

std::vector<SweepPoint> sweepDial(
    pipe::Body& body,
    pipe::Body::Link& link,
    const StyleFeatures& target,
    int dialIndex,
    float start = 0.0f,
    float end = 1.0f,
    int steps = 21)
{
    std::vector<SweepPoint> results;

    // Save original value
    float original = link.get(dialIndex);

    for (int i = 0; i < steps; i++) {
        float value = start + (end - start) * i / (steps - 1);

        // Set dial and render
        link.set(dialIndex, value);
        cv::Mat output;
        body.render().copyTo(output);

        // Compute features
        StyleFeatures candidate = computeStyleFeatures(output);
        float loss = spectralLoss(target, candidate);

        SweepPoint pt;
        pt.dialValue = value;
        pt.loss = loss;
        pt.std_L = candidate.data[IDX_STD_L];
        pt.L_p10 = candidate.data[IDX_L_P10];
        pt.L_p75 = candidate.data[IDX_L_P75];

        results.push_back(pt);
    }

    // Restore original
    link.set(dialIndex, original);

    return results;
}

void printSweepResults(const std::string& dialName,
                       const std::vector<SweepPoint>& results,
                       const StyleFeatures& target) {
    std::cout << "\n=== " << dialName << " sweep ===" << std::endl;
    std::cout << "Target: std_L=" << target.data[IDX_STD_L]
              << ", L_p10=" << target.data[IDX_L_P10]
              << ", L_p75=" << target.data[IDX_L_P75] << std::endl;
    std::cout << std::setw(8) << "value" << " | "
              << std::setw(8) << "loss" << " | "
              << std::setw(8) << "std_L" << " | "
              << std::setw(8) << "L_p10" << " | "
              << std::setw(8) << "L_p75" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    float minLoss = 1.0f;
    float bestValue = 0.5f;
    for (const auto& pt : results) {
        if (pt.loss < minLoss) {
            minLoss = pt.loss;
            bestValue = pt.dialValue;
        }
        std::cout << std::fixed << std::setprecision(2)
                  << std::setw(8) << pt.dialValue << " | "
                  << std::setprecision(4)
                  << std::setw(8) << pt.loss << " | "
                  << std::setw(8) << pt.std_L << " | "
                  << std::setw(8) << pt.L_p10 << " | "
                  << std::setw(8) << pt.L_p75 << std::endl;
    }

    std::cout << "\nBest " << dialName << " = " << bestValue
              << " (loss = " << minLoss << ")" << std::endl;
}

void saveSweepCSV(const std::string& path,
                  const std::string& dialName,
                  const std::vector<SweepPoint>& results) {
    std::ofstream out(path);
    out << "dial_value,loss,std_L,L_p10,L_p75\n";
    for (const auto& pt : results) {
        out << pt.dialValue << "," << pt.loss << ","
            << pt.std_L << "," << pt.L_p10 << "," << pt.L_p75 << "\n";
    }
    out.close();
}

int main(int argc, char** argv) {
    std::cout << "=== Dial Sweep Diagnostic ===" << std::endl;
    std::cout << "(Understand loss landscape for washed-out images)" << std::endl;

    // Use a known washed-out image
    std::string arwPath = "var/pics/DSC01531.ARW";  // "totally washed out"
    if (argc > 1) arwPath = argv[1];

    std::cout << "\nImage: " << arwPath << std::endl;

    // Setup
    pipe::Config config;
    config.workSize = 720;

    pipe::Body body(arwPath, config);
    if (!body.valid()) {
        std::cerr << "Failed to load " << arwPath << std::endl;
        return 1;
    }

    pipe::Body::Link& link = body.linear();

    // Get target features
    cv::Mat preview;
    body.preview().copyTo(preview);
    StyleFeatures target = computeStyleFeatures(preview);

    std::cout << "\nTarget features:" << std::endl;
    std::cout << "  std_L (contrast): " << target.data[IDX_STD_L] << std::endl;
    std::cout << "  L_p10 (blacks):   " << target.data[IDX_L_P10] << std::endl;
    std::cout << "  L_p25 (shadows):  " << target.data[IDX_L_P25] << std::endl;
    std::cout << "  L_p75 (hi-mids):  " << target.data[IDX_L_P75] << std::endl;
    std::cout << "  L_p90 (whites):   " << target.data[IDX_L_P90] << std::endl;

    // Create output directory
    fs::create_directories("tmp/var/sweep");

    // Sweep key dials
    std::vector<int> dialsToSweep = {
        DIAL_CONTRAST, DIAL_HIGHLIGHTS, DIAL_SHADOWS,
        DIAL_TOE, DIAL_SHOULDER, DIAL_BLACK, DIAL_WHITE
    };

    for (int dial : dialsToSweep) {
        auto results = sweepDial(body, link, target, dial);
        printSweepResults(DIAL_NAMES[dial], results, target);
        saveSweepCSV("tmp/var/sweep/" + std::string(DIAL_NAMES[dial]) + ".csv",
                     DIAL_NAMES[dial], results);
    }

    std::cout << "\nSaved sweep data to tmp/var/sweep/*.csv" << std::endl;
    std::cout << "\nInterpretation:" << std::endl;
    std::cout << "- If best value != 0.5 but optimizer stays at 0.5: step size too small" << std::endl;
    std::cout << "- If loss is flat around 0.5: weak gradient, need stronger feature weights" << std::endl;
    std::cout << "- If best value = 0.5: optimizer is correct, problem is elsewhere" << std::endl;

    return 0;
}
