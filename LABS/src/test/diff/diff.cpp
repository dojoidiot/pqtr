// test_diff.cpp
// Validation test for loss metrics (diff module)
//
// Tests:
// 1. Spectral loss: identical images → 0, different → >0
// 2. Loss ordering: more different → higher loss
// 3. Task caching works correctly
//
// Usage: test_diff <input.ARW> <output_dir>

#include <pipe.hpp>
#include <tune.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <tool.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <cmath>

struct TestResult {
    bool passed = false;
    std::string message;
};

// Test spectral loss with identical images
TestResult test_identical(tune::Task& task, tune::View& img)
{
    TestResult r;

    tune::Data loss = task.diff(img);

    // Allow small floating point errors (including -0.0)
    if (loss.spectral < -0.001f) {
        r.message = "Negative spectral loss: " + std::to_string(loss.spectral);
        return r;
    }

    if (std::abs(loss.spectral) > 0.01f) {
        r.message = "Loss too high for identical: " + std::to_string(loss.spectral * 100) + "%";
        return r;
    }

    r.passed = true;
    r.message = "Identical: spectral=" + std::to_string(std::abs(loss.spectral) * 100) +
                "%, freq=" + std::to_string(loss.frequency);
    return r;
}

// Test spectral loss with modified image
TestResult test_modified(tune::Task& task, tune::View& original)
{
    TestResult r;

    // Create modified version (shift colors)
    tune::View modified;
    original.copyTo(modified);

    // Add color shift
    cv::add(modified, cv::Scalar(20, 0, -20), modified);

    tune::Data loss = task.diff(modified);

    if (loss.spectral < 0.0f) {
        r.message = "Negative spectral loss";
        return r;
    }

    if (loss.spectral < 0.001f) {
        r.message = "Loss too low for modified image: " + std::to_string(loss.spectral * 100) + "%";
        return r;
    }

    r.passed = true;
    r.message = "Modified: spectral=" + std::to_string(loss.spectral * 100) +
                "%, freq=" + std::to_string(loss.frequency);
    return r;
}

// Test loss ordering (more different = higher loss)
TestResult test_ordering(tune::Task& task, tune::View& original)
{
    TestResult r;

    // Create two levels of modification
    tune::View small_mod, large_mod;
    original.copyTo(small_mod);
    original.copyTo(large_mod);

    cv::add(small_mod, cv::Scalar(10, 0, -10), small_mod);
    cv::add(large_mod, cv::Scalar(40, 0, -40), large_mod);

    tune::Data small_loss = task.diff(small_mod);
    tune::Data large_loss = task.diff(large_mod);

    if (large_loss.spectral <= small_loss.spectral) {
        r.message = "Ordering wrong: small=" + std::to_string(small_loss.spectral * 100) +
                    "%, large=" + std::to_string(large_loss.spectral * 100) + "%";
        return r;
    }

    r.passed = true;
    r.message = "Ordering correct: small=" + std::to_string(small_loss.spectral * 100) +
                "% < large=" + std::to_string(large_loss.spectral * 100) + "%";
    return r;
}

// Test visual diff output
TestResult test_visual_diff(tune::Task& task, tune::View& original, const std::string& outputDir)
{
    TestResult r;

    // Create modified version
    tune::View modified;
    original.copyTo(modified);
    cv::add(modified, cv::Scalar(30, -15, -30), modified);

    tune::View diffView = task.view(modified, 5.0f);

    if (diffView.empty()) {
        r.message = "Visual diff returned empty";
        return r;
    }

    // Save diff image
    std::string path = outputDir + "/diff_visual.png";
    cv::Mat diffMat;
    diffView.copyTo(diffMat);
    cv::imwrite(path, diffMat);

    r.passed = true;
    r.message = "Saved visual diff: " + std::to_string(diffMat.cols) + "x" + std::to_string(diffMat.rows);
    return r;
}

void printResult(const std::string& name, const TestResult& r)
{
    std::cout << (r.passed ? "  ✓ " : "  ✗ ") << name << ": " << r.message << std::endl;
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
    std::cout << std::endl;

    try {
        // Load RAW and get processed image
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
        pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(rawPath));
        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));

        if (!head) {
            std::cerr << "Failed to decode RAW" << std::endl;
            return 1;
        }

        // Get image for testing (use body view at 512px)
        tune::View testImg = head->body(512).view();

        if (testImg.empty()) {
            std::cerr << "Failed to get test image" << std::endl;
            return 1;
        }

        std::cout << "Test image: " << testImg.cols << "x" << testImg.rows << std::endl;
        std::cout << std::endl;

        // Create tune task with test image as target
        pqtr::Hold<tune::Task> task = tune::make(testImg);

        std::cout << "Running tests..." << std::endl;
        std::cout << std::endl;

        int failed = 0;

        TestResult r1 = test_identical(*task, testImg);
        printResult("Identical images", r1);
        if (!r1.passed) failed++;

        TestResult r2 = test_modified(*task, testImg);
        printResult("Modified image", r2);
        if (!r2.passed) failed++;

        TestResult r3 = test_ordering(*task, testImg);
        printResult("Loss ordering", r3);
        if (!r3.passed) failed++;

        TestResult r4 = test_visual_diff(*task, testImg, outputDir);
        printResult("Visual diff", r4);
        if (!r4.passed) failed++;

        std::cout << std::endl;
        if (failed == 0) {
            std::cout << "✓ diff: PASSED (all tests)" << std::endl;
            return 0;
        } else {
            std::cout << "✗ diff: FAILED (" << failed << " tests)" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
