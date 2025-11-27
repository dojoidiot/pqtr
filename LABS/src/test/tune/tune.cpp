// tune.cpp
// Tune test harness
// Loads DSC00202.ARW, runs GEOS optimization to match camera preview
// Outputs at social media size (1080px):
//   1. head.png       - Camera embedded preview (target)
//   2. body.png       - Body view (no edit steps, scene-referred)
//   3. tail.png       - Baseline output (before optimization)
//   4. diff.png       - Visual difference (head vs body baseline)
//   5. tune.json      - Baseline loss metrics
//   6. optimized.png  - Final output (after GEOS optimization)
//   7. diff_optimized.png - Visual difference (head vs optimized)
//
// Usage: make -f Makefile.tune test

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <tune.hpp>
#include <data.hpp>
#include <iostream>
#include <iomanip>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

constexpr int SOCIAL_SIZE = 1080;
const std::string OUTPUT_DIR = "tmp/var/tune/";

int main()
{
    const std::string rawPath = "var/pics/DSC00202.ARW";

    std::cout << "=== Tune Test Harness ===" << std::endl;
    std::cout << "Loading: " << rawPath << std::endl;
    std::cout << "Output size: " << SOCIAL_SIZE << "px" << std::endl;

    try
    {
        // Create pipe instance
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();

        // Load RAW file into Sink
        pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(rawPath));
        std::cout << "  Sink size: " << rawSink->size() << " bytes" << std::endl;

        // HEAD: Decode RAW
        std::cout << "\n[HEAD] Decoding..." << std::endl;
        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
        if (!head)
        {
            throw std::runtime_error("Failed to decode RAW file");
        }

        // Get decoded metadata
        pipe::Info info = head->data().info();
        std::cout << "  Decoded: " << info["width"] << "x" << info["height"] << std::endl;
        std::cout << "  Camera: " << info["camera_make"] << " " << info["camera_model"] << std::endl;

        // 1. Save HEAD view (embedded camera preview) - THE TARGET
        std::cout << "\n[1] Saving head view (camera preview - target)..." << std::endl;
        pipe::View headView = head->view().view();
        cv::Mat headMat;
        headView.copyTo(headMat);

        // Resize head to match body size for fair comparison
        cv::Mat headResized;
        int headMaxDim = std::max(headMat.cols, headMat.rows);
        float headScale = static_cast<float>(SOCIAL_SIZE) / headMaxDim;
        cv::resize(headMat, headResized, cv::Size(), headScale, headScale, cv::INTER_AREA);

        std::string headPath = OUTPUT_DIR + "head.png";
        cv::imwrite(headPath, headResized);
        std::cout << "  Saved: " << headPath << " (" << headResized.cols << "x" << headResized.rows << ")" << std::endl;

        // BODY: Create with no edit steps, using social media working size
        std::cout << "\n[BODY] Creating empty body (no edit steps)..." << std::endl;
        pipe::Body& body = head->body(SOCIAL_SIZE);

        // Verify no links
        pipe::Body::Iterator& iter = body.links();
        int linkCount = 0;
        while (iter.next()) linkCount++;
        std::cout << "  Links: " << linkCount << std::endl;

        // 2. Save BODY view (scene-linear processed through body, gamma encoded)
        std::cout << "\n[2] Saving body view (no edit steps)..." << std::endl;
        pipe::View bodyView = body.view();
        cv::Mat bodyMat;
        bodyView.copyTo(bodyMat);
        std::string bodyPath = OUTPUT_DIR + "body.png";
        cv::imwrite(bodyPath, bodyMat);
        std::cout << "  Saved: " << bodyPath << " (" << bodyMat.cols << "x" << bodyMat.rows << ")" << std::endl;

        // 3. Save TAIL view (final output at social size)
        std::cout << "\n[3] Saving tail output (final)..." << std::endl;
        std::string tailPath = OUTPUT_DIR + "tail.png";
        if (!body.tail().save(tailPath, SOCIAL_SIZE))
        {
            throw std::runtime_error("Failed to save tail output");
        }
        std::cout << "  Saved: " << tailPath << std::endl;

        // 4. Compute diff between head (target) and body (candidate)
        std::cout << "\n[4] Computing diff (head vs body)..." << std::endl;

        // Resize head to exactly match body dimensions
        cv::Mat headForDiff;
        cv::resize(headResized, headForDiff, cv::Size(bodyMat.cols, bodyMat.rows), 0, 0, cv::INTER_AREA);

        cv::UMat headUMat, bodyUMat;
        headForDiff.copyTo(headUMat);
        bodyMat.copyTo(bodyUMat);

        // Create tune task with head as target (features cached)
        pqtr::Hold<tune::Task> tuneTask = tune::make(headUMat);

        // Compute metrics
        tune::Data metrics = tuneTask->diff(bodyUMat);
        std::cout << "  Spectral loss:  " << std::fixed << std::setprecision(4) << metrics.spectral
                  << " (" << std::setprecision(2) << (metrics.spectral * 100) << "%)" << std::endl;
        std::cout << "  Frequency loss: " << std::fixed << std::setprecision(4) << metrics.frequency
                  << " (" << std::setprecision(2) << (metrics.frequency * 100) << "%)" << std::endl;

        // 5. Save diff image
        std::cout << "\n[5] Saving diff image..." << std::endl;
        cv::UMat diffUMat = tuneTask->view(bodyUMat);
        cv::Mat diffMat;
        diffUMat.copyTo(diffMat);
        std::string diffImgPath = OUTPUT_DIR + "diff.png";
        cv::imwrite(diffImgPath, diffMat);
        std::cout << "  Saved: " << diffImgPath << std::endl;

        // 6. Save tune.json using data layer
        std::cout << "\n[6] Saving tune.json (baseline)..." << std::endl;
        std::string tuneJsonPath = OUTPUT_DIR + "tune.json";
        if (!data::tune::save(metrics, tuneJsonPath))
        {
            throw std::runtime_error("Failed to save tune.json");
        }
        std::cout << "  Saved: " << tuneJsonPath << std::endl;

        // DEBUG: Test what happens when we add a link with neutral (0.5) dials
        // NOTE: We use a single link "tune" for both debug and GEOS optimization
        std::cout << "\n[DEBUG] Testing link dial effects..." << std::endl;

        pipe::Body::Link& link = body.add("tune");

        // Test 1: Empty link (no dials set) - should be same as baseline
        {
            cv::UMat view1 = body.view();
            tune::Data loss1 = tuneTask->diff(view1);
            std::cout << "  Empty link:     spectral=" << std::fixed << std::setprecision(4)
                      << loss1.spectral << std::endl;
        }

        // Test 2: Set just exposure to 0.5 (should be neutral)
        link.colorCorrection().exposure().set(0.5f);
        {
            cv::UMat view2 = body.view();
            tune::Data loss2 = tuneTask->diff(view2);
            std::cout << "  +exposure=0.5:  spectral=" << std::fixed << std::setprecision(4)
                      << loss2.spectral << std::endl;
        }

        // Test 3: Set tone mapping dials to 0.5
        link.toneMapping().contrast().set(0.5f);
        link.toneMapping().curveAdjustment().highlights().set(0.5f);
        link.toneMapping().curveAdjustment().shadows().set(0.5f);
        link.toneMapping().clippingPoint().white().set(0.5f);
        link.toneMapping().clippingPoint().black().set(0.5f);
        {
            cv::UMat view3 = body.view();
            tune::Data loss3 = tuneTask->diff(view3);
            std::cout << "  +tonemap=0.5:   spectral=" << std::fixed << std::setprecision(4)
                      << loss3.spectral << " <-- tone_map is the culprit if high" << std::endl;

            // Save this for visual inspection
            cv::Mat debugMat;
            view3.copyTo(debugMat);
            cv::imwrite(OUTPUT_DIR + "debug_tonemap.png", debugMat);
        }

        // Test 4: Set ALL 35 dials to 0.5 (what GEOS init does)
        link.colorCorrection().whiteBalance().temperature(0.5f);
        link.colorCorrection().whiteBalance().tint(0.5f);
        link.globalColor().vibrance().set(0.5f);
        link.globalColor().saturation().set(0.5f);
        link.globalColor().colourDensity().set(0.5f);
        link.selectiveColour().red().hue(0.5f);
        link.selectiveColour().red().saturation(0.5f);
        link.selectiveColour().red().luminance(0.5f);
        link.selectiveColour().orange().hue(0.5f);
        link.selectiveColour().orange().saturation(0.5f);
        link.selectiveColour().orange().luminance(0.5f);
        link.selectiveColour().yellow().hue(0.5f);
        link.selectiveColour().yellow().saturation(0.5f);
        link.selectiveColour().yellow().luminance(0.5f);
        link.selectiveColour().green().hue(0.5f);
        link.selectiveColour().green().saturation(0.5f);
        link.selectiveColour().green().luminance(0.5f);
        link.selectiveColour().cyan().hue(0.5f);
        link.selectiveColour().cyan().saturation(0.5f);
        link.selectiveColour().cyan().luminance(0.5f);
        link.selectiveColour().blue().hue(0.5f);
        link.selectiveColour().blue().saturation(0.5f);
        link.selectiveColour().blue().luminance(0.5f);
        link.selectiveColour().purple().hue(0.5f);
        link.selectiveColour().purple().saturation(0.5f);
        link.selectiveColour().purple().luminance(0.5f);
        link.selectiveColour().magenta().hue(0.5f);
        link.selectiveColour().magenta().saturation(0.5f);
        link.selectiveColour().magenta().luminance(0.5f);
        {
            cv::UMat view4 = body.view();
            tune::Data loss4 = tuneTask->diff(view4);
            std::cout << "  All 35 @ 0.5:   spectral=" << std::fixed << std::setprecision(4)
                      << loss4.spectral << " <-- this is what GEOS starts with" << std::endl;

            cv::Mat debugMat;
            view4.copyTo(debugMat);
            cv::imwrite(OUTPUT_DIR + "debug_all35.png", debugMat);
        }

        std::cout << "  (Baseline was:  spectral=" << std::fixed << std::setprecision(4)
                  << metrics.spectral << ")" << std::endl;

        // 7. Run GEOS optimization
        // Using the same link from debug section (already has all dials at 0.5)
        std::cout << "\n[7] Running GEOS optimization..." << std::endl;
        std::cout << "  Baseline spectral: " << std::fixed << std::setprecision(4)
                  << metrics.spectral << std::endl;

        tune::Config config;
        config.skip_edge = true;  // Only run GEOS for now
        config.geos_max_iter = 500;
        config.geos_multi_starts = 5;

        const char* phaseNames[] = {"HUGE", "MIDS", "TINY"};
        tune::Result result = tuneTask->run(body, link, config,
            [&phaseNames](const tune::Progress& p) {
                if (p.stage == tune::Progress::Stage::GEOS)
                {
                    std::cout << "\r  [" << phaseNames[static_cast<int>(p.phase)] << "] "
                              << std::setw(3) << p.iteration << "/" << p.max_iterations
                              << "  loss=" << std::fixed << std::setprecision(4) << p.loss.spectral
                              << "  r=" << std::setprecision(3) << p.dome.r
                              << "     " << std::flush;
                }
                return true;  // Continue optimization
            });

        std::cout << std::endl;
        std::cout << "  Iterations: " << result.geos_iterations << std::endl;
        std::cout << "  Final spectral: " << std::fixed << std::setprecision(4)
                  << result.loss.spectral << " (" << std::setprecision(2)
                  << (result.loss.spectral * 100) << "%)" << std::endl;

        // 8. Save optimized output
        std::cout << "\n[8] Saving optimized output..." << std::endl;
        std::string optPath = OUTPUT_DIR + "optimized.png";
        if (!body.tail().save(optPath, SOCIAL_SIZE))
        {
            throw std::runtime_error("Failed to save optimized output");
        }
        std::cout << "  Saved: " << optPath << std::endl;

        // 9. Save optimized diff
        std::cout << "\n[9] Saving optimized diff..." << std::endl;
        cv::UMat optView = body.view();
        cv::UMat optDiffUMat = tuneTask->view(optView);
        cv::Mat optDiffMat;
        optDiffUMat.copyTo(optDiffMat);
        std::string optDiffPath = OUTPUT_DIR + "diff_optimized.png";
        cv::imwrite(optDiffPath, optDiffMat);
        std::cout << "  Saved: " << optDiffPath << std::endl;

        std::cout << "\n[OK] All outputs saved to " << OUTPUT_DIR << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
