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
//   --optimizer <algo>      Optimizer: spsa, aceo, hybrid (default: spsa)
//   --full                  Full ACEO mode: single-pass 45-dial optimization (no LUT, no two-link)
//   --skip-lut              Skip 3D LUT estimation (pure dial optimization)
//   --logs                  Verbose progress output (dome.r, edge.ratio)
//   --fine                  Save intermediate images + meta.json
//   --fine-area <dir>       Directory for --fine outputs (default: --save-area)
//
// ACEO Covariance Options:
//   --with-cov <path>       Load prior covariance to blend with
//   --save-cov <path>       Save accumulated covariance after run
//
// Examples:
//   tune photo.ARW preview --save-area ./out                # Match camera JPEG
//   tune photo.ARW reference.png --save-area ./out          # Match external reference
//   tune photo.ARW preview --save-area ./out --mode linear  # Linear ops only
//   tune photo.ARW preview --save-area ./out --logs --fine  # Verbose + intermediates
//
// ACEO Covariance Chaining:
//   tune img1.ARW preview --save-area ./out --optimizer aceo --save-cov tmp/cov1.json
//   tune img2.ARW preview --save-area ./out --optimizer aceo --with-cov tmp/cov1.json --save-cov tmp/cov2.json
//   tune img3.ARW preview --save-area ./out --optimizer aceo --with-cov tmp/cov2.json --save-cov etc/aceo_full.json

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
    std::cerr << "  --optimizer <algo>      spsa, aceo (default: spsa)\n";
    std::cerr << "  --full                  Full 45-dial mode: single-pass, no LUT, no two-link\n";
    std::cerr << "  --skip-lut              Skip 3D LUT estimation\n";
    std::cerr << "  --regional              Enable regional refinement (slower, off by default)\n";
    std::cerr << "  --logs                  Verbose progress (dome.r, edge.ratio)\n";
    std::cerr << "  --fine                  Save intermediate images + meta.json\n";
    std::cerr << "  --fine-area <dir>       Directory for --fine outputs (default: --save-area)\n";
    std::cerr << "\nACEO covariance options:\n";
    std::cerr << "  --with-cov <path>       Load prior covariance to blend with\n";
    std::cerr << "  --save-cov <path>       Save accumulated covariance after run\n";
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
    std::string withCov;   // ACEO: prior covariance path
    std::string saveCov;   // ACEO: save covariance path
    float threshold = 0.005f;
    int workingSize = 1080;
    geos::Mode mode = geos::Mode::BLOCKWISE;
    geos::Optimizer optimizer = geos::Optimizer::SPSA;
    bool skipLut = false;
    bool skipRegional = true;  // Off by default (faster)
    bool fullMode = false;     // Full 45-dial single-pass mode
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
        else if (arg == "--regional") skipRegional = false;  // Enable regional refinement
        else if (arg == "--full") fullMode = true;  // Full 45-dial single-pass mode
        else if (arg == "--logs") logs = true;
        else if (arg == "--fine") fine = true;
        else if (arg == "--mode" && i + 1 < argc)
        {
            std::string m = argv[++i];
            if (m == "full35d" || m == "full") mode = geos::Mode::FULL_35D;
            else if (m == "linear" || m == "lin") mode = geos::Mode::LINEAR_ONLY;
            else mode = geos::Mode::BLOCKWISE;
        }
        else if (arg == "--optimizer" && i + 1 < argc)
        {
            std::string o = argv[++i];
            if (o == "aceo" || o == "ACEO") optimizer = geos::Optimizer::ACEO;
            else if (o == "hybrid" || o == "HYBRID") optimizer = geos::Optimizer::HYBRID;
            else optimizer = geos::Optimizer::SPSA;
        }
        else if (arg == "--with-cov" && i + 1 < argc) withCov = argv[++i];
        else if (arg == "--save-cov" && i + 1 < argc) saveCov = argv[++i];
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
        const char* modeName = fullMode ? "FULL_45D" :
                               (mode == geos::Mode::FULL_35D) ? "FULL_35D" :
                               (mode == geos::Mode::LINEAR_ONLY) ? "LINEAR_ONLY" : "BLOCKWISE";
        const char* optimizerName = (optimizer == geos::Optimizer::ACEO) ? "ACEO" : "SPSA";

        std::cout << "=== TUNE ===" << std::endl;
        std::cout << "Source: " << sourcePath << std::endl;
        std::cout << "Target: " << targetPath << std::endl;
        std::cout << "Save area: " << saveArea << std::endl;
        std::cout << "Mode: " << modeName << (fullMode ? " (single-pass, no LUT)" : "") << std::endl;
        std::cout << "Optimizer: " << optimizerName << std::endl;
        std::cout << "Working size: " << workingSize << "px" << std::endl;
        if (!withCov.empty()) std::cout << "With covariance: " << withCov << std::endl;
        if (!saveCov.empty()) std::cout << "Save covariance: " << saveCov << std::endl;
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

        // Progress callback (shared by all modes)
        const char* phaseNames[] = {"HUGE", "MIDS", "TINY"};
        auto progressCallback = [&phaseNames, logs](const geos::Progress& p) {
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
        };

        // Storage for links (need stable addresses for save)
        pipe::Body::Link* linkPtr = nullptr;
        std::vector<pipe::Body::Link*> links;

        if (fullMode)
        {
            // ============================================================
            // FULL MODE: Single-pass holistic 45-dial optimization
            // No LUT, no two-link split - pure dial matching
            // ============================================================
            std::cout << "\n[FULL] Creating single holistic link (45 dials)..." << std::endl;
            pipe::Body::Link& fullLink = body.add("full");

            // Initialize all dials to neutral (0.5)
            fullLink.colorCorrection().exposure().set(0.5f);
            fullLink.colorCorrection().whiteBalance().temperature(0.5f);
            fullLink.colorCorrection().whiteBalance().tint(0.5f);
            fullLink.toneMapping().contrast().set(0.5f);
            fullLink.toneMapping().curveAdjustment().highlights().set(0.5f);
            fullLink.toneMapping().curveAdjustment().shadows().set(0.5f);
            fullLink.toneMapping().curveAdjustment().toePivot().set(0.5f);
            fullLink.toneMapping().curveAdjustment().shoulderPivot().set(0.5f);
            fullLink.toneMapping().clippingPoint().white().set(0.5f);
            fullLink.toneMapping().clippingPoint().black().set(0.5f);
            fullLink.globalColor().vibrance().set(0.5f);
            fullLink.globalColor().saturation().set(0.5f);
            fullLink.globalColor().colourDensity().set(0.5f);

            std::cout << "[FULL] Optimizing all 45 dials holistically..." << std::endl;

            geos::Config fullConfig;
            fullConfig.skip_edge = false;
            fullConfig.skip_lut = true;   // No LUT in full mode - pure dials
            fullConfig.skip_regional = skipRegional;
            fullConfig.geos_max_iter = 500;  // More iterations for 45 dials
            fullConfig.geos_threshold = threshold;
            fullConfig.geos_mode = geos::Mode::FULL_35D;  // Uses all dials
            fullConfig.optimizer = optimizer;
            fullConfig.aceo_with_cov = withCov;
            fullConfig.aceo_save_cov = saveCov;

            geos::Result fullResult = geosTask->run(body, fullLink, fullConfig, progressCallback);
            std::cout << std::endl;
            std::cout << "  Full iterations: " << fullResult.geos_iterations << std::endl;
            std::cout << "  Final spectral: " << std::fixed << std::setprecision(4)
                      << fullResult.loss.spectral << " (" << std::setprecision(2)
                      << (fullResult.loss.spectral * 100) << "%)" << std::endl;

            linkPtr = &fullLink;
            links = {&fullLink};
        }
        else
        {
            // ============================================================
            // TWO-LINK ARCHITECTURE: linear (scene-referred) + display (display-referred)
            // ============================================================

            // ------------------------------------------------------------
            // LINK 1: Linear (scene-referred)
            // Dials: exposure, temperature, tint, black_point, white_point
            // ------------------------------------------------------------
            std::cout << "\n[LINEAR] Creating scene-referred link..." << std::endl;
            pipe::Body::Link& linearLink = body.add("linear");

            // Initialize all dials to neutral (0.5)
            linearLink.colorCorrection().exposure().set(0.5f);
            linearLink.colorCorrection().whiteBalance().temperature(0.5f);
            linearLink.colorCorrection().whiteBalance().tint(0.5f);
            linearLink.toneMapping().contrast().set(0.5f);
            linearLink.toneMapping().curveAdjustment().highlights().set(0.5f);
            linearLink.toneMapping().curveAdjustment().shadows().set(0.5f);
            linearLink.toneMapping().curveAdjustment().toePivot().set(0.5f);
            linearLink.toneMapping().curveAdjustment().shoulderPivot().set(0.5f);
            linearLink.toneMapping().clippingPoint().white().set(0.5f);
            linearLink.toneMapping().clippingPoint().black().set(0.5f);
            linearLink.globalColor().vibrance().set(0.5f);
            linearLink.globalColor().saturation().set(0.5f);
            linearLink.globalColor().colourDensity().set(0.5f);

            std::cout << "[LINEAR] Optimizing scene-referred dials (exposure, WB, clipping)..." << std::endl;

            geos::Config linearConfig;
            linearConfig.skip_edge = true;  // No sharpness for linear
            linearConfig.skip_lut = true;   // No LUT for linear
            linearConfig.skip_regional = true;  // Never regional for 5-dial linear
            linearConfig.geos_max_iter = 150;  // Fewer iterations for 5 dials
            linearConfig.geos_threshold = threshold;
            linearConfig.geos_mode = geos::Mode::SCENE_LINEAR;
            linearConfig.optimizer = optimizer;
            linearConfig.aceo_with_cov = withCov;
            linearConfig.aceo_save_cov = saveCov;

            geos::Result linearResult = geosTask->run(body, linearLink, linearConfig, progressCallback);
            std::cout << std::endl;
            std::cout << "  Linear iterations: " << linearResult.geos_iterations << std::endl;
            std::cout << "  Linear loss: " << std::fixed << std::setprecision(4)
                      << linearResult.loss.spectral << " (" << std::setprecision(2)
                      << (linearResult.loss.spectral * 100) << "%)" << std::endl;

            // ------------------------------------------------------------
            // LINK 2: Display (display-referred)
            // Dials: contrast, curves, saturation, split tone, selective color + LUT
            // ------------------------------------------------------------
            std::cout << "\n[DISPLAY] Creating display-referred link..." << std::endl;
            pipe::Body::Link& displayLink = body.add("display");

            // Initialize all dials to neutral (0.5) - linear dials stay neutral
            displayLink.colorCorrection().exposure().set(0.5f);
            displayLink.colorCorrection().whiteBalance().temperature(0.5f);
            displayLink.colorCorrection().whiteBalance().tint(0.5f);
            displayLink.toneMapping().contrast().set(0.5f);
            displayLink.toneMapping().curveAdjustment().highlights().set(0.5f);
            displayLink.toneMapping().curveAdjustment().shadows().set(0.5f);
            displayLink.toneMapping().curveAdjustment().toePivot().set(0.5f);
            displayLink.toneMapping().curveAdjustment().shoulderPivot().set(0.5f);
            displayLink.toneMapping().clippingPoint().white().set(0.5f);
            displayLink.toneMapping().clippingPoint().black().set(0.5f);
            displayLink.globalColor().vibrance().set(0.5f);
            displayLink.globalColor().saturation().set(0.5f);
            displayLink.globalColor().colourDensity().set(0.5f);

            std::cout << "[DISPLAY] Optimizing display-referred dials (contrast, color, style)..." << std::endl;

            geos::Config displayConfig;
            displayConfig.skip_edge = false;
            displayConfig.skip_lut = skipLut;  // LUT applies to display link
            displayConfig.skip_regional = skipRegional;  // Regional refinement (off by default)
            displayConfig.geos_max_iter = 350;  // More iterations for 36 dials
            displayConfig.geos_threshold = threshold;
            displayConfig.geos_mode = geos::Mode::DISPLAY;
            displayConfig.optimizer = optimizer;
            displayConfig.aceo_with_cov = withCov;
            displayConfig.aceo_save_cov = saveCov;

            geos::Result displayResult = geosTask->run(body, displayLink, displayConfig, progressCallback);
            std::cout << std::endl;
            std::cout << "  Display iterations: " << displayResult.geos_iterations << std::endl;
            std::cout << "  Final spectral: " << std::fixed << std::setprecision(4)
                      << displayResult.loss.spectral << " (" << std::setprecision(2)
                      << (displayResult.loss.spectral * 100) << "%)" << std::endl;

            linkPtr = &displayLink;
            links = {&linearLink, &displayLink};
        }

        // ------------------------------------------------------------
        // Save link(s)
        // ------------------------------------------------------------
        std::cout << "\n[SAVE] Writing tune settings (" << links.size() << " link" << (links.size() > 1 ? "s" : "") << ")..." << std::endl;
        std::string tunePath = saveArea + "/tune.json";
        if (!data::links::save(links, tunePath))
        {
            throw std::runtime_error("Failed to save: " + tunePath);
        }
        std::cout << "  Saved: " << tunePath << std::endl;

        // Save fine outputs if requested
        if (fine)
        {
            std::cout << "\n[FINE] Saving intermediate images and metadata..." << std::endl;

            // meta.json - camera metadata
            std::string metaPath = fineArea + "/meta.json";
            if (data::info::save(info, metaPath))
            {
                std::cout << "  Saved: " << metaPath << " (camera metadata)" << std::endl;
            }

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
