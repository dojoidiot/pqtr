// test_lute.cpp - LUTE test suite
// Tests camera profile transforms and persistence

#include <lute.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;
using Name = std::string;

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) \
    do { \
        tests_total++; \
        std::cout << "  " << name << "... "; \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        std::cout << "PASS\n"; \
    } while(0)

#define FAIL(msg) \
    do { \
        std::cout << "FAIL: " << msg << "\n"; \
    } while(0)

// Create test image (gradient)
static cv::UMat createTestImage(int w, int h)
{
    cv::Mat cpu(h, w, CV_32FC3);
    for (int y = 0; y < h; y++)
    {
        float* row = cpu.ptr<float>(y);
        for (int x = 0; x < w; x++)
        {
            float u = float(x) / (w - 1);
            float v = float(y) / (h - 1);
            row[x * 3 + 0] = u;       // B
            row[x * 3 + 1] = v;       // G
            row[x * 3 + 2] = (u + v) / 2; // R
        }
    }
    cv::UMat result;
    cpu.copyTo(result);
    return result;
}

// Test factory creation
static void test_create()
{
    TEST("create()");

    auto lute = lute::create();
    if (!lute)
    {
        FAIL("create() returned nullptr");
        return;
    }

    if (lute->profile() != nullptr)
    {
        FAIL("profile should be null before setKey()");
        return;
    }

    PASS();
}

// Test setKey and profile identity
static void test_setKey()
{
    TEST("setKey()");

    auto lute = lute::create();
    lute->setKey("Sony_ILCE-7M4", "Standard", "Off");

    auto* p = lute->profile();
    if (!p)
    {
        FAIL("profile is null after setKey()");
        return;
    }

    if (p->cameraModel() != "Sony_ILCE-7M4")
    {
        FAIL("wrong camera model");
        return;
    }

    if (p->creativeStyle() != "Standard")
    {
        FAIL("wrong creative style");
        return;
    }

    if (p->key() != "Sony_ILCE-7M4_Standard_Off")
    {
        FAIL("wrong key: " + p->key());
        return;
    }

    PASS();
}

// Test BaseCurve
static void test_base_curve()
{
    TEST("BaseCurve");

    auto lute = lute::create();
    lute->setKey("Test", "Style", "");

    auto& bc = lute->profile()->baseCurve();

    // Initially inactive
    if (bc.isActive())
    {
        FAIL("should be inactive initially");
        return;
    }

    // Set custom curve
    std::array<float, lute::Lute::BaseCurve::CURVE_SIZE> curve;
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = std::pow(float(i) / 255.0f, 2.2f); // gamma

    bc.setCurve(curve.data());

    if (!bc.isActive())
    {
        FAIL("should be active after setCurve");
        return;
    }

    // Verify curve data
    const float* data = bc.curve();
    float err = std::abs(data[255] - 1.0f);
    if (err > 0.001f)
    {
        FAIL("curve[255] should be 1.0");
        return;
    }

    PASS();
}

// Test PolyColor
static void test_poly_color()
{
    TEST("PolyColor");

    auto lute = lute::create();
    lute->setKey("Test", "Style", "");

    auto& pc = lute->profile()->polyColor();

    if (pc.isActive())
    {
        FAIL("should be inactive initially");
        return;
    }

    // Test estimate with synthetic pair
    cv::UMat base = createTestImage(64, 64);

    // Create slightly modified target (simulate camera processing)
    cv::Mat target_cpu;
    base.copyTo(target_cpu);
    target_cpu *= 0.9f; // darken
    target_cpu += 0.05f; // lift blacks
    target_cpu.convertTo(target_cpu, CV_8UC3, 255.0);
    cv::UMat target;
    target_cpu.copyTo(target);

    if (!pc.estimate(base, target))
    {
        FAIL("estimate failed");
        return;
    }

    if (!pc.isActive())
    {
        FAIL("should be active after estimate");
        return;
    }

    PASS();
}

