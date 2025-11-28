// tune.cpp
// Optimizer tool: Finds optimal Link settings to match camera preview
//
// WORKFLOW:
//   tune <source.ARW> --output edit.json
//   → Loads RAW, uses embedded preview as target
//   → Runs SPSA optimizer to find dial values
//   → Saves optimized Link settings to edit.json
//
// Usage:
//   tune <source.ARW> [options]
//
// Options:
//   --output <edit.json>    Save optimized Link (default: stdout)
//   --target <image.png>    Use external target instead of embedded preview
//   --mode <mode>           Optimization mode: blockwise|full|linear (default: blockwise)
//   --iterations <n>        Max iterations per phase (default: 500)
//
// Output: edit.json containing optimized Link dial values

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <tune.hpp>
#include <data.hpp>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

constexpr int WORKING_SIZE = 512;  // Optimization proxy size

void printUsage(const char* prog)
{
    std::cerr << "Usage: " << prog << " <source.ARW> [options]\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --output <edit.json>    Save optimized Link (default: stdout)\n";
    std::cerr << "  --target <image.png>    Use external target instead of embedded preview\n";
    std::cerr << "  --mode <mode>           blockwise|full|linear (default: blockwise)\n";
    std::cerr << "  --iterations <n>        Max iterations per phase (default: 500)\n";
}

tune::GeosMode parseMode(const std::string& mode)
{
    if (mode == "full" || mode == "full35d") return tune::GeosMode::FULL_35D;
    if (mode == "linear" || mode == "lin") return tune::GeosMode::LINEAR_ONLY;
    return tune::GeosMode::BLOCKWISE;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printUsage(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string sourcePath = argv[1];
    std::string outputPath;
    std::string targetPath;
    std::string modeStr = "blockwise";
    int maxIter = 500;

    for (int i = 2; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "--target" && i + 1 < argc) targetPath = argv[++i];
        else if (arg == "--mode" && i + 1 < argc) modeStr = argv[++i];
        else if (arg == "--iterations" && i + 1 < argc) maxIter = std::stoi(argv[++i]);
        else if (arg == "--help" || arg == "-h") { printUsage(argv[0]); return 0; }
        else { std::cerr << "Unknown option: " << arg << "\n"; printUsage(argv[0]); return 1; }
    }

    try
    {
        std::cout << "=== TUNE ===" << std::endl;
        std::cout << "Source: " << sourcePath << std::endl;

        // Create pipe and load RAW
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
        pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(sourcePath));

        std::cout << "Decoding RAW..." << std::endl;
        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
        if (!head)
        {
            throw std::runtime_error("Failed to decode: " + sourcePath);
        }

        pipe::Info info = head->data().info();
        std::cout << "  Size: " << info["width"] << "x" << info["height"] << std::endl;
        std::cout << "  Camera: " << info["camera_model"] << std::endl;

        // Get target image (embedded preview or external)
        cv::UMat targetImg;
        if (!targetPath.empty())
        {
            std::cout << "Loading external target: " << targetPath << std::endl;
            cv::Mat ext = cv::imread(targetPath);
            if (ext.empty()) throw std::runtime_error("Failed to load target: " + targetPath);
            ext.copyTo(targetImg);
        }
        else
        {
            std::cout << "Using embedded camera preview as target" << std::endl;
            head->view().view().copyTo(targetImg);
        }

        // Resize target to working size
        int maxDim = std::max(targetImg.cols, targetImg.rows);
        float scale = static_cast<float>(WORKING_SIZE) / maxDim;
        cv::UMat targetResized;
        cv::resize(targetImg, targetResized, cv::Size(), scale, scale, cv::INTER_AREA);
        std::cout << "  Target: " << targetResized.cols << "x" << targetResized.rows << std::endl;

        // Create body with working size
        pipe::Body& body = head->body(WORKING_SIZE);

        // Add optimization link
        pipe::Body::Link& link = body.add("tune");

        // Create tune task
        pqtr::Hold<tune::Task> tuneTask = tune::make(targetResized);

        // Configure optimizer
        tune::Config config;
        config.geos_mode = parseMode(modeStr);
        config.geos_max_iter = maxIter;
        config.geos_multi_starts = 5;
        config.skip_edge = false;
        config.skip_lut = (config.geos_mode == tune::GeosMode::LINEAR_ONLY);

        std::cout << "\nOptimizing (mode: " << modeStr << ", max_iter: " << maxIter << ")..." << std::endl;

        // Run optimizer with progress callback
        const char* phaseNames[] = {"HUGE", "MIDS", "TINY"};
        tune::Result result = tuneTask->run(body, link, config,
            [&phaseNames](const tune::Progress& p) {
                if (p.stage == tune::Progress::Stage::GEOS)
                {
                    std::cout << "\r  [" << phaseNames[static_cast<int>(p.phase)] << "] "
                              << std::setw(3) << p.iteration << "/" << p.max_iterations
                              << "  loss=" << std::fixed << std::setprecision(4) << p.loss.spectral
                              << "     " << std::flush;
                }
                else if (p.stage == tune::Progress::Stage::EDGE)
                {
                    std::cout << "\r  [EDGE] "
                              << std::setw(3) << p.iteration << "/" << p.max_iterations
                              << "  freq=" << std::fixed << std::setprecision(4) << p.loss.frequency
                              << "     " << std::flush;
                }
                return true;
            });

        std::cout << std::endl;
        std::cout << "\nResult:" << std::endl;
        std::cout << "  Spectral loss: " << std::fixed << std::setprecision(4)
                  << result.loss.spectral << " (" << std::setprecision(2)
                  << (result.loss.spectral * 100) << "%)" << std::endl;
        std::cout << "  Frequency loss: " << std::fixed << std::setprecision(4)
                  << result.loss.frequency << std::endl;
        std::cout << "  Iterations: " << result.geos_iterations << std::endl;

        // Output Link settings
        if (!outputPath.empty())
        {
            std::cout << "\nSaving: " << outputPath << std::endl;
            if (!data::link::save(link, outputPath))
            {
                throw std::runtime_error("Failed to save: " + outputPath);
            }
            std::cout << "  Done!" << std::endl;
        }
        else
        {
            std::cout << "\n--- edit.json ---" << std::endl;
            std::cout << data::link::toJson(link);
        }

        std::cout << "\n[OK] Tune complete" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
