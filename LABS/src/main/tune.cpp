// tune.cpp
// Headless tool: Automatically finds optimal pipe dial values to match a target image
//
// WORKFLOW:
//   tune <source.ARW> <target.png> --output link.json
//   → Loads RAW through HEAD
//   → Uses target.png as reference (or embedded preview if target is "preview")
//   → Runs GEOS + EDGE optimization
//   → Saves optimized Link settings to link.json
//
// Usage:
//   tune <source.ARW> <target.png> [options]
//   tune <source.ARW> preview [options]      # Use embedded camera preview as target
//
// Options:
//   --output <link.json>    Save optimized dial values (required)
//   --threshold <value>     Stop when spectral loss below this (default: 0.005 = 0.5%)
//   --size <pixels>         Working size for optimization (default: 1080)
//   --mode <mode>           Optimization mode: blockwise, full35d, linear (default: blockwise)
//   --skip-lut              Skip 3D LUT estimation (pure dial optimization)
//
// Examples:
//   tune photo.ARW preview --output style.json              # Match camera JPEG
//   tune photo.ARW reference.png --output style.json        # Match external reference
//   tune photo.ARW preview --output style.json --mode linear  # Linear ops only

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <geos.hpp>
#include <data.hpp>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

void printUsage(const char* prog)
{
    std::cerr << "Usage: " << prog << " <source.ARW> <target.png|preview> --output <link.json> [options]\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --output <link.json>    Save optimized dial values (required)\n";
    std::cerr << "  --threshold <value>     Stop when spectral loss below (default: 0.005)\n";
    std::cerr << "  --size <pixels>         Working size (default: 1080)\n";
    std::cerr << "  --mode <mode>           blockwise, full35d, linear (default: blockwise)\n";
    std::cerr << "  --skip-lut              Skip 3D LUT estimation\n";
}

