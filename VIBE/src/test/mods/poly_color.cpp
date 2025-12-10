// poly_color.cpp - VIBE Test
#include "../diff.h"

bool test_poly_color()
{
    cv::UMat input = load_input();
    cv::UMat gold = load_gold("poly_color");
    cv::UMat vibe_out, labs_out;

    float coeffs[30] = {0};
    coeffs[1] = 1.0f;
    coeffs[12] = 1.0f;
    coeffs[23] = 1.0f;

    vibe::mods::poly_color(input, vibe_out, coeffs);
    pipe::mods::poly_color(input, labs_out, coeffs);

    bool ok = print_gold("poly_color", compare_clamped(vibe_out, gold));
    ok &= print_cv("poly_color", compare(vibe_out, labs_out));
    return ok;
}
