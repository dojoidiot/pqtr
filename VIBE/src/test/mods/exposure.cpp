// exposure.cpp - VIBE Test
#include "../diff.h"

bool test_exposure()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("exposure");
    cv::UMat vibe_out, labs_out;

    vibe::mods::exposure(input, vibe_out, 0.7f);
    pipe::mods::exposure(input, labs_out, 0.7f);

    bool ok = print_gold("exposure", compare_clamped(vibe_out, gold));
    ok &= print_cv("exposure", compare(vibe_out, labs_out));
    return ok;
}