// Test LutCurve
static void test_lut_curve()
{
    TEST("LutCurve");

    auto lute = lute::create();
    lute->setKey("Test", "Style", "");

    auto& lc = lute->profile()->lutCurve();

    if (lc.isEstimated())
    {
        FAIL("should not be estimated initially");
        return;
    }

    // Test estimate
    cv::UMat base = createTestImage(64, 64);

    // Create modified target manually (avoid cv::pow OpenCL issues)
    cv::Mat target_cpu;
    base.copyTo(target_cpu);
    for (int y = 0; y < target_cpu.rows; y++)
    {
        float* row = target_cpu.ptr<float>(y);
        for (int x = 0; x < target_cpu.cols * 3; x++)
            row[x] = std::pow(row[x], 0.8f);
    }
    target_cpu.convertTo(target_cpu, CV_8UC3, 255.0);
    cv::UMat target;
    target_cpu.copyTo(target);

    if (!lc.estimate(base, target))
    {
        FAIL("estimate failed");
        return;
    }

    if (!lc.isEstimated())
    {
        FAIL("should be estimated after estimate()");
        return;
    }

    PASS();
}

// Test HsvLut
static void test_hsv_lut()
{
    TEST("HsvLut");

    auto lute = lute::create();
    lute->setKey("Test", "Style", "");

    auto& hsv = lute->profile()->hsvLut();

    if (hsv.isEstimated())
    {
        FAIL("should not be estimated initially");
        return;
    }

    // Test estimate
    cv::UMat base = createTestImage(64, 64);

    cv::Mat target_cpu;
    base.copyTo(target_cpu);
    // Shift hue slightly
    cv::Mat hsv_mat;
    target_cpu.convertTo(target_cpu, CV_8UC3, 255.0);
    cv::cvtColor(target_cpu, hsv_mat, cv::COLOR_BGR2HSV);
    // Add 10 to hue
    for (int y = 0; y < hsv_mat.rows; y++)
    {
        uchar* row = hsv_mat.ptr<uchar>(y);
        for (int x = 0; x < hsv_mat.cols; x++)
            row[x * 3] = (row[x * 3] + 10) % 180;
    }
    cv::cvtColor(hsv_mat, target_cpu, cv::COLOR_HSV2BGR);
    cv::UMat target;
    target_cpu.copyTo(target);

    if (!hsv.estimate(base, target))
    {
        FAIL("estimate failed");
        return;
    }

    if (!hsv.isEstimated())
    {
        FAIL("should be estimated after estimate()");
        return;
    }

    PASS();
}

// Test view (apply transforms)
static void test_view()
{
    TEST("view()");

    auto lute = lute::create();
    lute->setKey("Test", "Style", "");

    cv::UMat in = createTestImage(64, 64);

    // With no active transforms, output should match input
    cv::UMat out = lute->view(in);

    cv::Mat in_cpu, out_cpu;
    in.copyTo(in_cpu);
    out.copyTo(out_cpu);

    // They should be identical (identity transforms)
    cv::Mat diff;
    cv::absdiff(in_cpu, out_cpu, diff);
    double maxDiff = 0;
    cv::minMaxLoc(diff.reshape(1), nullptr, &maxDiff);

    if (maxDiff > 0.001)
    {
        FAIL("identity transforms should not modify image");
        return;
    }

    PASS();
}

