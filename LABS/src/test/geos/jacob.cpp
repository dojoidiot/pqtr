// jacob.cpp
// Jacobian estimation: dial→feature sensitivity matrix (45×23)
//
// For each dial d, perturbs by ±ε and measures Δfeature/Δdial.
// The resulting Jacobian J[d][f] tells how much feature f changes
// when dial d moves by 1 unit.
//
// Uses:
//   1. Gradient-informed SPSA (take steps in high-impact directions)
//   2. Feature weight adjustment (low-sensitivity features are unreachable)
//   3. Understanding dial→feature relationships

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <geos.hpp>
#include "../../../src/main/part/geos/diff.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <array>
#include <cmath>

using namespace geos::internal;

constexpr int DIAL_COUNT = 45;
constexpr float EPSILON = 0.05f;  // Perturbation size (5% of range)

// Dial names for output
const char* DIAL_NAMES[DIAL_COUNT] = {
    "exposure", "temperature", "tint",                    // 0-2: ColorCorrection
    "contrast", "highlights", "shadows",                   // 3-5: ToneMapping curve
    "toe_pivot", "shoulder_pivot", "black", "white",       // 6-9: ToneMapping clip
    "vibrance", "saturation", "colourDensity",             // 10-12: GlobalColor
    "shadow_temp", "shadow_tint",                          // 13-14: SplitTone shadows
    "highlight_temp", "highlight_tint",                    // 15-16: SplitTone highlights
    "red_H", "red_S", "red_L",                             // 17-19: SelectiveColour
    "orange_H", "orange_S", "orange_L",                    // 20-22
    "yellow_H", "yellow_S", "yellow_L",                    // 23-25
    "green_H", "green_S", "green_L",                       // 26-28
    "cyan_H", "cyan_S", "cyan_L",                          // 29-31
    "blue_H", "blue_S", "blue_L",                          // 32-34
    "purple_H", "purple_S", "purple_L",                    // 35-37
    "magenta_H", "magenta_S", "magenta_L",                 // 38-40
    "sharpen_amount", "sharpen_radius",                    // 41-42: Detail
    "denoise_luma", "denoise_chroma"                       // 43-44
};

using Theta = std::array<float, DIAL_COUNT>;
using Jacobian = std::array<std::array<float, STYLE_DIM>, DIAL_COUNT>;  // [dial][feature]

// Read current dial values from link
void readDials(pipe::Body::Link& link, Theta& theta)
{
    theta[0] = link.colorCorrection().exposure().get();
    theta[1] = link.colorCorrection().whiteBalance().temperature();
    theta[2] = link.colorCorrection().whiteBalance().tint();
    theta[3] = link.toneMapping().contrast().get();
    theta[4] = link.toneMapping().curveAdjustment().highlights().get();
    theta[5] = link.toneMapping().curveAdjustment().shadows().get();
    theta[6] = link.toneMapping().curveAdjustment().toePivot().get();
    theta[7] = link.toneMapping().curveAdjustment().shoulderPivot().get();
    theta[8] = link.toneMapping().clippingPoint().black().get();
    theta[9] = link.toneMapping().clippingPoint().white().get();
    theta[10] = link.globalColor().vibrance().get();
    theta[11] = link.globalColor().saturation().get();
    theta[12] = link.globalColor().colourDensity().get();
    theta[13] = link.splitTone().shadows().temperature();
    theta[14] = link.splitTone().shadows().tint();
    theta[15] = link.splitTone().highlights().temperature();
    theta[16] = link.splitTone().highlights().tint();

    auto readHSL = [&](pipe::Body::Link::SelectiveColour::HslAdjust& hsl, int base) {
        theta[base + 0] = hsl.hue();
        theta[base + 1] = hsl.saturation();
        theta[base + 2] = hsl.luminance();
    };
    readHSL(link.selectiveColour().red(), 17);
    readHSL(link.selectiveColour().orange(), 20);
    readHSL(link.selectiveColour().yellow(), 23);
    readHSL(link.selectiveColour().green(), 26);
    readHSL(link.selectiveColour().cyan(), 29);
    readHSL(link.selectiveColour().blue(), 32);
    readHSL(link.selectiveColour().purple(), 35);
    readHSL(link.selectiveColour().magenta(), 38);

    theta[41] = link.detail().sharpen().amount();
    theta[42] = link.detail().sharpen().radius();
    theta[43] = link.detail().denoise().luminance().get();
    theta[44] = link.detail().denoise().chroma().get();
}

