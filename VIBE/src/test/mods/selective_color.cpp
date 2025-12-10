// selective_color.cpp - VIBE Test
#include "../diff.h"

bool test_selective_color()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("selective_color");
    cv::UMat vibe_out, labs_out;

    float hue[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float sat[8] = {0.6f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float lum[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    vibe::mods::selective_color(input, vibe_out, hue, sat, lum);
    pipe::mods::selective_color(input, labs_out, hue, sat, lum);

    bool ok = print_gold("selective_color", compare_clamped(vibe_out, gold));
    ok &= print_cv("selective_color", compare(vibe_out, labs_out));
    return ok;
}
