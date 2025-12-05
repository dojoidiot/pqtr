// vibe.cpp
// Neural dial predictor - learns dial settings from image features
//
// Stage 1: Camera Vibe - replicate camera JPG appearance
// Stage 2: User Vibe - learn personal editing style (future)
//
// Usage:
//   vibe extract <pics_dir> --out <train.json>   Extract features + run optimizer
//   vibe train <train.json> --out <model.vibe>   Train MLP on extracted data
//   vibe predict <image.ARW> --model <model.vibe>   Predict dials for new image

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <geos.hpp>
#include <data.hpp>
#include "part/vibe/mlp.hpp"
#include "part/geos/diff.hpp"
#include "part/geos/spsa.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace fs = std::filesystem;

// ============================================================
// Vibe class extraction from EXIF
// ============================================================

struct VibeClass {
    std::string flash;      // "off", "on", "auto"
    std::string profile;    // "portrait", "standard", "vivid"

    std::string key() const { return profile + "_" + flash; }
};

// Extract vibe class from ARW file using exiftool
VibeClass extractVibeClass(const std::string& arwPath) {
    VibeClass vc;
    vc.flash = "off";
    vc.profile = "standard";

    // Run exiftool
    std::string cmd = "exiftool -Flash -PictureProfile \"" + arwPath + "\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return vc;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);

        if (line.find("Flash") != std::string::npos) {
            if (line.find("Did not fire") != std::string::npos)
                vc.flash = "off";
            else if (line.find("On") != std::string::npos)
                vc.flash = "on";
            else if (line.find("Auto") != std::string::npos)
                vc.flash = "auto";
        }

        if (line.find("Picture Profile") != std::string::npos) {
            if (line.find("Portrait") != std::string::npos)
                vc.profile = "portrait";
            else if (line.find("Vivid") != std::string::npos)
                vc.profile = "vivid";
            else
                vc.profile = "standard";
        }
    }
    pclose(pipe);

    return vc;
}

// ============================================================
// Training sample
// ============================================================

struct TrainingSample {
    std::string image;
    std::string vibe_class;
    std::array<float, 23> features;
    std::array<float, 45> dials;
};

// ============================================================
// Extract command: process all ARW+JPG pairs
// ============================================================

