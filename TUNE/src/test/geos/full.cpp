// full.cpp
// Full optimizer test with LUT estimation
//
// Tests:
// 1. Full SPSA optimization (all phases including LUT)
// 2. Validates improvement over baseline
// 3. Saves edit.json with dials + LUT
//
// Usage: tune_full <input.ARW> <output_dir> [--mode blockwise|full|linear]

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

constexpr int OUTPUT_SIZE = 1080;  // Social media size for outputs
constexpr int WORKING_SIZE = 512;  // Optimization proxy size

geos::Mode parseMode(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
        {
            if (std::strcmp(argv[i+1], "full") == 0) return geos::Mode::FULL_35D;
            if (std::strcmp(argv[i+1], "linear") == 0) return geos::Mode::LINEAR_ONLY;
        }
    }
    return geos::Mode::BLOCKWISE;
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.ARW> <output_dir> [--mode blockwise|full|linear]" << std::endl;
        return 1;
    }

    std::string rawPath = argv[1];
    std::string outputDir = argv[2];

    std::cout << "=== Tune Full Test ===" << std::endl;
    std::cout << "Input: " << rawPath << std::endl;
    std::cout << "Output: " << outputDir << std::endl;
    std::cout << std::endl;

    try
    {
        // ================================================================
        // SETUP: Load RAW, get target (camera preview)
        // ================================================================

        std::cout << "[SETUP] Loading: " << rawPath << std::endl;

        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
        pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(rawPath));

        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
        if (!head) throw std::runtime_error("Failed to decode RAW");

        pipe::Info info = head->data().info();
        std::cout << "  Decoded: " << info["width"] << "x" << info["height"] << std::endl;
        std::cout << "  Camera: " << info["camera_model"] << std::endl;

        // Save head.png (camera preview - the target)
        std::cout << "\n[1] Saving head.png (camera preview = target)..." << std::endl;
        cv::UMat headView = head->view().view();
        cv::Mat headMat;
        headView.copyTo(headMat);
        int headMax = std::max(headMat.cols, headMat.rows);
        float headScale = static_cast<float>(OUTPUT_SIZE) / headMax;
        cv::Mat headResized;
        cv::resize(headMat, headResized, cv::Size(), headScale, headScale, cv::INTER_AREA);
        cv::imwrite(outputDir + "/head.png", headResized);
        std::cout << "  Saved: " << outputDir << "/head.png (" << headResized.cols << "x" << headResized.rows << ")" << std::endl;

        // Save body.png (baseline - no edits)
        std::cout << "\n[2] Saving body.png (baseline, no edits)..." << std::endl;
        pipe::Body& bodyBaseline = head->body(OUTPUT_SIZE);
        cv::UMat bodyView = bodyBaseline.view();
        cv::Mat bodyMat;
        bodyView.copyTo(bodyMat);
        cv::imwrite(outputDir + "/body.png", bodyMat);
        std::cout << "  Saved: " << outputDir << "/body.png" << std::endl;

        // Compute baseline loss
        cv::Mat headForDiff;
        cv::resize(headResized, headForDiff, cv::Size(bodyMat.cols, bodyMat.rows));
        cv::UMat headU, bodyU;
        headForDiff.copyTo(headU);
        bodyMat.copyTo(bodyU);
        pqtr::Hold<geos::Task> tuneTask = geos::make(headU);
        geos::Data baselineLoss = tuneTask->diff(bodyU);
        std::cout << "  Baseline loss: " << std::fixed << std::setprecision(2)
                  << (baselineLoss.spectral * 100) << "%" << std::endl;

        // ================================================================
        // PHASE 1: OPTIMIZE (what `tune` tool does)
        // ================================================================

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "PHASE 1: OPTIMIZE (tune)" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        // Need fresh head for optimization (body was consumed above)
        pqtr::Hold<pqtr::Sink> rawSink2(pqtr::Tool::read(rawPath));
        pqtr::Hold<pipe::Head> head2 = pipeline->open(std::move(rawSink2));

        // Prepare target at working size
        cv::UMat targetResized;
        cv::resize(headU, targetResized, cv::Size(),
                   static_cast<float>(WORKING_SIZE) / OUTPUT_SIZE,
                   static_cast<float>(WORKING_SIZE) / OUTPUT_SIZE, cv::INTER_AREA);

        pipe::Body& bodyOpt = head2->body(WORKING_SIZE);
        pipe::Body::Link& linkOpt = bodyOpt.add("tune");

        pqtr::Hold<geos::Task> optTask = geos::make(targetResized);

        geos::Mode mode = parseMode(argc, argv);
        const char* modeName = (mode == geos::Mode::FULL_35D) ? "full" :
                               (mode == geos::Mode::LINEAR_ONLY) ? "linear" : "blockwise";

        geos::Config config;
        config.geos_mode = mode;
        config.geos_max_iter = 500;
        config.geos_multi_starts = 5;
        config.skip_edge = false;
        config.skip_lut = (mode == geos::Mode::LINEAR_ONLY);

        std::cout << "Optimizing (mode: " << modeName << ")..." << std::endl;

        const char* phaseNames[] = {"HUGE", "MIDS", "TINY"};
        geos::Result result = optTask->run(bodyOpt, linkOpt, config,
            [&phaseNames](const geos::Progress& p) {
                if (p.stage == geos::Progress::Stage::GEOS)
                {
                    std::cout << "\r  [" << phaseNames[static_cast<int>(p.phase)] << "] "
                              << std::setw(3) << p.iteration << "/" << p.max_iterations
                              << "  loss=" << std::fixed << std::setprecision(4) << p.loss.spectral
                              << "     " << std::flush;
                }
                return true;
            });

        std::cout << std::endl;
        std::cout << "  Final loss: " << std::fixed << std::setprecision(2)
                  << (result.loss.spectral * 100) << "%" << std::endl;
        std::cout << "  Iterations: " << result.geos_iterations << std::endl;

        // Save edit.json
        std::cout << "\n[3] Saving edit.json (optimized Link settings)..." << std::endl;
        std::string editPath = outputDir + "/edit.json";
        if (!data::link::save(linkOpt, editPath))
        {
            throw std::runtime_error("Failed to save edit.json");
        }
        std::cout << "  Saved: " << editPath << std::endl;

        // ================================================================
        // PHASE 2: APPLY (what `labs` tool does)
        // ================================================================

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "PHASE 2: APPLY (labs)" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        // Fresh load - simulates running labs separately
        std::cout << "Loading RAW fresh (simulates separate labs run)..." << std::endl;
        pqtr::Hold<pqtr::Sink> rawSink3(pqtr::Tool::read(rawPath));
        pqtr::Hold<pipe::Head> head3 = pipeline->open(std::move(rawSink3));

        pipe::Body& bodyApply = head3->body(0);  // Full res for output

        // Load edit.json into new link
        std::cout << "Loading edit.json..." << std::endl;
        pipe::Body::Link& linkApply = bodyApply.add("edit");
        if (!data::link::load(linkApply, editPath))
        {
            throw std::runtime_error("Failed to load edit.json");
        }
        std::cout << "  Applied!" << std::endl;

        // Save tail.png via TAIL
        std::cout << "\n[4] Saving tail.png (final output with edits)..." << std::endl;
        std::string tailPath = outputDir + "/tail.png";
        if (!bodyApply.tail().save(tailPath, OUTPUT_SIZE))
        {
            throw std::runtime_error("Failed to save tail.png");
        }
        std::cout << "  Saved: " << tailPath << std::endl;

        // ================================================================
        // VERIFY: Compare tail to target
        // ================================================================

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "VERIFY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        // Load tail.png and compute final loss
        cv::Mat tailMat = cv::imread(tailPath);
        cv::Mat tailForDiff;
        cv::resize(tailMat, tailForDiff, cv::Size(headForDiff.cols, headForDiff.rows));
        cv::UMat tailU;
        tailForDiff.copyTo(tailU);

        geos::Data finalLoss = tuneTask->diff(tailU);
        std::cout << "  Baseline loss: " << std::fixed << std::setprecision(2)
                  << (baselineLoss.spectral * 100) << "%" << std::endl;
        std::cout << "  Final loss:    " << std::fixed << std::setprecision(2)
                  << (finalLoss.spectral * 100) << "%" << std::endl;
        std::cout << "  Improvement:   " << std::fixed << std::setprecision(1)
                  << ((baselineLoss.spectral - finalLoss.spectral) / baselineLoss.spectral * 100)
                  << "%" << std::endl;

        // Save diff.png (visual difference)
        std::cout << "\n[5] Saving diff.png (head vs tail)..." << std::endl;
        cv::UMat diffU = tuneTask->view(tailU);
        cv::Mat diffMat;
        diffU.copyTo(diffMat);
        cv::imwrite(outputDir + "/diff.png", diffMat);
        std::cout << "  Saved: " << outputDir << "/diff.png" << std::endl;

        // ================================================================
        // SUMMARY
        // ================================================================

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "SUMMARY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "Outputs in " << outputDir << ":" << std::endl;
        std::cout << "  head.png  - Camera preview (target)" << std::endl;
        std::cout << "  body.png  - Baseline (no edits)" << std::endl;
        std::cout << "  edit.json - Optimized Link settings" << std::endl;
        std::cout << "  tail.png  - Final output (with edit.json)" << std::endl;
        std::cout << "  diff.png  - Visual difference (head vs tail)" << std::endl;
        std::cout << "\nWorkflow demonstrated:" << std::endl;
        std::cout << "  tune photo.ARW --output edit.json" << std::endl;
        std::cout << "  labs photo.ARW --output tail.png --edit edit.json" << std::endl;

        std::cout << "\n[OK] Test complete!" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
