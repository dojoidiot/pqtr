// Axis Contrast Preservation - Hypothesis 3
// Test harness with synthetic opponent-pair images
//
// Theory: When both poles of an opponent axis exist in a scene,
// holistic optimization averages them toward grey instead of
// preserving contrast on that axis.
//
// This test creates synthetic images with known opponent pairs
// and validates the axis measurement and loss functions.

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <cmath>
#include <string>

// ============================================================
// Opponent Axis Definitions
// ============================================================

// RGB values for the 8-spoke color wheel
namespace Colors {
    const cv::Vec3b WHITE   = {255, 255, 255};
    const cv::Vec3b RED     = {0, 0, 255};      // BGR
    const cv::Vec3b GREEN   = {0, 255, 0};
    const cv::Vec3b BLUE    = {255, 0, 0};
    const cv::Vec3b CYAN    = {255, 255, 0};    // G + B
    const cv::Vec3b MAGENTA = {255, 0, 255};    // R + B
    const cv::Vec3b YELLOW  = {0, 255, 255};    // R + G
    const cv::Vec3b ORANGE  = {0, 165, 255};    // Approx
    const cv::Vec3b TEAL    = {128, 128, 0};    // Approx cyan-green
}

// Axis balance: positive = first pole dominant, negative = second pole dominant
struct AxisBalance {
    float r_c;  // Red vs Cyan
    float g_m;  // Green vs Magenta
    float b_y;  // Blue vs Yellow

    void print(const std::string& label) const {
        std::cout << label << ":\n";
        std::cout << "  R-C: " << r_c << " (+" << (r_c > 0 ? "Red" : "Cyan") << ")\n";
        std::cout << "  G-M: " << g_m << " (+" << (g_m > 0 ? "Green" : "Magenta") << ")\n";
        std::cout << "  B-Y: " << b_y << " (+" << (b_y > 0 ? "Blue" : "Yellow") << ")\n";
    }
};

// ============================================================
// Axis Measurement
// ============================================================

// Convert BGR to opponent-axis space
// Returns (r_c, g_m, b_y) where each is in [-1, 1]
cv::Vec3f bgrToOpponent(const cv::Vec3b& bgr) {
    float b = bgr[0] / 255.0f;
    float g = bgr[1] / 255.0f;
    float r = bgr[2] / 255.0f;

    // Opponent axes (normalized)
    // R-C axis: R - (G+B)/2 ... but simpler: R - C where C = min(G,B) contribution
    // More accurate: project onto Lab-like opponent space

    // Simple approach: use actual opponent definitions
    // Cyan = G + B, so R-C contrast = R - min(G,B)
    // Magenta = R + B, so G-M contrast = G - min(R,B)
    // Yellow = R + G, so B-Y contrast = B - min(R,G)

    // Actually let's use the Lab-like formulation:
    // a* axis (R-G): captures red vs green
    // b* axis (B-Y): captures blue vs yellow
    // But we want R-C which is different...

    // Cleaner: compute saturation contribution per pole
    float c_strength = std::min(g, b);  // Cyan = G+B, limited by smaller
    float m_strength = std::min(r, b);  // Magenta = R+B
    float y_strength = std::min(r, g);  // Yellow = R+G

    float r_c = r - c_strength;         // Red minus cyan contribution
    float g_m = g - m_strength;         // Green minus magenta contribution
    float b_y = b - y_strength;         // Blue minus yellow contribution

    return cv::Vec3f(r_c, g_m, b_y);
}

// Measure axis balance across entire image
AxisBalance measureAxes(const cv::Mat& img) {
    AxisBalance balance = {0, 0, 0};
    int count = 0;

    // Sum opponent values
    double sum_rc = 0, sum_gm = 0, sum_by = 0;

    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            cv::Vec3b bgr = img.at<cv::Vec3b>(y, x);
            cv::Vec3f opp = bgrToOpponent(bgr);

            sum_rc += opp[0];
            sum_gm += opp[1];
            sum_by += opp[2];
            count++;
        }
    }

    if (count > 0) {
        balance.r_c = sum_rc / count;
        balance.g_m = sum_gm / count;
        balance.b_y = sum_by / count;
    }

    return balance;
}

// Measure axis CONTRAST (spread between poles, not just mean)
struct AxisContrast {
    float r_c;  // How much R and C are both present and saturated
    float g_m;
    float b_y;

    void print(const std::string& label) const {
        std::cout << label << ":\n";
        std::cout << "  R-C contrast: " << r_c << "\n";
        std::cout << "  G-M contrast: " << g_m << "\n";
        std::cout << "  B-Y contrast: " << b_y << "\n";
    }
};