int cmdExtract(const std::string& picsDir, const std::string& outPath, bool optimize = true) {
    std::cout << "Extracting training data from " << picsDir << "\n";
    if (!optimize) {
        std::cout << "NOTE: Using neutral dials (0.5). Use --optimize for real training data.\n";
    }

    std::vector<TrainingSample> samples;
    int processed = 0, skipped = 0;

    // Collect ARW+JPG pairs first
    std::vector<std::pair<std::string, std::string>> pairs;
    for (const auto& entry : fs::directory_iterator(picsDir)) {
        if (entry.path().extension() != ".ARW") continue;
        std::string arwPath = entry.path().string();
        std::string jpgPath = arwPath.substr(0, arwPath.size() - 4) + ".JPG";
        if (fs::exists(jpgPath)) {
            pairs.push_back({arwPath, jpgPath});
        } else {
            skipped++;
        }
    }
    std::cout << "Found " << pairs.size() << " ARW+JPG pairs\n";

    // Setup config for optimization
    geos::Config config;
    config.skip_edge = true;  // Skip edge optimization for speed
    config.skip_lut = true;
    config.skip_regional = true;
    config.geos_max_iter = 300;  // Quick optimization
    config.geos_threshold = 0.01f;
    config.geos_mode = geos::Mode::FULL_35D;
    config.optimizer = geos::Optimizer::HYBRID;

    const int workingSize = 720;  // Smaller for speed

    // Pre-allocate samples vector for thread safety
    samples.resize(pairs.size());
    std::vector<bool> valid(pairs.size(), false);
    std::atomic<int> completed{0};
    std::mutex printMutex;

#ifdef _OPENMP
    int numThreads = omp_get_max_threads();
    std::cout << "Using " << numThreads << " OpenMP threads\n";
#endif

    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < pairs.size(); i++) {
        const auto& [arwPath, jpgPath] = pairs[i];
        std::string basename = fs::path(arwPath).stem().string();

        // Thread-local pipeline
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();

        // Progress (thread-safe)
        {
            std::lock_guard<std::mutex> lock(printMutex);
            int done = completed.load();
            std::cout << "\r[" << done << "/" << pairs.size() << "] " << basename;
            if (optimize) std::cout << " (optimizing)";
            std::cout << "          " << std::flush;
        }

        // Extract vibe class
        VibeClass vc = extractVibeClass(arwPath);

        // Load camera JPG for target features
        cv::Mat jpg = cv::imread(jpgPath);
        if (jpg.empty()) {
            completed++;
            continue;
        }

        // Extract style features from camera JPG (this is our target)
        cv::UMat jpgU;
        jpg.copyTo(jpgU);
        auto targetStyle = geos::internal::extractStyleFromBGR(
            geos::internal::resizeProxy(jpgU));

        TrainingSample sample;
        sample.image = basename;
        sample.vibe_class = vc.key();
        std::copy(targetStyle.v.begin(), targetStyle.v.end(), sample.features.begin());

        if (optimize) {
            // Load RAW
            pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(arwPath));
            pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
            if (!head) {
                completed++;
                continue;
            }

            // Create body and link
            pipe::Body& body = head->body(workingSize);
            pipe::Body::Link& link = body.add("vibe");

            // Get body size and resize JPG target to match
            cv::Mat initialMat;
            body.view().copyTo(initialMat);
            cv::Size bodySize(initialMat.cols, initialMat.rows);
            cv::Mat jpgResized;
            cv::resize(jpg, jpgResized, bodySize, 0, 0, cv::INTER_AREA);

            // Re-extract features from resized JPG
            cv::UMat jpgResizedU;
            jpgResized.copyTo(jpgResizedU);
            auto targetStyleResized = geos::internal::extractStyleFromBGR(
                geos::internal::resizeProxy(jpgResizedU));

            // Compute target laplacian variance for edge matching
            cv::Mat jpgGray;
            cv::cvtColor(jpgResized, jpgGray, cv::COLOR_BGR2GRAY);
            cv::Mat laplacian;
            cv::Laplacian(jpgGray, laplacian, CV_64F);
            cv::Scalar mu, sigma;
            cv::meanStdDev(laplacian, mu, sigma);
            float targetLaplacianVar = sigma[0] * sigma[0];

            // Run optimizer (silent mode)
            geos::internal::optimizeGeos(
                body, link, targetStyleResized, targetLaplacianVar, config,
                nullptr,  // No progress callback
                false     // No LUT estimation
            );

            // Read optimized dials
            geos::internal::Theta theta;
            geos::internal::readDials(link, theta);
            std::copy(theta.begin(), theta.end(), sample.dials.begin());
        } else {
            // Neutral dials (placeholder)
            std::fill(sample.dials.begin(), sample.dials.end(), 0.5f);
        }

        samples[i] = sample;
        valid[i] = true;
        completed++;
    }

    // Compact valid samples
    std::vector<TrainingSample> validSamples;
    for (size_t i = 0; i < pairs.size(); i++) {
        if (valid[i]) {
            validSamples.push_back(samples[i]);
            processed++;
        } else {
            skipped++;
        }
    }
    samples = std::move(validSamples);

    std::cout << "\nProcessed: " << processed << ", Skipped: " << skipped << "\n";

    // Save as JSON
    std::ofstream out(outPath);
    out << "{\n  \"samples\": [\n";

    for (size_t i = 0; i < samples.size(); i++) {
        const auto& s = samples[i];
        out << "    {\n";
        out << "      \"image\": \"" << s.image << "\",\n";
        out << "      \"vibe_class\": \"" << s.vibe_class << "\",\n";
        out << "      \"features\": [";
        for (int j = 0; j < 23; j++) {
            if (j > 0) out << ", ";
            out << std::fixed << std::setprecision(6) << s.features[j];
        }
        out << "],\n";
        out << "      \"dials\": [";
        for (int j = 0; j < 45; j++) {
            if (j > 0) out << ", ";
            out << std::fixed << std::setprecision(4) << s.dials[j];
        }
        out << "]\n";
        out << "    }" << (i < samples.size() - 1 ? ",\n" : "\n");
    }

    out << "  ]\n}\n";
    out.close();

    std::cout << "Saved " << samples.size() << " samples to " << outPath << "\n";

    // Print vibe class distribution
    std::map<std::string, int> vibeCount;
    for (const auto& s : samples) vibeCount[s.vibe_class]++;

    std::cout << "\nVibe class distribution:\n";
    for (const auto& [k, v] : vibeCount) {
        std::cout << "  " << k << ": " << v << "\n";
    }

    return 0;
}

// ============================================================
// Train command: train MLP on extracted data
// ============================================================

