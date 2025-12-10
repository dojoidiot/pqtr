// sigmoid.cpp - VIBE Test
#include "../diff.h"

bool test_sigmoid()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("sigmoid");
    cv::UMat vibe_out, labs_out;

    vibe::mods::sigmoid(input, vibe_out, 1.5f, 0.0f, 1.0f, 0.000152f);
    pipe::mods::sigmoid(input, labs_out, 1.5f, 0.0f, 1.0f, 0.000152f);

    bool ok = print_gold("sigmoid", compare_clamped(vibe_out, gold));
    ok &= print_cv("sigmoid", compare(vibe_out, labs_out));
    return ok;
}
