// test_tune.cpp - Unit tests for the TUNE plugin

#include "tune.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

// Anonymous namespace for test helpers and internal access
namespace
{
    struct HSV { float h, s, v; };

    // r,g,b inputs are [0,1]
    // h is [0,360], s is [0,1], v is [0,1]
    static HSV rgb_to_hsv(float r, float g, float b) {
        float max_val = std::max(r, std::max(g, b));
        float min_val = std::min(r, std::min(g, b));
        float delta = max_val - min_val;

        HSV hsv;
        hsv.v = max_val;

        if (delta < 1e-6f) {
            hsv.h = 0.0f;
            hsv.s = 0.0f;
        } else {
            hsv.s = delta / max_val;
            if (r >= max_val) {
                hsv.h = 60.0f * fmod(((g - b) / delta), 6.0f);
            } else if (g >= max_val) {
                hsv.h = 60.0f * (((b - r) / delta) + 2.0f);
            } else {
                hsv.h = 60.0f * (((r - g) / delta) + 4.0f);
            }
            if (hsv.h < 0.0f) {
                hsv.h += 360.0f;
            }
        }
        return hsv;
    }

    static void hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b) {
        if (s < 1e-6f) {
            r = g = b = v;
            return;
        }

        float c = v * s;
        float x = c * (1.0f - std::abs(fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;

        if (h >= 0 && h < 60) {
            r = c; g = x; b = 0;
        } else if (h >= 60 && h < 120) {
            r = x; g = c; b = 0;
        } else if (h >= 120 && h < 180) {
            r = 0; g = c; b = x;
        } else if (h >= 180 && h < 240) {
            r = 0; g = x; b = c;
        } else if (h >= 240 && h < 300) {
            r = x; g = 0; b = c;
        } else {
            r = c; g = 0; b = x;
        }

        r += m; g += m; b += m;
    }

    // A simple assertion helper for floating point comparison
    void assert_approx(float a, float b, float epsilon = 1e-4f) {
        if (std::abs(a - b) > epsilon) {
            std::cerr << "ASSERT FAILED: " << a << " != " << b << std::endl;
            assert(false);
        }
    }
}

void test_hsv_conversion() {
    std::cout << "Running HSV conversion tests..." << std::endl;
    
    // Test cases for RGB -> HSV -> RGB round trip
    std::vector<std::vector<float>> test_colors = {
        {1.0, 0.0, 0.0}, // Red
        {0.0, 1.0, 0.0}, // Green
        {0.0, 0.0, 1.0}, // Blue
        {1.0, 1.0, 0.0}, // Yellow
        {0.0, 1.0, 1.0}, // Cyan
        {1.0, 0.0, 1.0}, // Magenta
        {1.0, 1.0, 1.0}, // White
        {0.0, 0.0, 0.0}, // Black
        {0.5, 0.5, 0.5}, // Gray
        {0.7, 0.2, 0.3}  // Salmon pink
    };

    for (const auto& color : test_colors) {
        float r = color[0], g = color[1], b = color[2];
        
        // Convert to HSV and back
        HSV hsv = rgb_to_hsv(r, g, b);
        float r_out, g_out, b_out;
        hsv_to_rgb(hsv.h, hsv.s, hsv.v, r_out, g_out, b_out);
        
        // Check if the round trip is accurate
        assert_approx(r, r_out);
        assert_approx(g, g_out);
        assert_approx(b, b_out);
    }
    
    std::cout << "HSV conversion tests passed." << std::endl;
}


int main() {
    test_hsv_conversion();
    
    std::cout << "\nAll TUNE tests passed!" << std::endl;
    return 0;
}