// Simple JSON parser for training data
std::vector<TrainingSample> loadTrainingData(const std::string& path) {
    std::vector<TrainingSample> samples;

    std::ifstream in(path);
    if (!in) {
        std::cerr << "Cannot open " << path << "\n";
        return samples;
    }

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    // Very basic JSON parsing - find each sample block
    size_t pos = 0;
    while ((pos = content.find("\"image\":", pos)) != std::string::npos) {
        TrainingSample sample;

        // Parse image name
        size_t start = content.find("\"", pos + 8) + 1;
        size_t end = content.find("\"", start);
        sample.image = content.substr(start, end - start);

        // Parse vibe_class
        pos = content.find("\"vibe_class\":", end);
        start = content.find("\"", pos + 13) + 1;
        end = content.find("\"", start);
        sample.vibe_class = content.substr(start, end - start);

        // Parse features array
        pos = content.find("\"features\":", end);
        start = content.find("[", pos) + 1;
        end = content.find("]", start);
        std::string featStr = content.substr(start, end - start);
        std::istringstream featIss(featStr);
        for (int i = 0; i < 23; i++) {
            featIss >> sample.features[i];
            if (featIss.peek() == ',') featIss.ignore();
        }

        // Parse dials array
        pos = content.find("\"dials\":", end);
        start = content.find("[", pos) + 1;
        end = content.find("]", start);
        std::string dialStr = content.substr(start, end - start);
        std::istringstream dialIss(dialStr);
        for (int i = 0; i < 45; i++) {
            dialIss >> sample.dials[i];
            if (dialIss.peek() == ',') dialIss.ignore();
        }

        samples.push_back(sample);
        pos = end;
    }

    return samples;
}

int cmdTrain(const std::string& dataPath, const std::string& modelPath) {
    std::cout << "Training MLP from " << dataPath << "\n";

    auto samples = loadTrainingData(dataPath);
    if (samples.empty()) {
        std::cerr << "No training samples loaded\n";
        return 1;
    }

    std::cout << "Loaded " << samples.size() << " samples\n";

    // Prepare training matrices
    int n = samples.size();
    cv::Mat X(n, 23, CV_32F);
    cv::Mat Y(n, 45, CV_32F);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 23; j++)
            X.at<float>(i, j) = samples[i].features[j];
        for (int j = 0; j < 45; j++)
            Y.at<float>(i, j) = samples[i].dials[j];
    }

    // Normalize features (mean 0, std 1) per column
    std::vector<float> feat_mean(23, 0.0f), feat_std(23, 0.0f);
    for (int j = 0; j < 23; j++) {
        cv::Scalar m, s;
        cv::meanStdDev(X.col(j), m, s);
        feat_mean[j] = m[0];
        feat_std[j] = s[0];
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 23; j++) {
            if (feat_std[j] > 1e-6)
                X.at<float>(i, j) = (X.at<float>(i, j) - feat_mean[j]) / feat_std[j];
        }
    }

    // Split train/val (80/20)
    int trainSize = n * 8 / 10;
    cv::Mat X_train = X.rowRange(0, trainSize);
    cv::Mat Y_train = Y.rowRange(0, trainSize);
    cv::Mat X_val = X.rowRange(trainSize, n);
    cv::Mat Y_val = Y.rowRange(trainSize, n);

    std::cout << "Train: " << trainSize << ", Val: " << (n - trainSize) << "\n";

    // Create and train MLP
    vibe::MLP mlp;
    mlp.init({23, 128, 64, 45});

    std::cout << "Training...\n";
    mlp.train(X_train, Y_train, 1000, 0.01f, 32);

    // Validate
    cv::Mat pred = mlp.forward(X_val);
    cv::Mat diff = pred - Y_val;
    float valLoss = cv::norm(diff, cv::NORM_L2SQR) / (X_val.rows * 45);
    std::cout << "Validation loss: " << valLoss << "\n";

    // Save model
    mlp.save(modelPath);
    std::cout << "Saved model to " << modelPath << "\n";

    return 0;
}

// ============================================================
// Predict command: predict dials for new image
// ============================================================