// Test tune (full learning)
static void test_tune()
{
    TEST("tune()");

    auto lute = lute::create();
    lute->setKey("Test", "Style", "");

    cv::UMat flat = createTestImage(64, 64);

    // Create preview (simulating camera JPEG)
    cv::Mat preview_cpu;
    flat.copyTo(preview_cpu);
    // Apply gamma manually to avoid cv::pow OpenCL issues
    for (int y = 0; y < preview_cpu.rows; y++)
    {
        float* row = preview_cpu.ptr<float>(y);
        for (int x = 0; x < preview_cpu.cols * 3; x++)
        {
            row[x] = std::pow(row[x], 0.45f) * 1.1f;
            if (row[x] > 1.0f) row[x] = 1.0f;
        }
    }
    preview_cpu.convertTo(preview_cpu, CV_8UC3, 255.0);
    cv::UMat preview;
    preview_cpu.copyTo(preview);

    if (!lute->tune(flat, preview))
    {
        FAIL("tune failed");
        return;
    }

    auto* p = lute->profile();
    if (p->sampleCount() != 1)
    {
        FAIL("sample count should be 1");
        return;
    }

    if (!p->polyColor().isActive())
    {
        FAIL("polyColor should be active after tune");
        return;
    }

    PASS();
}

// Test save/load
static void test_persistence()
{
    TEST("save/load");

    Name test_dir = "/tmp/lute_test/";
    fs::create_directories(test_dir);
    Name test_path = test_dir + "test_profile.lute";

    // Create and tune a profile
    auto lute1 = lute::create();
    lute1->setKey("TestCam", "Vivid", "DROAuto");

    cv::UMat flat = createTestImage(64, 64);
    cv::Mat preview_cpu;
    flat.copyTo(preview_cpu);
    // Apply gamma manually
    for (int y = 0; y < preview_cpu.rows; y++)
    {
        float* row = preview_cpu.ptr<float>(y);
        for (int x = 0; x < preview_cpu.cols * 3; x++)
            row[x] = std::pow(row[x], 0.45f);
    }
    preview_cpu.convertTo(preview_cpu, CV_8UC3, 255.0);
    cv::UMat preview;
    preview_cpu.copyTo(preview);

    lute1->tune(flat, preview);

    // Save
    if (!lute1->profile()->save(test_path))
    {
        FAIL("save failed");
        return;
    }

    // Load into new instance
    auto lute2 = lute::create();
    lute2->setKey("Other", "Style", "");

    if (!lute2->profile()->load(test_path))
    {
        FAIL("load failed");
        return;
    }

    // Verify loaded data
    auto* p = lute2->profile();
    if (p->cameraModel() != "TestCam")
    {
        FAIL("wrong camera model after load");
        return;
    }

    if (p->creativeStyle() != "Vivid")
    {
        FAIL("wrong style after load");
        return;
    }

    if (!p->polyColor().isActive())
    {
        FAIL("polyColor should be active after load");
        return;
    }

    // Cleanup
    fs::remove(test_path);
    fs::remove(test_dir);

    PASS();
}

// Test profile reset
static void test_reset()
{
    TEST("reset()");

    auto lute = lute::create();
    lute->setKey("Test", "Style", "");

    // Tune to activate transforms
    cv::UMat flat = createTestImage(64, 64);
    cv::Mat preview_cpu;
    flat.copyTo(preview_cpu);
    preview_cpu.convertTo(preview_cpu, CV_8UC3, 255.0);
    cv::UMat preview;
    preview_cpu.copyTo(preview);
    lute->tune(flat, preview);

    // Verify active
    if (!lute->profile()->polyColor().isActive())
    {
        FAIL("should be active before reset");
        return;
    }

    // Reset
    lute->profile()->reset();

    if (lute->profile()->polyColor().isActive())
    {
        FAIL("should be inactive after reset");
        return;
    }

    if (lute->profile()->sampleCount() != 0)
    {
        FAIL("sample count should be 0 after reset");
        return;
    }

    PASS();
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::cout << "\nLUTE Test Suite\n";
    std::cout << "===============\n\n";

    test_create();
    test_setKey();
    test_base_curve();
    test_poly_color();
    test_lut_curve();
    test_hsv_lut();
    test_view();
    test_tune();
    test_persistence();
    test_reset();

    std::cout << "\n===============\n";
    std::cout << "Passed: " << tests_passed << "/" << tests_total << "\n\n";

    return (tests_passed == tests_total) ? 0 : 1;
}
