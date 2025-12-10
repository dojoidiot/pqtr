// white_balance.cpp - VIBE Test
#include "../diff.h"

bool test_white_balance()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("white_balance");
    cv::UMat vibe_out, labs_out;

    vibe::mods::white_balance(input, vibe_out, 0.6f, 0.4f);
    pipe::mods::white_balance(input, labs_out, 0.6f, 0.4f);

    bool ok = print_gold("white_balance", compare_clamped(vibe_out, gold));
    ok &= print_cv("white_balance", compare(vibe_out, labs_out));
    return ok;
}