int cmdPredict(const std::string& imagePath, const std::string& modelPath) {
    std::cout << "Predicting dials for " << imagePath << "\n";

    // Load model
    vibe::MLP mlp;
    if (!mlp.load(modelPath)) {
        std::cerr << "Cannot load model from " << modelPath << "\n";
        return 1;
    }

    // Load image and extract features
    cv::Mat img = cv::imread(imagePath);
    if (img.empty()) {
        std::cerr << "Cannot load image " << imagePath << "\n";
        return 1;
    }

    cv::UMat imgU;
    img.copyTo(imgU);
    auto sf = geos::internal::extractStyleFromBGR(
        geos::internal::resizeProxy(imgU));

    // Predict
    std::vector<float> features(sf.v.begin(), sf.v.end());
    auto dials = mlp.predict(features);

    // Print results
    std::cout << "\nPredicted dials:\n";
    const char* dialNames[] = {
        "exposure", "temperature", "tint",
        "contrast", "highlights", "shadows", "toe_pivot", "shoulder_pivot", "white_point", "black_point",
        "vibrance", "saturation", "density",
        "shadow_temp", "shadow_tint", "highlight_temp", "highlight_tint",
        "red_hue", "red_sat", "red_lum",
        "orange_hue", "orange_sat", "orange_lum",
        "yellow_hue", "yellow_sat", "yellow_lum",
        "green_hue", "green_sat", "green_lum",
        "cyan_hue", "cyan_sat", "cyan_lum",
        "blue_hue", "blue_sat", "blue_lum",
        "purple_hue", "purple_sat", "purple_lum",
        "magenta_hue", "magenta_sat", "magenta_lum",
        "sharpen_amount", "sharpen_radius", "denoise_luma", "denoise_chroma"
    };

    for (int i = 0; i < 45; i++) {
        std::cout << "  " << std::setw(18) << dialNames[i] << ": "
                  << std::fixed << std::setprecision(4) << dials[i] << "\n";
    }

    return 0;
}

// ============================================================
// Render command: predict dials and render ARW
// ============================================================

// Camera base dials (mean across 537 training samples)
static const float CAMERA_BASE[45] = {
    0.5743f, 0.5351f, 0.4650f, 0.4906f, 0.5288f, 0.4944f,  // VIEW
    0.4609f, 0.4626f, 0.4678f, 0.4750f, 0.4886f, 0.4969f,  // POPS global
    0.5241f, 0.5197f, 0.4830f, 0.5089f, 0.4701f, 0.5056f,  // R-C axis
    0.4830f, 0.4895f, 0.5200f, 0.5166f, 0.5139f, 0.4802f,  // G-M axis
    0.5173f, 0.5122f, 0.4767f, 0.4795f, 0.4669f, 0.5030f,  // B-Y axis
    0.4921f, 0.4926f, 0.4998f, 0.4830f, 0.5247f, 0.4729f,  // O-P axis
    0.4754f, 0.5410f, 0.5119f, 0.4888f, 0.5174f,           // extra HSL
    0.0302f, 0.3896f, 0.0359f, 0.0360f                      // special
};

int cmdRender(const std::string& arwPath, const std::string& modelPath, const std::string& outDir, float lush = 0.0f, bool useBase = false) {
    std::cout << "Rendering " << arwPath << " with predicted dials\n";

    // Load model
    vibe::MLP mlp;
    if (!mlp.load(modelPath)) {
        std::cerr << "Cannot load model from " << modelPath << "\n";
        return 1;
    }

    // Create pipeline and load RAW
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(arwPath));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
    if (!head) {
        std::cerr << "Cannot decode " << arwPath << "\n";
        return 1;
    }

    // Get camera preview and extract features
    cv::Mat preview;
    head->view().view().copyTo(preview);

    cv::UMat previewU;
    preview.copyTo(previewU);
    auto sf = geos::internal::extractStyleFromBGR(
        geos::internal::resizeProxy(previewU));

    // Predict dials
    std::vector<float> features(sf.v.begin(), sf.v.end());
    auto dials = mlp.predict(features);

    if (useBase) {
        // Use camera base + MLP deltas from neutral
        // MLP predicted what dials should be to match camera
        // Delta = predicted - neutral (0.5)
        // Final = base + delta = base + (predicted - 0.5)
        std::cout << "Using camera base + deltas mode\n";
        for (int i = 0; i < 45; i++) {
            float delta = dials[i] - 0.5f;  // how far from neutral
            dials[i] = std::clamp(CAMERA_BASE[i] + delta, 0.0f, 1.0f);
        }
    }

    // Apply lush boost to color dials
    if (lush != 0.0f) {
        dials[6] = std::clamp(dials[6] + lush, 0.0f, 1.0f);  // vibrance
        dials[7] = std::clamp(dials[7] + lush, 0.0f, 1.0f);  // saturation
        dials[8] = std::clamp(dials[8] + lush * 0.5f, 0.0f, 1.0f);  // colourDensity (half boost)
        std::cout << "Lush boost=" << lush << " → vibrance=" << dials[6]
                  << " saturation=" << dials[7] << " colourDensity=" << dials[8] << "\n";
    }

    std::cout << "Predicted exposure=" << dials[0] << " temperature=" << dials[1] << "\n";

    // Create body and apply dials
    const int workingSize = 1080;
    pipe::Body& body = head->body(workingSize);
    pipe::Body::Link& link = body.add("vibe");

    // Write predicted dials to link
    geos::internal::Theta theta;
    std::copy(dials.begin(), dials.end(), theta.begin());
    geos::internal::writeDials(link, theta);

    // Render
    cv::Mat output;
    body.view().copyTo(output);

    // Save outputs
    fs::create_directories(outDir);
    std::string basename = fs::path(arwPath).stem().string();

    cv::imwrite(outDir + "/" + basename + "_preview.png", preview);
    cv::imwrite(outDir + "/" + basename + "_vibe.png", output);

    // Also save the camera JPG reference if it exists
    std::string jpgPath = arwPath.substr(0, arwPath.size() - 4) + ".JPG";
    if (fs::exists(jpgPath)) {
        cv::Mat jpg = cv::imread(jpgPath);
        cv::Mat jpgResized;
        cv::resize(jpg, jpgResized, cv::Size(output.cols, output.rows), 0, 0, cv::INTER_AREA);
        cv::imwrite(outDir + "/" + basename + "_camera.png", jpgResized);

        // Compute diff
        cv::Mat diff;
        cv::absdiff(output, jpgResized, diff);
        diff *= 5;  // amplify
        cv::imwrite(outDir + "/" + basename + "_diff.png", diff);
    }

    std::cout << "Saved to " << outDir << "/" << basename << "_*.png\n";

    return 0;
}

