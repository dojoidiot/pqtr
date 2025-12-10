// split_tone.cpp - VIBE Test
#include "../diff.h"

bool test_split_tone()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("split_tone");
    cv::UMat vibe_out, labs_out;

    vibe::mods::split_tone(input, vibe_out, 0.4f, 0.5f, 0.6f, 0.5f);
    pipe::mods::split_tone(input, labs_out, 0.4f, 0.5f, 0.6f, 0.5f);

    bool ok = print_gold("split_tone", compare_clamped(vibe_out, gold));
    ok &= print_cv("split_tone", compare(vibe_out, labs_out));
    return ok;
}
