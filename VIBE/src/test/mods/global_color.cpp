// global_color.cpp - VIBE Test
#include "../diff.h"

bool test_global_color()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("global_color");
    cv::UMat vibe_out, labs_out;

    vibe::mods::global_color(input, vibe_out, 0.6f, 0.55f, 0.5f);
    pipe::mods::global_color(input, labs_out, 0.6f, 0.55f, 0.5f);

    bool ok = print_gold("global_color", compare_clamped(vibe_out, gold));
    ok &= print_cv("global_color", compare(vibe_out, labs_out));
    return ok;
}
