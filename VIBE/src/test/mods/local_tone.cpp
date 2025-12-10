// local_tone.cpp - VIBE Test
#include "../diff.h"

bool test_local_tone()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("local_tone");
    cv::UMat vibe_out, labs_out;

    vibe::mods::local_tone(input, vibe_out, 0.5f, 0.02f, 0.1f);
    pipe::mods::local_tone(input, labs_out, 0.5f, 0.02f, 0.1f);

    bool ok = print_gold("local_tone", compare_clamped(vibe_out, gold));
    ok &= print_cv("local_tone", compare(vibe_out, labs_out));
    return ok;
}
