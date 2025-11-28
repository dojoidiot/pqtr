// test_pipe.cpp
// Integration test for HEAD → BODY → TAIL pipeline
//
// Tests:
// 1. HEAD: RAW decode + color science (WB, matrix)
// 2. BODY: Link creation, dial operations
// 3. TAIL: PNG output
//
// Usage: test_pipe <input.ARW> <output_dir>

#include <pipe.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <tool.hpp>
#include <iostream>
#include <fstream>
#include <chrono>

using namespace std::chrono;

// Test result tracking
struct TestResult {
    bool passed = false;
    std::string message;
    double duration_ms = 0;
};

// Global head holder (Hold doesn't have release())
static pqtr::Hold<pipe::Head> g_head;

// Test HEAD: decode RAW and apply color science
TestResult test_head(const std::string& rawPath, pqtr::Hold<pipe::Pipe>& pipeline)
{
    TestResult r;
    auto start = high_resolution_clock::now();

    try {
        pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(rawPath));
        g_head = pipeline->open(std::move(sink));

        if (!g_head) {
            r.message = "Failed to decode RAW";
            return r;
        }

        pipe::Info info = g_head->data().info();

        // Validate metadata
        if (info.find("width") == info.end() || info.find("height") == info.end()) {
            r.message = "Missing dimension metadata";
            return r;
        }

        int width = std::stoi(info["width"]);
        int height = std::stoi(info["height"]);

        if (width <= 0 || height <= 0) {
            r.message = "Invalid dimensions: " + std::to_string(width) + "x" + std::to_string(height);
            return r;
        }

        // Check color space was updated by HEAD
        if (info["color_space"] != "scene_linear_srgb") {
            r.message = "Color space not updated: " + info["color_space"];
            return r;
        }

        r.passed = true;
        r.message = "Decoded " + std::to_string(width) + "x" + std::to_string(height) +
                    " from " + info["camera_model"];
    }
    catch (const std::exception& e) {
        r.message = std::string("Exception: ") + e.what();
    }

    auto end = high_resolution_clock::now();
    r.duration_ms = duration_cast<milliseconds>(end - start).count();
    return r;
}

// Test BODY: create link, manipulate dials
TestResult test_body()
{
    TestResult r;
    auto start = high_resolution_clock::now();

    try {
        // Get body at working size
        pipe::Body& body = g_head->body(1080);

        // Create a link (edit step)
        pipe::Body::Link& link = body.add("test_link");

        // Test dial access - exposure (via colorCorrection)
        link.colorCorrection().exposure().set(0.6f);
        float ev = link.colorCorrection().exposure().get();
        if (ev < 0.59f || ev > 0.61f) {
            r.message = "Exposure dial not set correctly: " + std::to_string(ev);
            return r;
        }

        // Test dial access - tone mapping
        link.toneMapping().contrast().set(0.55f);
        link.toneMapping().curveAdjustment().shadows().set(0.6f);

        // Test dial access - global color
        link.globalColor().saturation().set(0.55f);

        r.passed = true;
        r.message = "Created link with exposure, tone, color dials";
    }
    catch (const std::exception& e) {
        r.message = std::string("Exception: ") + e.what();
    }

    auto end = high_resolution_clock::now();
    r.duration_ms = duration_cast<milliseconds>(end - start).count();
    return r;
}

// Test TAIL: render and save PNG
TestResult test_tail(const std::string& outputDir)
{
    TestResult r;
    auto start = high_resolution_clock::now();

    try {
        pipe::Body& body = g_head->body(1080);
        std::string outputPath = outputDir + "/test_output.png";

        if (!body.tail().save(outputPath, 1080)) {
            r.message = "Failed to save PNG";
            return r;
        }

        // Verify file exists and has content
        std::ifstream check(outputPath, std::ios::binary | std::ios::ate);
        if (!check.good()) {
            r.message = "Output file not created";
            return r;
        }

        size_t size = check.tellg();
        if (size < 1000) {
            r.message = "Output file too small: " + std::to_string(size) + " bytes";
            return r;
        }

        r.passed = true;
        r.message = "Saved " + std::to_string(size / 1024) + " KB to " + outputPath;
    }
    catch (const std::exception& e) {
        r.message = std::string("Exception: ") + e.what();
    }

    auto end = high_resolution_clock::now();
    r.duration_ms = duration_cast<milliseconds>(end - start).count();
    return r;
}

// Test preview extraction
TestResult test_preview()
{
    TestResult r;
    auto start = high_resolution_clock::now();

    try {
        pipe::Data& viewData = g_head->view();
        pipe::View preview = viewData.view();

        if (preview.empty()) {
            r.message = "No preview available";
            return r;
        }

        pipe::Info info = viewData.info();
        int width = std::stoi(info["width"]);
        int height = std::stoi(info["height"]);

        if (width <= 0 || height <= 0) {
            r.message = "Invalid preview dimensions";
            return r;
        }

        std::string style = info.count("creative_style") ? info["creative_style"] : "unknown";
        r.passed = true;
        r.message = "Preview: " + std::to_string(width) + "x" + std::to_string(height) +
                    ", style=" + style;
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
    std::cout << " (" << r.duration_ms << "ms)" << std::endl;
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

    // Create pipeline
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();

    int failed = 0;

    // Run tests
    std::cout << "Running tests..." << std::endl;
    std::cout << std::endl;

    TestResult r1 = test_head(rawPath, pipeline);
    printResult("HEAD (decode + color)", r1);
    if (!r1.passed) failed++;

    if (g_head) {
        TestResult r2 = test_body();
        printResult("BODY (link + dials)", r2);
        if (!r2.passed) failed++;

        TestResult r3 = test_tail(outputDir);
        printResult("TAIL (render + save)", r3);
        if (!r3.passed) failed++;

        TestResult r4 = test_preview();
        printResult("PREVIEW (extraction)", r4);
        if (!r4.passed) failed++;
    }

    std::cout << std::endl;
    if (failed == 0) {
        std::cout << "✓ pipe: PASSED (all tests)" << std::endl;
        return 0;
    } else {
        std::cout << "✗ pipe: FAILED (" << failed << " tests)" << std::endl;
        return 1;
    }
}
