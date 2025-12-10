// tone_map.cpp - VIBE Test
#include "../diff.h"

bool test_tone_map()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("tone_map");
    cv::UMat vibe_out, labs_out;

    vibe::mods::tone_map(input, vibe_out, 0.6f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
    pipe::mods::tone_map(input, labs_out, 0.6f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);

    bool ok = print_gold("tone_map", compare_clamped(vibe_out, gold));
    ok &= print_cv("tone_map", compare(vibe_out, labs_out));
    return ok;
}
