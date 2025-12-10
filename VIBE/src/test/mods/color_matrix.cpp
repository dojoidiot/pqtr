// color_matrix.cpp - VIBE Test
#include "../diff.h"

bool test_color_matrix()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("color_matrix");
    cv::UMat vibe_out, labs_out;

    cv::Matx33f m(1.1f, -0.05f, -0.05f,
                  -0.05f, 1.1f, -0.05f,
                  -0.05f, -0.05f, 1.1f);

    vibe::mods::color_matrix(input, vibe_out, m);
    pipe::mods::color_matrix(input, labs_out, m);

    bool ok = print_gold("color_matrix", compare_clamped(vibe_out, gold));
    ok &= print_cv("color_matrix", compare(vibe_out, labs_out));
    return ok;
}
