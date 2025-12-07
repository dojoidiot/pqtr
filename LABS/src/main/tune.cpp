// tune.cpp
// Three-phase vibe creation:
//   Phase 0 (Camera Math): Apply polynomial transform from RAWS (deterministic)
//   Phase 1 (Camera Vibe): Optimize dials to match camera preview (on top of poly)
//   Phase 2 (User Vibe): Optimize dials to match user's edit (on top of camera vibe)
//
// Usage:
//   tune <source.ARW> <target.png|preview> [options]
//
// Default paths (~/.pqtr or --pqtr override):
//   etc/tune.json  Built-in priors (from code)
//   var/tune.json  Learned priors (accumulated)
//   var/tune/      Project outputs
//
// Debug with labs:
//   labs photo.ARW --tune ~/.pqtr/var/tune/tune.json --output photo.png --debug

#include <pqtr.hpp>
#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <geos.hpp>
#include <data.hpp>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <sstream>
#include <vector>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

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
    std::cerr << "Usage: " << prog << " <source.ARW> <target.png|preview> [options]\n\n";
    std::cerr << "Three-phase optimization:\n";
    std::cerr << "  Phase 0: Camera Math - apply polynomial transform (deterministic)\n";
    std::cerr << "  Phase 1: Camera Vibe - match embedded preview (45 dials)\n";
    std::cerr << "  Phase 2: User Vibe - match target (if not 'preview')\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --pqtr <dir>     Override ~/.pqtr root directory\n";
    std::cerr << "  --output <dir>   Override output directory (default: ~/.pqtr/var/tune)\n";
    std::cerr << "  --no-learn       Disable automatic prior learning\n";
    std::cerr << "  --jito           JITO pre-pass warm-start (experimental)\n\n";
    std::cerr << "Paths (relative to --pqtr or ~/.pqtr):\n";
    std::cerr << "  etc/tune.json    Built-in priors (from code)\n";
    std::cerr << "  var/tune.json    Learned priors (accumulated over time)\n";
    std::cerr << "  var/tune/        Project outputs\n\n";
    std::cerr << "Use 'labs --debug' to inspect results.\n";
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
    std::string pqtrRoot;
    std::string outputDir;
    bool useJito = false;
    bool noLearn = false;

    for (int i = 3; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--pqtr" && i + 1 < argc) pqtrRoot = argv[++i];
        else if (arg == "--output" && i + 1 < argc) outputDir = argv[++i];
        else if (arg == "--save-area" && i + 1 < argc) outputDir = argv[++i];  // Legacy
        else if (arg == "--jito") useJito = true;
        else if (arg == "--no-learn") noLearn = true;
        else if (arg == "--help" || arg == "-h") { /* handled above */ }
        else { std::cerr << "Unknown option: " << arg << "\n"; printUsage(argv[0]); return 1; }
    }

    // Resolve pqtr root (default: ~/.pqtr)
    std::string root = pqtr::root(pqtrRoot);

    // Initialize directory structure
    if (!pqtr::init(root))
    {
        std::cerr << "Warning: Could not create pqtr directories in " << root << std::endl;
    }

    // Install built-in priors if etc/tune.json doesn't exist
    // Look for built-in priors relative to executable or in common locations
    std::vector<std::string> builtinPaths = {
        "etc/tune.json",           // Current directory
        "../etc/tune.json",        // One level up
        "../../LABS/etc/tune.json" // From DESK
    };
    for (const auto& path : builtinPaths)
    {
        if (pqtr::installPriors(root, path))
        {
            break;
        }
    }

    // Output directory (default: ~/.pqtr/var/tune)
    std::string saveArea = outputDir.empty() ? pqtr::varTuneDir(root) : outputDir;

    try
    {
        std::cout << "=== TUNE ===" << std::endl;
        std::cout << "Source: " << sourcePath << std::endl;
        std::cout << "Target: " << targetPath << std::endl;
        std::cout << "Output: " << saveArea << "/tune.json" << std::endl;

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
        std::cout << "  BaseCurve: " << (head->hasBaseCurve() ? "yes" : "no") << std::endl;
        std::cout << "  PolyCoeffs: " << (info.count("poly_coeffs") ? "yes" : "no") << std::endl;

        // Check if target is preview-only or external reference
        bool isPreviewOnly = (targetPath == "preview" || targetPath == "Preview" || targetPath == "PREVIEW");

        // Get camera preview (always needed for Phase 1)
        cv::Mat previewMat;
        head->view().view().copyTo(previewMat);

        // Get user target if not preview-only
        cv::Mat userTargetMat;
        if (!isPreviewOnly)
        {
            userTargetMat = cv::imread(targetPath);
            if (userTargetMat.empty())
            {
                throw std::runtime_error("Failed to load target: " + targetPath);
            }
        }

        // Working size
        const int workingSize = 1080;

        // Create body
        std::cout << "\n[BODY] Creating pipeline..." << std::endl;
        pipe::Body& body = head->body(workingSize);

        // Get body dimensions
        pipe::View initialView = body.view();
        cv::Mat initialMat;
        initialView.copyTo(initialMat);
        cv::Size bodySize(initialMat.cols, initialMat.rows);

        // Resize preview to match body
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
            else if (p.stage == geos::Progress::Stage::EDGE)
            {
                std::cout << "\r  [EDGE] "
                          << std::setw(3) << p.iteration << "/" << p.max_iterations
                          << "  freq=" << std::fixed << std::setprecision(4) << p.loss.frequency
                          << "     " << std::flush;
            }
            return true;
        };

        // Optimizer config
        geos::Config config;
        config.skip_edge = false;
        config.skip_lut = false;  // Enable LUT curve estimation (camera math)
        config.skip_regional = true;
        config.geos_max_iter = 500;
        config.geos_threshold = 0.005f;
        config.geos_mode = geos::Mode::FULL_35D;
        config.optimizer = geos::Optimizer::HYBRID;
        config.use_jito = useJito;  // JITO pre-pass warm-start

        // Automatic prior learning using pqtr paths:
        //   Load: var/tune.json (if exists) or etc/tune.json (built-in)
        //   Save: var/tune.json (accumulates over time)
        if (!noLearn)
        {
            std::string loadPath = pqtr::priorsPath(root);  // var or etc
            std::string savePath = pqtr::varTune(root);     // always var

            if (pqtr::exists(loadPath))
            {
                config.aceo_with_cov = loadPath;
                std::cout << "  Priors: " << loadPath << " (loaded)" << std::endl;
            }
            else
            {
                std::cout << "  Priors: " << savePath << " (will create)" << std::endl;
            }

            // Always save updated priors to var/tune.json
            config.aceo_save_cov = savePath;
        }
        else
        {
            std::cout << "  Priors: disabled (--no-learn)" << std::endl;
        }

        // ============================================================
        // PHASE 0: Camera Math - apply polynomial transform
        // ============================================================
        std::cout << "\n=== PHASE 0: Camera Math ===" << std::endl;

        // Create camera link
        pipe::Body::Link& cameraLink = body.add("camera");

        // Load polynomial coefficients if available
        bool hasPolyCoeffs = false;
        if (info.count("poly_coeffs") && !info["poly_coeffs"].empty())
        {
            float polyCoeffs[30];
            if (parsePolyCoeffs(info["poly_coeffs"], polyCoeffs, 30))
            {
                cameraLink.polyColor().setCoeffs(polyCoeffs);
                hasPolyCoeffs = true;
                std::cout << "  Applied 30-coefficient polynomial transform" << std::endl;
            }
            else
            {
                std::cout << "  Warning: Failed to parse poly_coeffs" << std::endl;
            }
        }
        else
        {
            std::cout << "  No poly_coeffs available (using identity)" << std::endl;
        }

        // Create preview UMat for comparison (used in both phases)
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
        // PHASE 1: Camera Vibe - match embedded preview with dials
        // ============================================================
        std::cout << "\n=== PHASE 1: Camera Vibe ===" << std::endl;
        std::cout << "Target: embedded preview" << std::endl;

        // Initialize dials to neutral - poly already did heavy lifting
        cameraLink.colorCorrection().exposure().set(0.5f);
        cameraLink.colorCorrection().whiteBalance().temperature(0.5f);
        cameraLink.colorCorrection().whiteBalance().tint(0.5f);
        cameraLink.toneMapping().contrast().set(0.5f);
        cameraLink.toneMapping().curveAdjustment().highlights().set(0.5f);
        cameraLink.toneMapping().curveAdjustment().shadows().set(0.5f);
        cameraLink.toneMapping().curveAdjustment().toePivot().set(0.5f);
        cameraLink.toneMapping().curveAdjustment().shoulderPivot().set(0.5f);
        cameraLink.toneMapping().clippingPoint().white().set(0.5f);
        cameraLink.toneMapping().clippingPoint().black().set(0.5f);
        cameraLink.globalColor().vibrance().set(0.5f);
        cameraLink.globalColor().saturation().set(0.5f);
        cameraLink.globalColor().colourDensity().set(0.5f);

        // Create geos task for preview optimization
        pqtr::Hold<geos::Task> cameraTask = geos::make(previewUMat);

        // Baseline (after poly, before dial optimization)
        std::cout << "  Starting from: " << std::fixed << std::setprecision(1)
                  << (afterPoly.spectral * 100) << "%" << std::endl;

        // Optimize camera link
        geos::Result cameraResult = cameraTask->run(body, cameraLink, config, progressCallback);
        std::cout << std::endl;
        std::cout << "  Camera Vibe: " << std::fixed << std::setprecision(1)
                  << (cameraResult.loss.spectral * 100) << "%" << std::endl;

        // Storage for links to save
        std::vector<pipe::Body::Link*> links = {&cameraLink};

        // ============================================================
        // PHASE 2: User Vibe - match user's edit (if not preview-only)
        // ============================================================
        if (!isPreviewOnly)
        {
            std::cout << "\n=== PHASE 2: User Vibe ===" << std::endl;
            std::cout << "Target: " << targetPath << std::endl;

            // Resize user target to match body
            cv::Mat userResized;
            cv::resize(userTargetMat, userResized, bodySize, 0, 0, cv::INTER_AREA);

            // Create user link (on top of camera link)
            pipe::Body::Link& userLink = body.add("user");

            // Initialize to neutral (no base curve - camera link has it)
            userLink.colorCorrection().exposure().set(0.5f);
            userLink.colorCorrection().whiteBalance().temperature(0.5f);
            userLink.colorCorrection().whiteBalance().tint(0.5f);
            userLink.toneMapping().contrast().set(0.5f);
            userLink.toneMapping().curveAdjustment().highlights().set(0.5f);
            userLink.toneMapping().curveAdjustment().shadows().set(0.5f);
            userLink.toneMapping().curveAdjustment().toePivot().set(0.5f);
            userLink.toneMapping().curveAdjustment().shoulderPivot().set(0.5f);
            userLink.toneMapping().clippingPoint().white().set(0.5f);
            userLink.toneMapping().clippingPoint().black().set(0.5f);
            userLink.globalColor().vibrance().set(0.5f);
            userLink.globalColor().saturation().set(0.5f);
            userLink.globalColor().colourDensity().set(0.5f);

            // Create geos task for user target
            cv::UMat userUMat;
            userResized.copyTo(userUMat);
            pqtr::Hold<geos::Task> userTask = geos::make(userUMat);

            // Baseline (after camera link)
            cv::Mat afterCameraMat;
            body.view().copyTo(afterCameraMat);
            cv::UMat afterCameraUMat;
            afterCameraMat.copyTo(afterCameraUMat);
            geos::Data userBaseline = userTask->diff(afterCameraUMat);
            std::cout << "  After Camera: " << std::fixed << std::setprecision(1)
                      << (userBaseline.spectral * 100) << "%" << std::endl;

            // Optimize user link
            geos::Result userResult = userTask->run(body, userLink, config, progressCallback);
            std::cout << std::endl;
            std::cout << "  User Vibe: " << std::fixed << std::setprecision(1)
                      << (userResult.loss.spectral * 100) << "%" << std::endl;

            links.push_back(&userLink);
        }

        // Save
        std::cout << "\n[SAVE]" << std::endl;
        std::string tunePath = saveArea + "/tune.json";
        if (!data::links::save(links, tunePath))
        {
            throw std::runtime_error("Failed to save: " + tunePath);
        }
        std::cout << "  " << tunePath << " (" << links.size() << " link" << (links.size() > 1 ? "s" : "") << ")" << std::endl;

        std::cout << "\n[OK]" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