// ============================================================
// Main
// ============================================================

void printUsage(const char* prog) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << prog << " extract <pics_dir> --out <train.json> [--no-optimize]\n";
    std::cerr << "  " << prog << " train <train.json> --out <model.vibe>\n";
    std::cerr << "  " << prog << " predict <image> --model <model.vibe>\n";
    std::cerr << "  " << prog << " render <image.ARW> --model <model.vibe> --out <dir>\n";
    std::cerr << "\nNeural dial predictor - learns dial settings from image features.\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  --no-optimize    Skip optimizer (use neutral 0.5 dials) for quick testing\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "extract" && argc >= 5) {
        std::string picsDir = argv[2];
        std::string outPath;
        bool optimize = true;
        for (int i = 3; i < argc; i++) {
            if (std::string(argv[i]) == "--out" && i + 1 < argc)
                outPath = argv[++i];
            else if (std::string(argv[i]) == "--no-optimize")
                optimize = false;
        }
        if (outPath.empty()) { printUsage(argv[0]); return 1; }
        return cmdExtract(picsDir, outPath, optimize);
    }
    else if (cmd == "train" && argc >= 5) {
        std::string dataPath = argv[2];
        std::string modelPath;
        for (int i = 3; i < argc; i++) {
            if (std::string(argv[i]) == "--out" && i + 1 < argc)
                modelPath = argv[++i];
        }
        if (modelPath.empty()) { printUsage(argv[0]); return 1; }
        return cmdTrain(dataPath, modelPath);
    }
    else if (cmd == "predict" && argc >= 5) {
        std::string imagePath = argv[2];
        std::string modelPath;
        for (int i = 3; i < argc; i++) {
            if (std::string(argv[i]) == "--model" && i + 1 < argc)
                modelPath = argv[++i];
        }
        if (modelPath.empty()) { printUsage(argv[0]); return 1; }
        return cmdPredict(imagePath, modelPath);
    }
    else if (cmd == "render" && argc >= 7) {
        std::string arwPath = argv[2];
        std::string modelPath, outDir;
        float lush = 0.0f;
        bool useBase = false;
        for (int i = 3; i < argc; i++) {
            if (std::string(argv[i]) == "--model" && i + 1 < argc)
                modelPath = argv[++i];
            else if (std::string(argv[i]) == "--out" && i + 1 < argc)
                outDir = argv[++i];
            else if (std::string(argv[i]) == "--lush" && i + 1 < argc)
                lush = std::stof(argv[++i]);
            else if (std::string(argv[i]) == "--base")
                useBase = true;
        }
        if (modelPath.empty() || outDir.empty()) { printUsage(argv[0]); return 1; }
        return cmdRender(arwPath, modelPath, outDir, lush, useBase);
    }
    else {
        printUsage(argv[0]);
        return 1;
    }
}
