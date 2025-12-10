// test.cpp - VIBE
// Main test runner for VIBE module comparison tests

#include <iostream>
#include <iomanip>
#include <cmath>
#include <opencv2/core.hpp>

#include "diff.h"

int main()
{
    std::cout << "VIBE Mod Comparison Test\n";
    std::cout << "========================\n\n";

    int passed = 0, failed = 0;

    // Dial mods
    if (test_exposure()) passed++; else failed++;
    if (test_white_balance()) passed++; else failed++;
    if (test_tone_map()) passed++; else failed++;
    if (test_global_color()) passed++; else failed++;
    if (test_geometric()) passed++; else failed++;
    if (test_selective_color()) passed++; else failed++;
    if (test_split_tone()) passed++; else failed++;
    if (test_detail()) passed++; else failed++;

    // Meta mods
    if (test_baseline()) passed++; else failed++;
    if (test_sigmoid()) passed++; else failed++;
    if (test_base_curve()) passed++; else failed++;
    if (test_color_matrix()) passed++; else failed++;
    if (test_lut_curve()) passed++; else failed++;
    if (test_lut3d()) passed++; else failed++;
    if (test_hsv_lut()) passed++; else failed++;
    if (test_poly_color()) passed++; else failed++;
    if (test_local_tone()) passed++; else failed++;

    std::cout << "\n";
    std::cout << "Passed: " << passed << "/" << (passed + failed) << "\n";

    return failed > 0 ? 1 : 0;
}
