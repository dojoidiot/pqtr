// tune.cpp
// Headless tool: Automatically finds optimal pipe dial values to match a target image
//
// WORKFLOW:
//   tune <source.ARW> <target.png> --save-area <dir>
//   → Loads RAW through HEAD
//   → Uses target.png as reference (or embedded preview if target is "preview")
//   → Runs GEOS + EDGE optimization
//   → Saves optimized Link settings to <dir>/tune.json
//
// Usage:
//   tune <source.ARW> <target.png> [options]
//   tune <source.ARW> preview [options]      # Use embedded camera preview as target
//
// Options:
//   --save-area <dir>       Output directory for tune.json (required)
//   --threshold <value>     Stop when spectral loss below this (default: 0.005 = 0.5%)
//   --size <pixels>         Working size for optimization (default: 1080)
//   --mode <mode>           Optimization mode: blockwise, full35d, linear (default: blockwise)
//   --skip-lut              Skip 3D LUT estimation (pure dial optimization)
//   --logs                  Verbose progress output (dome.r, edge.ratio)
//   --fine                  Save intermediate images (head, body, optimized, diff)
//   --fine-area <dir>       Directory for --fine outputs (default: --save-area)
//
// Examples:
//   tune photo.ARW preview --save-area ./out                # Match camera JPEG
//   tune photo.ARW reference.png --save-area ./out          # Match external reference
//   tune photo.ARW preview --save-area ./out --mode linear  # Linear ops only
//   tune photo.ARW preview --save-area ./out --logs --fine  # Verbose + intermediates

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
    std::cerr << "Usage: " << prog << " <source.ARW> <target.png|preview> --save-area <dir> [options]\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --save-area <dir>       Output directory for tune.json (required)\n";
    std::cerr << "  --threshold <value>     Stop when spectral loss below (default: 0.005)\n";
    std::cerr << "  --size <pixels>         Working size (default: 1080)\n";
    std::cerr << "  --mode <mode>           blockwise, full35d, linear (default: blockwise)\n";
    std::cerr << "  --skip-lut              Skip 3D LUT estimation\n";
    std::cerr << "  --logs                  Verbose progress (dome.r, edge.ratio)\n";
    std::cerr << "  --fine                  Save intermediate images (head, body, optimized, diff)\n";
    std::cerr << "  --fine-area <dir>       Directory for --fine outputs (default: --save-area)\n";
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
    std::string saveArea;
    std::string fineArea;
    float threshold = 0.005f;
    int workingSize = 1080;
    geos::Mode mode = geos::Mode::BLOCKWISE;
    bool skipLut = false;
    bool logs = false;
    bool fine = false;

    for (int i = 3; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--save-area" && i + 1 < argc) saveArea = argv[++i];
        else if (arg == "--fine-area" && i + 1 < argc) fineArea = argv[++i];
        else if (arg == "--threshold" && i + 1 < argc) threshold = std::stof(argv[++i]);
        else if (arg == "--size" && i + 1 < argc) workingSize = std::stoi(argv[++i]);
        else if (arg == "--skip-lut") skipLut = true;
        else if (arg == "--logs") logs = true;
        else if (arg == "--fine") fine = true;
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

    if (saveArea.empty())
    {
        std::cerr << "Error: --save-area is required\n";
        printUsage(argv[0]);
        return 1;
    }

    // Default fine-area to save-area
    if (fineArea.empty()) fineArea = saveArea;

    try
    {
        const char* modeName = (mode == geos::Mode::FULL_35D) ? "FULL_35D" :
                               (mode == geos::Mode::LINEAR_ONLY) ? "LINEAR_ONLY" : "BLOCKWISE";

        std::cout << "=== TUNE ===" << std::endl;
        std::cout << "Source: " << sourcePath << std::endl;
        std::cout << "Target: " << targetPath << std::endl;
        std::cout << "Save area: " << saveArea << std::endl;
        std::cout << "Mode: " << modeName << std::endl;
        std::cout << "Working size: " << workingSize << "px" << std::endl;
        if (logs) std::cout << "Logs: enabled" << std::endl;
        if (fine) std::cout << "Fine area: " << fineArea << std::endl;

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
            [&phaseNames, logs](const geos::Progress& p) {
                if (p.stage == geos::Progress::Stage::GEOS)
                {
                    std::cout << "\r  [" << phaseNames[static_cast<int>(p.phase)] << "] "
                              << std::setw(3) << p.iteration << "/" << p.max_iterations
                              << "  loss=" << std::fixed << std::setprecision(4) << p.loss.spectral;
                    if (logs) std::cout << "  r=" << std::setprecision(3) << p.dome.r;
                    std::cout << "     " << std::flush;
                }
                else if (p.stage == geos::Progress::Stage::EDGE)
                {
                    std::cout << "\r  [EDGE] "
                              << std::setw(3) << p.iteration << "/" << p.max_iterations
                              << "  freq=" << std::fixed << std::setprecision(4) << p.loss.frequency;
                    if (logs) std::cout << "  ratio=" << std::setprecision(3) << p.edge.ratio;
                    std::cout << "     " << std::flush;
                }
                return true;
            });

        std::cout << std::endl;
        std::cout << "  Iterations: " << result.geos_iterations << std::endl;
        std::cout << "  Final spectral: " << std::fixed << std::setprecision(4)
                  << result.loss.spectral << " (" << std::setprecision(2)
                  << (result.loss.spectral * 100) << "%)" << std::endl;

        // Save tune settings
        std::cout << "\n[SAVE] Writing tune settings..." << std::endl;
        std::string tunePath = saveArea + "/tune.json";
        if (!data::link::save(link, tunePath))
        {
            throw std::runtime_error("Failed to save: " + tunePath);
        }
        std::cout << "  Saved: " << tunePath << std::endl;

        // Save fine outputs if requested
        if (fine)
        {
            std::cout << "\n[FINE] Saving intermediate images..." << std::endl;

            // tune.jpg - original target (camera JPEG or reference)
            if (!usePreview)
            {
                // Copy the original target file as tune.jpg
                cv::imwrite(fineArea + "/tune.jpg", targetMat);
                std::cout << "  Saved: " << fineArea << "/tune.jpg (original target)" << std::endl;
            }
            else
            {
                // Save the preview as tune.jpg
                cv::imwrite(fineArea + "/tune.jpg", targetMat);
                std::cout << "  Saved: " << fineArea << "/tune.jpg (camera preview)" << std::endl;
            }

            // head.png - target resized to working size
            cv::imwrite(fineArea + "/head.png", targetForTune);
            std::cout << "  Saved: " << fineArea << "/head.png (target at working size)" << std::endl;

            // body.png - baseline before optimization
            cv::imwrite(fineArea + "/body.png", bodyMat);
            std::cout << "  Saved: " << fineArea << "/body.png (baseline)" << std::endl;

            // tail.png - result after optimization
            cv::UMat optView = body.view();
            cv::Mat optMat;
            optView.copyTo(optMat);
            cv::imwrite(fineArea + "/tail.png", optMat);
            std::cout << "  Saved: " << fineArea << "/tail.png (optimized result)" << std::endl;

            // diff.png - visual difference (target vs tail, amplified 5x)
            cv::Mat diffMat;
            cv::absdiff(optMat, targetForTune, diffMat);
            diffMat.convertTo(diffMat, -1, 5.0);  // Amplify 5x for visibility
            cv::imwrite(fineArea + "/diff.png", diffMat);
            std::cout << "  Saved: " << fineArea << "/diff.png (difference x5)" << std::endl;
        }

        std::cout << "\n[OK] Done" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