// Write dial values to link
void writeDials(pipe::Body::Link& link, const Theta& theta)
{
    link.colorCorrection().exposure().set(theta[0]);
    link.colorCorrection().whiteBalance().temperature(theta[1]);
    link.colorCorrection().whiteBalance().tint(theta[2]);
    link.toneMapping().contrast().set(theta[3]);
    link.toneMapping().curveAdjustment().highlights().set(theta[4]);
    link.toneMapping().curveAdjustment().shadows().set(theta[5]);
    link.toneMapping().curveAdjustment().toePivot().set(theta[6]);
    link.toneMapping().curveAdjustment().shoulderPivot().set(theta[7]);
    link.toneMapping().clippingPoint().black().set(theta[8]);
    link.toneMapping().clippingPoint().white().set(theta[9]);
    link.globalColor().vibrance().set(theta[10]);
    link.globalColor().saturation().set(theta[11]);
    link.globalColor().colourDensity().set(theta[12]);
    link.splitTone().shadows().temperature(theta[13]);
    link.splitTone().shadows().tint(theta[14]);
    link.splitTone().highlights().temperature(theta[15]);
    link.splitTone().highlights().tint(theta[16]);

    auto writeHSL = [&](pipe::Body::Link::SelectiveColour::HslAdjust& hsl, int base) {
        hsl.hue(theta[base + 0]);
        hsl.saturation(theta[base + 1]);
        hsl.luminance(theta[base + 2]);
    };
    writeHSL(link.selectiveColour().red(), 17);
    writeHSL(link.selectiveColour().orange(), 20);
    writeHSL(link.selectiveColour().yellow(), 23);
    writeHSL(link.selectiveColour().green(), 26);
    writeHSL(link.selectiveColour().cyan(), 29);
    writeHSL(link.selectiveColour().blue(), 32);
    writeHSL(link.selectiveColour().purple(), 35);
    writeHSL(link.selectiveColour().magenta(), 38);

    link.detail().sharpen().amount(theta[41]);
    link.detail().sharpen().radius(theta[42]);
    link.detail().denoise().luminance().set(theta[43]);
    link.detail().denoise().chroma().set(theta[44]);
}

// Extract features for current body state
StyleFeatures extractFeatures(pipe::Body& body)
{
    cv::UMat view = body.view();
    cv::UMat proxy = resizeProxy(view);
    return extractStyleFromBGR(proxy);
}

// Compute Jacobian for a single image
Jacobian computeJacobian(pipe::Body& body, pipe::Body::Link& link)
{
    Jacobian J;

    // Start from neutral
    Theta neutral;
    neutral.fill(0.5f);
    writeDials(link, neutral);

    // Get baseline features
    StyleFeatures baseline = extractFeatures(body);

    // For each dial, perturb ±ε and measure gradient
    for (int d = 0; d < DIAL_COUNT; d++)
    {
        Theta theta = neutral;

        // Forward perturbation
        theta[d] = 0.5f + EPSILON;
        writeDials(link, theta);
        StyleFeatures fwd = extractFeatures(body);

        // Backward perturbation
        theta[d] = 0.5f - EPSILON;
        writeDials(link, theta);
        StyleFeatures bwd = extractFeatures(body);

        // Central difference: (f(x+ε) - f(x-ε)) / (2ε)
        for (int f = 0; f < STYLE_DIM; f++)
        {
            J[d][f] = (fwd.v[f] - bwd.v[f]) / (2 * EPSILON);
        }

        // Reset to neutral
        theta[d] = 0.5f;
    }

    // Restore neutral state
    writeDials(link, neutral);

    return J;
}