// Measure how much contrast exists on each axis
// High value = both poles present and saturated
// Low value = one-sided or desaturated
AxisContrast measureAxisContrast(const cv::Mat& img) {
    // Track saturation of each pole
    double sum_r = 0, sum_c = 0;
    double sum_g = 0, sum_m = 0;
    double sum_b = 0, sum_y = 0;
    int count = 0;

    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            cv::Vec3b bgr = img.at<cv::Vec3b>(y, x);
            float b = bgr[0] / 255.0f;
            float g = bgr[1] / 255.0f;
            float r = bgr[2] / 255.0f;

            // Red saturation: R high, G and B low
            float r_sat = r - std::max(g, b);
            if (r_sat > 0) sum_r += r_sat;

            // Cyan saturation: G and B high, R low
            float c_sat = std::min(g, b) - r;
            if (c_sat > 0) sum_c += c_sat;

            // Green saturation
            float g_sat = g - std::max(r, b);
            if (g_sat > 0) sum_g += g_sat;

            // Magenta saturation
            float m_sat = std::min(r, b) - g;
            if (m_sat > 0) sum_m += m_sat;

            // Blue saturation
            float b_sat = b - std::max(r, g);
            if (b_sat > 0) sum_b += b_sat;

            // Yellow saturation
            float y_sat = std::min(r, g) - b;
            if (y_sat > 0) sum_y += y_sat;

            count++;
        }
    }

    AxisContrast contrast;
    if (count > 0) {
        // Contrast = geometric mean of both poles (high only if both present)
        contrast.r_c = std::sqrt((sum_r / count) * (sum_c / count));
        contrast.g_m = std::sqrt((sum_g / count) * (sum_m / count));
        contrast.b_y = std::sqrt((sum_b / count) * (sum_y / count));
    }

    return contrast;
}

// ============================================================
// Axis Contrast Loss
// ============================================================

float axisContrastLoss(const AxisContrast& target, const AxisContrast& candidate) {
    // Penalize if we're collapsing an axis that exists in target
    const float threshold = 0.01f;  // Minimum contrast to care about

    float loss = 0;

    // R-C axis
    if (target.r_c > threshold) {
        // Target has R-C contrast, penalize reducing it
        float reduction = std::max(0.0f, target.r_c - candidate.r_c);
        loss += reduction;
    }

    // G-M axis
    if (target.g_m > threshold) {
        float reduction = std::max(0.0f, target.g_m - candidate.g_m);
        loss += reduction;
    }

    // B-Y axis
    if (target.b_y > threshold) {
        float reduction = std::max(0.0f, target.b_y - candidate.b_y);
        loss += reduction;
    }

    return loss;
}

// ============================================================
// Synthetic Test Image Generation
// ============================================================

cv::Mat makeAxisTest_RC() {
    // Red + Cyan on white background
    cv::Mat img(256, 256, CV_8UC3, Colors::WHITE);

    // Red patch top-left
    cv::rectangle(img, cv::Point(16, 16), cv::Point(112, 112),
                  cv::Scalar(Colors::RED), -1);

    // Cyan patch bottom-right
    cv::rectangle(img, cv::Point(144, 144), cv::Point(240, 240),
                  cv::Scalar(Colors::CYAN), -1);

    return img;
}

cv::Mat makeAxisTest_GM() {
    // Green + Magenta on white background
    cv::Mat img(256, 256, CV_8UC3, Colors::WHITE);

    cv::rectangle(img, cv::Point(16, 16), cv::Point(112, 112),
                  cv::Scalar(Colors::GREEN), -1);

    cv::rectangle(img, cv::Point(144, 144), cv::Point(240, 240),
                  cv::Scalar(Colors::MAGENTA), -1);

    return img;
}

cv::Mat makeAxisTest_BY() {
    // Blue + Yellow on white background
    cv::Mat img(256, 256, CV_8UC3, Colors::WHITE);

    cv::rectangle(img, cv::Point(16, 16), cv::Point(112, 112),
                  cv::Scalar(Colors::BLUE), -1);

    cv::rectangle(img, cv::Point(144, 144), cv::Point(240, 240),
                  cv::Scalar(Colors::YELLOW), -1);

    return img;
}

cv::Mat makeAxisTest_OneSided() {
    // Green only (no magenta) - like DSC00202
    cv::Mat img(256, 256, CV_8UC3, Colors::WHITE);

    cv::rectangle(img, cv::Point(16, 16), cv::Point(240, 240),
                  cv::Scalar(Colors::GREEN), -1);

    return img;
}

cv::Mat makeFullWheel() {
    // All 8 spokes + white center
    cv::Mat img(256, 256, CV_8UC3, Colors::WHITE);

    int cx = 128, cy = 128;
    int r = 100;

    // Draw 8 color patches around center
    cv::Vec3b colors[] = {
        Colors::RED, Colors::ORANGE, Colors::YELLOW, Colors::GREEN,
        Colors::CYAN, Colors::TEAL, Colors::BLUE, Colors::MAGENTA
    };

    for (int i = 0; i < 8; i++) {
        float angle = i * M_PI / 4;
        int px = cx + r * std::cos(angle);
        int py = cy + r * std::sin(angle);
        cv::circle(img, cv::Point(px, py), 30, cv::Scalar(colors[i]), -1);
    }

    return img;
}

