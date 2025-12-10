// lut_curve.cpp - VIBE Test
#include "../diff.h"
#include <cmath>

bool test_lut_curve()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("lut_curve");
    cv::UMat vibe_out, labs_out;

    float lut[96];
    for (int i = 0; i < 32; i++)
    {
        float v = float(i) / 31.0f;
        lut[i] = std::pow(v, 0.9f);
        lut[32 + i] = v;
        lut[64 + i] = std::pow(v, 1.1f);
    }

    vibe::mods::lut_curve(input, vibe_out, lut, 32);
    pipe::mods::lut_curve(input, labs_out, lut, 32);

    bool ok = print_gold("lut_curve", compare_clamped(vibe_out, gold));
    ok &= print_cv("lut_curve", compare(vibe_out, labs_out));
    return ok;
}
