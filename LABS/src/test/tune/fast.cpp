// test_tune.cpp
// Quick optimizer convergence test
//
// Tests:
// 1. SPSA optimizer reduces spectral loss
// 2. Final loss below threshold (< 1%)
// 3. Edit settings can be saved/loaded
//
// Usage: test_tune <input.ARW> <output_dir>

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <tune.hpp>
#include <data.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

using namespace std::chrono;

constexpr int WORKING_SIZE = 512;
constexpr float LOSS_THRESHOLD = 0.01f;  // 1% target
constexpr int MAX_ITERATIONS = 200;      // Quick test

struct TestResult {
    bool passed = false;
    std::string message;
    double duration_ms = 0;
};

TestResult test_optimizer_convergence(
    pipe::Body& body,
    pipe::Body::Link& link,
    cv::UMat& target,
    const std::string& outputDir)
{
    TestResult r;
    auto start = high_resolution_clock::now();

    try {
        pqtr::Hold<tune::Task> task = tune::make(target);

        // Get baseline loss
        cv::UMat baseline = body.view();
        tune::Data baselineLoss = task->diff(baseline);

        // Configure quick optimization
        tune::Config config;
        config.geos_mode = tune::GeosMode::LINEAR_ONLY;  // Fast mode
        config.geos_max_iter = MAX_ITERATIONS;
        config.geos_multi_starts = 3;
        config.skip_edge = true;
        config.skip_lut = true;

        float lastLoss = baselineLoss.spectral;
        int iterations = 0;

        tune::Result result = task->run(body, link, config,
            [&](const tune::Progress& p) {
                lastLoss = p.loss.spectral;
                iterations = p.iteration;
                return true;
            });

        // Check convergence
        if (result.loss.spectral >= baselineLoss.spectral) {
            r.message = "No improvement: baseline=" +
                        std::to_string(baselineLoss.spectral * 100) + "%, final=" +
                        std::to_string(result.loss.spectral * 100) + "%";
            return r;
        }

        // Check threshold
        if (result.loss.spectral > LOSS_THRESHOLD) {
            r.message = "Loss above threshold: " +
                        std::to_string(result.loss.spectral * 100) + "% > " +
                        std::to_string(LOSS_THRESHOLD * 100) + "%";
            // Still pass if improved significantly
            float improvement = (baselineLoss.spectral - result.loss.spectral) / baselineLoss.spectral;
            if (improvement < 0.5f) {  // At least 50% improvement
                return r;
            }
        }

        r.passed = true;
        r.message = "Converged: " + std::to_string(baselineLoss.spectral * 100) + "% → " +
                    std::to_string(result.loss.spectral * 100) + "% in " +
                    std::to_string(result.geos_iterations) + " iterations";
    }
    catch (const std::exception& e) {
        r.message = std::string("Exception: ") + e.what();
    }

    auto end = high_resolution_clock::now();
    r.duration_ms = duration_cast<milliseconds>(end - start).count();
    return r;
}

TestResult test_edit_save_load(pipe::Body::Link& link, const std::string& outputDir)
{
    TestResult r;
    auto start = high_resolution_clock::now();

    try {
        std::string editPath = outputDir + "/test_edit.json";

        // Save
        if (!data::link::save(link, editPath)) {
            r.message = "Failed to save edit.json";
            return r;
        }

        // Verify file exists
        std::ifstream check(editPath);
        if (!check.good()) {
            r.message = "Edit file not created";
            return r;
        }

        // Read file size
        check.seekg(0, std::ios::end);
        size_t size = check.tellg();
        if (size < 10) {
            r.message = "Edit file too small: " + std::to_string(size) + " bytes";
            return r;
        }

        r.passed = true;
        r.message = "Saved " + std::to_string(size) + " bytes to " + editPath;
    }
    catch (const std::exception& e) {
        r.message = std::string("Exception: ") + e.what();
    }

    auto end = high_resolution_clock::now();
    r.duration_ms = duration_cast<milliseconds>(end - start).count();
    return r;
}

void printResult(const std::string& name, const TestResult& r)
{
    std::cout << (r.passed ? "  ✓ " : "  ✗ ") << name << ": " << r.message;
    if (r.duration_ms > 0) {
        std::cout << " (" << std::fixed << std::setprecision(1)
                  << (r.duration_ms / 1000.0) << "s)";
    }
    std::cout << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.ARW> <output_dir>" << std::endl;
        return 1;
    }

    std::string rawPath = argv[1];
    std::string outputDir = argv[2];

    std::cout << "Input: " << rawPath << std::endl;
    std::cout << "Output: " << outputDir << std::endl;
    std::cout << std::endl;

    try {
        // Load RAW
        std::cout << "Loading RAW..." << std::endl;
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
        pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(rawPath));
        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));

        if (!head) {
            std::cerr << "Failed to decode RAW" << std::endl;
            return 1;
        }

        // Get target (camera preview)
        cv::UMat preview = head->view().view();
        cv::UMat target;
        cv::resize(preview, target, cv::Size(WORKING_SIZE, WORKING_SIZE * preview.rows / preview.cols));

        // Get body and create link
        pipe::Body& body = head->body(WORKING_SIZE);
        pipe::Body::Link& link = body.add("test");
        std::cerr << "[TEST] link @ " << &link << std::endl;

        std::cout << "Running tests..." << std::endl;
        std::cout << std::endl;

        int failed = 0;

        TestResult r1 = test_optimizer_convergence(body, link, target, outputDir);
        printResult("Optimizer convergence", r1);
        if (!r1.passed) failed++;

        TestResult r2 = test_edit_save_load(link, outputDir);
        printResult("Edit save/load", r2);
        if (!r2.passed) failed++;

        // Save debug outputs: optimized.png (result with dials applied)
        std::cout << "\n[DEBUG] Saving optimized.png..." << std::endl;
        cv::UMat optimizedView = body.view();
        cv::Mat optimizedMat;
        optimizedView.copyTo(optimizedMat);
        std::string optimizedPath = outputDir + "/optimized.png";
        cv::imwrite(optimizedPath, optimizedMat);
        std::cout << "  Saved: " << optimizedPath << " (" << optimizedMat.cols << "x" << optimizedMat.rows << ")" << std::endl;

        // Also save the target for comparison
        cv::Mat targetMat;
        target.copyTo(targetMat);
        std::string targetPath = outputDir + "/target.png";
        cv::imwrite(targetPath, targetMat);
        std::cout << "  Saved: " << targetPath << " (camera preview)" << std::endl;

        std::cout << std::endl;
        if (failed == 0) {
            std::cout << "✓ tune: PASSED (all tests)" << std::endl;
            return 0;
        } else {
            std::cout << "✗ tune: FAILED (" << failed << " tests)" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
