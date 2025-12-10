// lut3d.cpp - VIBE Test
#include "../diff.h"
#include <vector>

bool test_lut3d()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("lut3d");
    cv::UMat vibe_out, labs_out;

    int gs = 9;
    std::vector<float> lut(gs * gs * gs * 3);
    for (int ri = 0; ri < gs; ri++)
        for (int gi = 0; gi < gs; gi++)
            for (int bi = 0; bi < gs; bi++)
            {
                int idx = ((ri * gs + gi) * gs + bi) * 3;
                lut[idx] = float(ri) / (gs - 1);
                lut[idx + 1] = float(gi) / (gs - 1);
                lut[idx + 2] = float(bi) / (gs - 1);
            }

    vibe::mods::lut3d_apply(input, vibe_out, lut.data(), gs);
    pipe::mods::lut3d_apply(input, labs_out, lut.data(), gs);

    bool ok = print_gold("lut3d", compare_clamped(vibe_out, gold));
    ok &= print_cv("lut3d", compare(vibe_out, labs_out));
    return ok;
}
