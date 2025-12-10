// hsv_lut.cpp - VIBE Test
#include "../diff.h"
#include <vector>

bool test_hsv_lut()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("hsv_lut");
    cv::UMat vibe_out, labs_out;

    std::vector<float> lut(vibe::mods::HSV_LUT_SIZE, 0.0f);

    vibe::mods::hsv_lut_apply(input, vibe_out, lut.data());
    pipe::mods::hsv_lut_apply(input, labs_out, lut.data());

    bool ok = print_gold("hsv_lut", compare_clamped(vibe_out, gold));
    ok &= print_cv("hsv_lut", compare(vibe_out, labs_out));
    return ok;
}
