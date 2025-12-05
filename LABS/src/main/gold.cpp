// gold.cpp
// Stage-aware optimizer test binary
//
// Tests the STAGED mode: VIEW (6 tone dials) then POPS (39 color dials)
// Each phase uses stage-specific loss functions to reduce dial interference.
//
// Usage:
//   gold <source.ARW> preview --save-area <dir>
//
// Output:
//   <dir>/<basename>/head.png   - camera preview (reference)
//   <dir>/<basename>/tail.png   - pipeline output
//   <dir>/<basename>/diff.png   - difference x5
//   <dir>/<basename>/tune.json  - dial settings

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <geos.hpp>
#include <data.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;

// Parse comma-separated poly_coeffs from info string
static bool parsePolyCoeffs(const std::string& str, float* coeffs, int count)
{
    std::istringstream iss(str);
    std::string token;
    int i = 0;
    while (std::getline(iss, token, ',') && i < count)
    {
        try { coeffs[i++] = std::stof(token); }
        catch (...) { return false; }
    }
    return i == count;
}

void printUsage(const char* prog)
{
    std::cerr << "Usage: " << prog << " <source.ARW> preview --save-area <dir>\n\n";
    std::cerr << "Stage-aware optimizer test (STAGED mode):\n";
    std::cerr << "  Phase 0: Camera Math - apply polynomial transform\n";
    std::cerr << "  Phase 1: VIEW - optimize 6 tone dials with viewLoss()\n";
    std::cerr << "  Phase 2: POPS - optimize 39 color dials with popsLoss()\n";
    std::cerr << "  Phase 3: Joint - refine all 45 dials\n\n";
    std::cerr << "Target is always the embedded preview (match camera JPEG).\n";
}

