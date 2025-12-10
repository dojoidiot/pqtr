// detail.cpp - VIBE Test
#include "../diff.h"

bool test_detail()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("detail");
    cv::UMat vibe_out, labs_out;

    vibe::mods::detail(input, vibe_out, 0.6f, 0.4f, 0.3f, 0.5f);
    pipe::mods::detail(input, labs_out, 0.6f, 0.4f, 0.3f, 0.5f);

    bool ok = print_gold("detail", compare_clamped(vibe_out, gold));
    ok &= print_cv("detail", compare(vibe_out, labs_out));
    return ok;
}
