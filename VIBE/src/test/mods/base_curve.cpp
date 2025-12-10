// base_curve.cpp - VIBE Test
#include "../diff.h"
#include <cmath>

bool test_base_curve()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("base_curve");
    cv::UMat vibe_out, labs_out;

    float curve[768];  // 256 per channel (B, G, R)
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = std::pow(float(i) / 255.0f, 1.1f);

    vibe::mods::base_curve(input, vibe_out, curve);
    pipe::mods::base_curve(input, labs_out, curve);

    bool ok = print_gold("base_curve", compare_clamped(vibe_out, gold));
    ok &= print_cv("base_curve", compare(vibe_out, labs_out));
    return ok;
}