// Save Jacobian to JSON
bool saveJacobian(const Jacobian& J, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"description\": \"Jacobian: dial→feature sensitivity (45×23)\",\n";
    file << "  \"epsilon\": " << EPSILON << ",\n";
    file << "  \"dial_names\": [";
    for (int d = 0; d < DIAL_COUNT; d++)
    {
        if (d > 0) file << ", ";
        file << "\"" << DIAL_NAMES[d] << "\"";
    }
    file << "],\n";
    file << "  \"feature_names\": [";
    for (int f = 0; f < STYLE_DIM; f++)
    {
        if (f > 0) file << ", ";
        file << "\"" << FEATURE_NAMES[f] << "\"";
    }
    file << "],\n";
    file << "  \"matrix\": [\n";
    for (int d = 0; d < DIAL_COUNT; d++)
    {
        file << "    [";
        for (int f = 0; f < STYLE_DIM; f++)
        {
            if (f > 0) file << ", ";
            file << std::fixed << std::setprecision(6) << J[d][f];
        }
        file << "]";
        if (d < DIAL_COUNT - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    return true;
}

// Print top influences for each feature
void printTopInfluences(const Jacobian& J)
{
    std::cout << "\n=== Top Dial Influences per Feature ===" << std::endl;

    for (int f = 0; f < STYLE_DIM; f++)
    {
        // Find top 3 dials by absolute sensitivity
        std::array<std::pair<float, int>, DIAL_COUNT> ranked;
        for (int d = 0; d < DIAL_COUNT; d++)
        {
            ranked[d] = {std::abs(J[d][f]), d};
        }
        std::sort(ranked.begin(), ranked.end(), std::greater<>());

        std::cout << std::setw(12) << FEATURE_NAMES[f] << ": ";
        for (int i = 0; i < 3; i++)
        {
            if (ranked[i].first < 0.001f) break;
            std::cout << DIAL_NAMES[ranked[i].second];
            std::cout << "(" << std::fixed << std::setprecision(3) << J[ranked[i].second][f] << ") ";
        }
        std::cout << std::endl;
    }
}

// Print top features for each dial
void printTopFeatures(const Jacobian& J)
{
    std::cout << "\n=== Top Feature Impacts per Dial ===" << std::endl;

    for (int d = 0; d < DIAL_COUNT; d++)
    {
        // Compute total impact (sum of absolute sensitivities)
        float totalImpact = 0.0f;
        for (int f = 0; f < STYLE_DIM; f++)
        {
            totalImpact += std::abs(J[d][f]);
        }

        if (totalImpact < 0.01f)
        {
            std::cout << std::setw(15) << DIAL_NAMES[d] << ": (low impact)" << std::endl;
            continue;
        }

        // Find top 2 features by absolute sensitivity
        std::array<std::pair<float, int>, STYLE_DIM> ranked;
        for (int f = 0; f < STYLE_DIM; f++)
        {
            ranked[f] = {std::abs(J[d][f]), f};
        }
        std::sort(ranked.begin(), ranked.end(), std::greater<>());

        std::cout << std::setw(15) << DIAL_NAMES[d] << ": ";
        for (int i = 0; i < 2; i++)
        {
            if (ranked[i].first < 0.001f) break;
            std::cout << FEATURE_NAMES[ranked[i].second];
            std::cout << "(" << std::fixed << std::setprecision(3) << J[d][ranked[i].second] << ") ";
        }
        std::cout << std::endl;
    }
}

int main(int argc, char** argv)
{
    std::cout << "=== Jacobian Estimation (dial→feature) ===" << std::endl;

    std::string arwPath = "var/pics/DSC00144.ARW";
    std::string outPath = "etc/jacob.json";

    if (argc > 1) arwPath = argv[1];
    if (argc > 2) outPath = argv[2];

    std::cout << "Image: " << arwPath << std::endl;
    std::cout << "Output: " << outPath << std::endl;

    // Setup pipeline
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(arwPath));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
    if (!head)
    {
        std::cerr << "Failed to open " << arwPath << std::endl;
        return 1;
    }

    pipe::Body& body = head->body(720);
    pipe::Body::Link& link = body.add("jacob");

    // Apply base curve if available
    if (head->hasBaseCurve())
    {
        link.baseCurve().setCurve(head->baseCurve());
        std::cout << "[jacob] Base curve applied" << std::endl;
    }

    // Compute Jacobian
    std::cout << "\nComputing Jacobian (" << DIAL_COUNT << " dials × " << STYLE_DIM << " features)..." << std::endl;
    std::cout << "Perturbation: ε = " << EPSILON << std::endl;

    Jacobian J = computeJacobian(body, link);

    // Save to JSON
    if (saveJacobian(J, outPath))
    {
        std::cout << "\n✓ Jacobian saved to: " << outPath << std::endl;
    }
    else
    {
        std::cerr << "✗ Failed to save Jacobian" << std::endl;
        return 1;
    }

    // Print analysis
    printTopInfluences(J);
    printTopFeatures(J);

    return 0;
}