int main(int argc, char** argv)
{
    // Handle --help
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { printUsage(argv[0]); return 0; }
    }

    if (argc < 4)
    {
        printUsage(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string sourcePath = argv[1];
    std::string targetPath = argv[2];  // Should be "preview"
    std::string saveArea;

    for (int i = 3; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--save-area" && i + 1 < argc) saveArea = argv[++i];
        else { std::cerr << "Unknown option: " << arg << "\n"; return 1; }
    }

    if (saveArea.empty())
    {
        std::cerr << "Error: --save-area is required\n";
        return 1;
    }

    // Validate target is "preview"
    if (targetPath != "preview" && targetPath != "Preview" && targetPath != "PREVIEW")
    {
        std::cerr << "Error: gold only supports 'preview' target (not external images)\n";
        return 1;
    }

    try
    {
        std::cout << "=== GOLD (Stage-Aware Optimizer) ===" << std::endl;
        std::cout << "Source: " << sourcePath << std::endl;
        std::cout << "Mode: STAGED (VIEW → POPS → Joint)" << std::endl;

        // Extract basename for output directory
        fs::path srcPath(sourcePath);
        std::string basename = srcPath.stem().string();
        fs::path outDir = fs::path(saveArea) / basename;
        fs::create_directories(outDir);
        std::cout << "Output: " << outDir.string() << std::endl;

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

        // Get camera preview
        cv::Mat previewMat;
        head->view().view().copyTo(previewMat);

        // Save head.png (reference)
        cv::imwrite((outDir / "head.png").string(), previewMat);
        std::cout << "  Saved: head.png" << std::endl;

        // Working size
        const int workingSize = 1080;

        // Create body
        std::cout << "\n[BODY] Creating pipeline..." << std::endl;
        pipe::Body& body = head->body(workingSize);

        // Get body dimensions and resize preview to match
        pipe::View initialView = body.view();
        cv::Mat initialMat;
        initialView.copyTo(initialMat);
        cv::Size bodySize(initialMat.cols, initialMat.rows);

        cv::Mat previewResized;
        cv::resize(previewMat, previewResized, bodySize, 0, 0, cv::INTER_AREA);

        // Progress callback
        const char* phaseNames[] = {"HUGE", "MIDS", "TINY"};
        auto progressCallback = [&phaseNames](const geos::Progress& p) {
            if (p.stage == geos::Progress::Stage::GEOS)
            {
                std::cout << "\r  [" << phaseNames[static_cast<int>(p.phase)] << "] "
                          << std::setw(3) << p.iteration << "/" << p.max_iterations
                          << "  loss=" << std::fixed << std::setprecision(4) << p.loss.spectral
                          << "     " << std::flush;
            }
            return true;
        };

        // Optimizer config - use STAGED mode
        geos::Config config;
        config.skip_edge = false;
        config.skip_lut = true;
        config.skip_regional = true;
        config.geos_max_iter = 600;  // More iterations for staged
        config.geos_threshold = 0.005f;
        config.geos_mode = geos::Mode::STAGED;  // Key difference from tune
        config.optimizer = geos::Optimizer::SPSA;  // Pure SPSA for staged

        // ============================================================
        // PHASE 0: Camera Math - apply polynomial transform
        // ============================================================
        std::cout << "\n=== PHASE 0: Camera Math ===" << std::endl;

        pipe::Body::Link& link = body.add("camera");

        // Load polynomial coefficients if available
        if (info.count("poly_coeffs") && !info["poly_coeffs"].empty())
        {
            float polyCoeffs[30];
            if (parsePolyCoeffs(info["poly_coeffs"], polyCoeffs, 30))
            {
                link.polyColor().setCoeffs(polyCoeffs);
                std::cout << "  Applied 30-coefficient polynomial transform" << std::endl;
            }
        }

        // Create preview UMat
        cv::UMat previewUMat;
        previewResized.copyTo(previewUMat);

        // Measure error after Camera Math
        cv::Mat afterPolyMat;
        body.view().copyTo(afterPolyMat);
        cv::UMat afterPolyUMat;
        afterPolyMat.copyTo(afterPolyUMat);
        pqtr::Hold<geos::Task> measureTask = geos::make(previewUMat);
        geos::Data afterPoly = measureTask->diff(afterPolyUMat);
        std::cout << "  After poly: " << std::fixed << std::setprecision(1)
                  << (afterPoly.spectral * 100) << "%" << std::endl;

        // ============================================================
        // PHASE 1-3: STAGED optimization (VIEW → POPS → Joint)
        // ============================================================
        std::cout << "\n=== STAGED Optimization ===" << std::endl;
        std::cout << "  Phase 1: VIEW (6 dials) - viewLoss()" << std::endl;
        std::cout << "  Phase 2: POPS (39 dials) - popsLoss()" << std::endl;
        std::cout << "  Phase 3: Joint (45 dials) - geodesicLoss()" << std::endl;

        // Create geos task
        pqtr::Hold<geos::Task> geosTask = geos::make(previewUMat);

        // Run staged optimization
        geos::Result result = geosTask->run(body, link, config, progressCallback);
        std::cout << std::endl;

        // Final result
        std::cout << "\n=== RESULT ===" << std::endl;
        std::cout << "  Final loss: " << std::fixed << std::setprecision(1)
                  << (result.loss.spectral * 100) << "%" << std::endl;
        std::cout << "  Iterations: " << result.geos_iterations << std::endl;

        // Save outputs
        cv::Mat tailMat;
        body.view().copyTo(tailMat);
        cv::imwrite((outDir / "tail.png").string(), tailMat);
        std::cout << "  Saved: tail.png" << std::endl;

        // Compute and save diff
        cv::UMat tailUMat;
        tailMat.copyTo(tailUMat);
        cv::UMat diffUMat = geosTask->view(tailUMat, 5.0f);
        cv::Mat diffMat;
        diffUMat.copyTo(diffMat);
        cv::imwrite((outDir / "diff.png").string(), diffMat);
        std::cout << "  Saved: diff.png" << std::endl;

        // Save dial settings
        std::string tunePath = (outDir / "tune.json").string();
        data::link::save(link, tunePath);
        std::cout << "  Saved: tune.json" << std::endl;

        std::cout << "\n=== DONE ===" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