// Simulate "collapsed" version (desaturated toward grey)
cv::Mat collapse(const cv::Mat& img, float amount) {
    cv::Mat result = img.clone();

    for (int y = 0; y < result.rows; y++) {
        for (int x = 0; x < result.cols; x++) {
            cv::Vec3b& bgr = result.at<cv::Vec3b>(y, x);

            // Desaturate toward grey
            float grey = (bgr[0] + bgr[1] + bgr[2]) / 3.0f;
            bgr[0] = bgr[0] * (1 - amount) + grey * amount;
            bgr[1] = bgr[1] * (1 - amount) + grey * amount;
            bgr[2] = bgr[2] * (1 - amount) + grey * amount;
        }
    }

    return result;
}

// ============================================================
// Main Test
// ============================================================

int main(int argc, char** argv) {
    std::cout << "=== Axis Contrast Preservation Test ===" << std::endl;
    std::cout << std::endl;

    std::string outDir = "tmp/var/axis/";
    system(("mkdir -p " + outDir).c_str());

    // Test 1: R-C axis
    {
        std::cout << "--- Test 1: Red + Cyan ---" << std::endl;
        cv::Mat img = makeAxisTest_RC();
        cv::Mat collapsed = collapse(img, 0.5f);

        AxisContrast orig = measureAxisContrast(img);
        AxisContrast coll = measureAxisContrast(collapsed);

        orig.print("Original");
        coll.print("Collapsed 50%");

        float loss = axisContrastLoss(orig, coll);
        std::cout << "Loss: " << loss << std::endl;
        std::cout << std::endl;

        cv::imwrite(outDir + "test1_rc_orig.png", img);
        cv::imwrite(outDir + "test1_rc_collapsed.png", collapsed);
    }

    // Test 2: G-M axis
    {
        std::cout << "--- Test 2: Green + Magenta ---" << std::endl;
        cv::Mat img = makeAxisTest_GM();
        cv::Mat collapsed = collapse(img, 0.5f);

        AxisContrast orig = measureAxisContrast(img);
        AxisContrast coll = measureAxisContrast(collapsed);

        orig.print("Original");
        coll.print("Collapsed 50%");

        float loss = axisContrastLoss(orig, coll);
        std::cout << "Loss: " << loss << std::endl;
        std::cout << std::endl;

        cv::imwrite(outDir + "test2_gm_orig.png", img);
        cv::imwrite(outDir + "test2_gm_collapsed.png", collapsed);
    }

    // Test 3: B-Y axis
    {
        std::cout << "--- Test 3: Blue + Yellow ---" << std::endl;
        cv::Mat img = makeAxisTest_BY();
        cv::Mat collapsed = collapse(img, 0.5f);

        AxisContrast orig = measureAxisContrast(img);
        AxisContrast coll = measureAxisContrast(collapsed);

        orig.print("Original");
        coll.print("Collapsed 50%");

        float loss = axisContrastLoss(orig, coll);
        std::cout << "Loss: " << loss << std::endl;
        std::cout << std::endl;

        cv::imwrite(outDir + "test3_by_orig.png", img);
        cv::imwrite(outDir + "test3_by_collapsed.png", collapsed);
    }

    // Test 4: One-sided (should have low loss when collapsed)
    {
        std::cout << "--- Test 4: One-sided (Green only) ---" << std::endl;
        cv::Mat img = makeAxisTest_OneSided();
        cv::Mat collapsed = collapse(img, 0.5f);

        AxisContrast orig = measureAxisContrast(img);
        AxisContrast coll = measureAxisContrast(collapsed);

        orig.print("Original");
        coll.print("Collapsed 50%");

        float loss = axisContrastLoss(orig, coll);
        std::cout << "Loss: " << loss << " (expected: low, no opponent to preserve)" << std::endl;
        std::cout << std::endl;

        cv::imwrite(outDir + "test4_onesided_orig.png", img);
        cv::imwrite(outDir + "test4_onesided_collapsed.png", collapsed);
    }

    // Test 5: Full wheel
    {
        std::cout << "--- Test 5: Full Color Wheel ---" << std::endl;
        cv::Mat img = makeFullWheel();
        cv::Mat collapsed = collapse(img, 0.5f);

        AxisContrast orig = measureAxisContrast(img);
        AxisContrast coll = measureAxisContrast(collapsed);

        orig.print("Original");
        coll.print("Collapsed 50%");

        float loss = axisContrastLoss(orig, coll);
        std::cout << "Loss: " << loss << " (expected: high, all axes present)" << std::endl;
        std::cout << std::endl;

        cv::imwrite(outDir + "test5_wheel_orig.png", img);
        cv::imwrite(outDir + "test5_wheel_collapsed.png", collapsed);
    }

    std::cout << "=== Output: " << outDir << " ===" << std::endl;

    return 0;
}
