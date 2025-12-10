// geometric.cpp - VIBE Test
#include "../diff.h"

bool test_geometric()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("geometric");
    cv::UMat vibe_out, labs_out;

    vibe::mods::geometric(input, vibe_out, 0.1f, 0.1f, 0.1f, 0.1f, 0.2f, 0.5f);
    pipe::mods::geometric(input, labs_out, 0.1f, 0.1f, 0.1f, 0.1f, 0.2f, 0.5f);

    bool ok = print_gold("geometric", compare_clamped(vibe_out, gold));
    ok &= print_cv("geometric", compare(vibe_out, labs_out));
    return ok;
}