int main(int argc, char** argv)
{
    // Handle --help before minimum argument check
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { printUsage(argv[0]); return 0; }
    }

    if (argc < 3)
    {
        printUsage(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string sourcePath = argv[1];
    std::string targetPath = argv[2];
    std::string outputPath;
    float threshold = 0.005f;
    int workingSize = 1080;
    geos::Mode mode = geos::Mode::BLOCKWISE;
    bool skipLut = false;

    for (int i = 3; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "--threshold" && i + 1 < argc) threshold = std::stof(argv[++i]);
        else if (arg == "--size" && i + 1 < argc) workingSize = std::stoi(argv[++i]);
        else if (arg == "--skip-lut") skipLut = true;
        else if (arg == "--mode" && i + 1 < argc)
        {
            std::string m = argv[++i];
            if (m == "full35d" || m == "full") mode = geos::Mode::FULL_35D;
            else if (m == "linear" || m == "lin") mode = geos::Mode::LINEAR_ONLY;
            else mode = geos::Mode::BLOCKWISE;
        }
        else if (arg == "--help" || arg == "-h") { /* handled above */ }
        else { std::cerr << "Unknown option: " << arg << "\n"; printUsage(argv[0]); return 1; }
    }

    if (outputPath.empty())
    {
        std::cerr << "Error: --output is required\n";
        printUsage(argv[0]);
        return 1;
    }

    try
    {
        const char* modeName = (mode == geos::Mode::FULL_35D) ? "FULL_35D" :
                               (mode == geos::Mode::LINEAR_ONLY) ? "LINEAR_ONLY" : "BLOCKWISE";

        std::cout << "=== TUNE ===" << std::endl;
        std::cout << "Source: " << sourcePath << std::endl;
        std::cout << "Target: " << targetPath << std::endl;
        std::cout << "Output: " << outputPath << std::endl;
        std::cout << "Mode: " << modeName << std::endl;
        std::cout << "Working size: " << workingSize << "px" << std::endl;

        // Create pipe and load RAW
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
        pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(sourcePath));

        std::cout << "\n[HEAD] Decoding..." << std::endl;
        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
        if (!head)
        {
            throw std::runtime_error("Failed to decode: " + sourcePath);
        }

        pipe::Info info = head->data().info();
        std::cout << "  Size: " << info["width"] << "x" << info["height"] << std::endl;
        std::cout << "  Camera: " << info["camera_model"] << std::endl;

        // Get target image
        cv::Mat targetMat;
        bool usePreview = (targetPath == "preview" || targetPath == "Preview" || targetPath == "PREVIEW");

        if (usePreview)
        {
            std::cout << "\n[TARGET] Using embedded camera preview..." << std::endl;
            pipe::View previewView = head->view().view();
            previewView.copyTo(targetMat);
        }
        else
        {
            std::cout << "\n[TARGET] Loading external reference..." << std::endl;
            targetMat = cv::imread(targetPath);
            if (targetMat.empty())
            {
                throw std::runtime_error("Failed to load target: " + targetPath);
            }
        }
        std::cout << "  Target size: " << targetMat.cols << "x" << targetMat.rows << std::endl;

        // Resize target to working size
        cv::Mat targetResized;
        int maxDim = std::max(targetMat.cols, targetMat.rows);
        float scale = static_cast<float>(workingSize) / maxDim;
        cv::resize(targetMat, targetResized, cv::Size(), scale, scale, cv::INTER_AREA);
        std::cout << "  Resized to: " << targetResized.cols << "x" << targetResized.rows << std::endl;

        // Create body with working size
        std::cout << "\n[BODY] Creating pipeline..." << std::endl;
        pipe::Body& body = head->body(workingSize);

        // Get body dimensions for exact target match
        pipe::View bodyView = body.view();
        cv::Mat bodyMat;
        bodyView.copyTo(bodyMat);

        // Resize target to exactly match body
        cv::Mat targetForTune;
        cv::resize(targetResized, targetForTune, cv::Size(bodyMat.cols, bodyMat.rows), 0, 0, cv::INTER_AREA);

        // Create geos task with target
        cv::UMat targetUMat;
        targetForTune.copyTo(targetUMat);
        pqtr::Hold<geos::Task> geosTask = geos::make(targetUMat);

        // Compute baseline loss
        cv::UMat bodyUMat;
        bodyMat.copyTo(bodyUMat);
        geos::Data baseline = geosTask->diff(bodyUMat);
        std::cout << "  Baseline spectral: " << std::fixed << std::setprecision(4)
                  << baseline.spectral << " (" << std::setprecision(2)
                  << (baseline.spectral * 100) << "%)" << std::endl;

        // Create link for optimization
        pipe::Body::Link& link = body.add("tune");

        // Initialize all dials to neutral (0.5)
        link.colorCorrection().exposure().set(0.5f);
        link.colorCorrection().whiteBalance().temperature(0.5f);
        link.colorCorrection().whiteBalance().tint(0.5f);
        link.toneMapping().contrast().set(0.5f);
        link.toneMapping().curveAdjustment().highlights().set(0.5f);
        link.toneMapping().curveAdjustment().shadows().set(0.5f);
        link.toneMapping().curveAdjustment().toePivot().set(0.5f);
        link.toneMapping().curveAdjustment().shoulderPivot().set(0.5f);
        link.toneMapping().clippingPoint().white().set(0.5f);
        link.toneMapping().clippingPoint().black().set(0.5f);
        link.globalColor().vibrance().set(0.5f);
        link.globalColor().saturation().set(0.5f);
        link.globalColor().colourDensity().set(0.5f);

        // Run optimization
        std::cout << "\n[GEOS] Running optimization..." << std::endl;

        geos::Config config;
        config.skip_edge = false;
        config.skip_lut = skipLut || (mode == geos::Mode::LINEAR_ONLY);
        config.geos_max_iter = 500;
        config.geos_multi_starts = 5;
        config.geos_threshold = threshold;
        config.geos_mode = mode;

        const char* phaseNames[] = {"HUGE", "MIDS", "TINY"};
        geos::Result result = geosTask->run(body, link, config,
            [&phaseNames](const geos::Progress& p) {
                if (p.stage == geos::Progress::Stage::GEOS)
                {
                    std::cout << "\r  [" << phaseNames[static_cast<int>(p.phase)] << "] "
                              << std::setw(3) << p.iteration << "/" << p.max_iterations
                              << "  loss=" << std::fixed << std::setprecision(4) << p.loss.spectral
                              << "     " << std::flush;
                }
                else if (p.stage == geos::Progress::Stage::EDGE)
                {
                    std::cout << "\r  [EDGE] "
                              << std::setw(3) << p.iteration << "/" << p.max_iterations
                              << "  freq=" << std::fixed << std::setprecision(4) << p.loss.frequency
                              << "     " << std::flush;
                }
                return true;
            });

        std::cout << std::endl;
        std::cout << "  Iterations: " << result.geos_iterations << std::endl;
        std::cout << "  Final spectral: " << std::fixed << std::setprecision(4)
                  << result.loss.spectral << " (" << std::setprecision(2)
                  << (result.loss.spectral * 100) << "%)" << std::endl;

        // Save link settings
        std::cout << "\n[SAVE] Writing link settings..." << std::endl;
        if (!data::link::save(link, outputPath))
        {
            throw std::runtime_error("Failed to save: " + outputPath);
        }
        std::cout << "  Saved: " << outputPath << std::endl;

        std::cout << "\n[OK] Done" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
