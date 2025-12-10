// baseline.cpp - VIBE Test
#include "../diff.h"

bool test_baseline()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("baseline");
    cv::UMat vibe_out, labs_out;

    vibe::mods::baseline(input, vibe_out, 0.7f, 0.95f);
    pipe::mods::baseline(input, labs_out, 0.7f, 0.95f);

    bool ok = print_gold("baseline", compare_clamped(vibe_out, gold));
    ok &= print_cv("baseline", compare(vibe_out, labs_out));
    return ok;
}
