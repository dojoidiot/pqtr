/*
 * colorbalancergb_test.cpp - Test colorbalancergb module against DT dump
 * Runtime data copied from DT via fprintf (PQTR_COLORBALANCERGB)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "mods/colorbalancergb.c"
}

static bool load_pfm(const char *path, std::vector<float> &data, int &width, int &height)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    char type[3] = {0};
    if (fscanf(f, "%2s", type) != 1 || strcmp(type, "PF") != 0) {
        fclose(f);
        return false;
    }
    fgetc(f);

    if (fscanf(f, "%d %d", &width, &height) != 2) {
        fclose(f);
        return false;
    }
    fgetc(f);

    float scale;
    if (fscanf(f, "%f", &scale) != 1) {
        fclose(f);
        return false;
    }
    fgetc(f);

    size_t npixels = (size_t)width * height;
    std::vector<float> rgb(npixels * 3);
    size_t read = fread(rgb.data(), sizeof(float), npixels * 3, f);
    fclose(f);

    if (read != npixels * 3) return false;

    /* Convert RGB to RGBA with row reversal (PFM is bottom-to-top) */
    data.resize(npixels * 4);
    for (int row = 0; row < height; row++) {
        int src_row = height - 1 - row;  /* flip rows */
        for (int col = 0; col < width; col++) {
            size_t src_idx = (size_t)src_row * width + col;
            size_t dst_idx = (size_t)row * width + col;
            data[dst_idx * 4 + 0] = rgb[src_idx * 3 + 0];
            data[dst_idx * 4 + 1] = rgb[src_idx * 3 + 1];
            data[dst_idx * 4 + 2] = rgb[src_idx * 3 + 2];
            data[dst_idx * 4 + 3] = 0.0f;
        }
    }

    return true;
}

int main()
{
    const char *in_path = "/tmp/dtdump/export/0000_colorbalancergb_cpu_in_C.pfm";
    const char *ref_path = "/tmp/dtdump/export/0001_colorbalancergb_cpu_out_C.pfm";

    std::vector<float> input;
    int width, height;
    if (!load_pfm(in_path, input, width, height)) {
        fprintf(stderr, "Failed to load: %s\n", in_path);
        return 1;
    }
    printf("Input: %dx%d\n", width, height);

    std::vector<float> ref;
    int ref_w, ref_h;
    if (!load_pfm(ref_path, ref, ref_w, ref_h)) {
        fprintf(stderr, "Failed to load: %s\n", ref_path);
        return 1;
    }

    /* ===== ALL VALUES COPIED FROM DT RUNTIME (PQTR_COLORBALANCERGB) ===== */

    ColorBalanceRGBData d;
    memset(&d, 0, sizeof(d));

    /* d struct values - copied exactly from DT */
    d.shadows_weight = 4.000000000f;
    d.highlights_weight = 4.000000000f;
    d.midtones_weight = 8.000000000f;
    d.mask_grey_fulcrum = 0.500000000f;
    d.white_fulcrum = 1.000000000f;
    d.grey_fulcrum = 0.184499994f;
    d.vibrance = 0.199999988f;
    d.saturation_global = 0.199999928f;
    d.chroma_global = 0.000000000f;
    d.contrast = 1.000000000f;
    d.hue_angle = 0.000000000f;
    d.midtones_Y = 1.000000000f;
    d.brilliance_global = 0.000000000f;
    d.saturation_formula = 1;

    d.global[0] = -0.000000119f; d.global[1] = 0.0f; d.global[2] = 0.0f; d.global[3] = 0.0f;
    d.shadows[0] = 0.999999881f; d.shadows[1] = 1.0f; d.shadows[2] = 1.0f; d.shadows[3] = 1.0f;
    d.highlights[0] = 0.999999881f; d.highlights[1] = 1.0f; d.highlights[2] = 1.0f; d.highlights[3] = 1.0f;
    d.midtones[0] = 1.000000119f; d.midtones[1] = 1.0f; d.midtones[2] = 1.0f; d.midtones[3] = 1.0f;

    d.chroma[0] = 0.0f; d.chroma[1] = 0.0f; d.chroma[2] = 0.0f; d.chroma[3] = 0.0f;
    d.saturation[0] = 0.0f; d.saturation[1] = 0.0f; d.saturation[2] = 0.0f; d.saturation[3] = 0.0f;
    d.brilliance[0] = 0.0f; d.brilliance[1] = 0.0f; d.brilliance[2] = 0.0f; d.brilliance[3] = 0.0f;

    /* gamut_LUT - all 512 values copied from DT (LUT_ELEM=512, power of 2 for bitmask) */
    float gamut_LUT[512] = {
        /* [0] */   0.000471161f,0.000467493f,0.000462786f,0.000458272f,0.000453975f,0.000449884f,0.000445992f,0.000442267f,
        /* [8] */   0.000438704f,0.000435323f,0.000432118f,0.000429083f,0.000426193f,0.000423466f,0.000420897f,0.000418463f,
        /* [16] */  0.000416182f,0.000414049f,0.000412060f,0.000410213f,0.000408492f,0.000406909f,0.000405461f,0.000404135f,
        /* [24] */  0.000402941f,0.000401877f,0.000400932f,0.000400109f,0.000399413f,0.000398841f,0.000398393f,0.000398065f,
        /* [32] */  0.000397855f,0.000397768f,0.000397801f,0.000397953f,0.000398227f,0.000398622f,0.000399137f,0.000399775f,
        /* [40] */  0.000400537f,0.000401426f,0.000402433f,0.000403566f,0.000404829f,0.000406223f,0.000407750f,0.000409392f,
        /* [48] */  0.000411189f,0.000413125f,0.000415176f,0.000417394f,0.000419756f,0.000422266f,0.000424924f,0.000427734f,
        /* [56] */  0.000430696f,0.000433814f,0.000437133f,0.000440616f,0.000444266f,0.000448085f,0.000452074f,0.000456295f,
        /* [64] */  0.000460696f,0.000465281f,0.000470120f,0.000475154f,0.000480384f,0.000485894f,0.000491697f,0.000497720f,
        /* [72] */  0.000503966f,0.000510536f,0.000517340f,0.000524496f,0.000532016f,0.000539804f,0.000547859f,0.000556321f,
        /* [80] */  0.000565207f,0.000574399f,0.000584051f,0.000594027f,0.000604503f,0.000615503f,0.000626872f,0.000638811f,
        /* [88] */  0.000651353f,0.000664533f,0.000678388f,0.000692717f,0.000707529f,0.000723364f,0.000740021f,0.000757265f,
        /* [96] */  0.000775412f,0.000794520f,0.000814650f,0.000835503f,0.000857483f,0.000881059f,0.000905525f,0.000931363f,
        /* [104] */ 0.000958651f,0.000987507f,0.001018026f,0.001049780f,0.001083993f,0.001120263f,0.001158771f,0.001199664f,
        /* [112] */ 0.001242391f,0.001288633f,0.001337883f,0.001389497f,0.001445533f,0.001505408f,0.001569508f,0.001639386f,
        /* [120] */ 0.001713141f,0.001792419f,0.001879208f,0.001972855f,0.002074075f,0.002183666f,0.002302541f,0.002431735f,
        /* [128] */ 0.002572431f,0.002728703f,0.002899821f,0.003087858f,0.003298447f,0.003531402f,0.003789911f,0.004077733f,
        /* [136] */ 0.004404949f,0.004772467f,0.005093151f,0.005022079f,0.004902991f,0.004788728f,0.004679102f,0.004575709f,
        /* [144] */ 0.004476490f,0.004381279f,0.004289972f,0.004202412f,0.004119779f,0.004040516f,0.003964487f,0.003891603f,
        /* [152] */ 0.003821734f,0.003755756f,0.003691585f,0.003630102f,0.003572067f,0.003515685f,0.003461721f,0.003410097f,
        /* [160] */ 0.003360069f,0.003312268f,0.003266613f,0.003223007f,0.003181378f,0.003141143f,0.003099290f,0.003049606f,
        /* [168] */ 0.002991822f,0.002927345f,0.002856965f,0.002781476f,0.002702748f,0.002625508f,0.002552167f,0.002482574f,
        /* [176] */ 0.002416520f,0.002353783f,0.002294785f,0.002238671f,0.002184732f,0.002133920f,0.002085546f,0.002039000f,
        /* [184] */ 0.001995101f,0.001953254f,0.001912968f,0.001874562f,0.001838276f,0.001803328f,0.001769976f,0.001738152f,
        /* [192] */ 0.001707764f,0.001678755f,0.001650804f,0.001624121f,0.001598638f,0.001574303f,0.001550864f,0.001528481f,
        /* [200] */ 0.001507110f,0.001486699f,0.001467206f,0.001448440f,0.001430386f,0.001413160f,0.001396727f,0.001381049f,
        /* [208] */ 0.001366097f,0.001351733f,0.001338044f,0.001325006f,0.001312502f,0.001300604f,0.001289294f,0.001278470f,
        /* [216] */ 0.001268199f,0.001258460f,0.001249235f,0.001240508f,0.001232262f,0.001224484f,0.001217110f,0.001210185f,
        /* [224] */ 0.001203739f,0.001197712f,0.001192098f,0.001186888f,0.001182073f,0.001177676f,0.001173660f,0.001170019f,
        /* [232] */ 0.001166752f,0.001163872f,0.001161374f,0.001159234f,0.001157467f,0.001156058f,0.001155016f,0.001154341f,
        /* [240] */ 0.001154023f,0.001154067f,0.001154468f,0.001155235f,0.001156367f,0.001157857f,0.001159717f,0.001161936f,
        /* [248] */ 0.001164532f,0.001167513f,0.001170864f,0.001174590f,0.001178692f,0.001183207f,0.001188114f,0.001193419f,
        /* [256] */ 0.001199132f,0.001205260f,0.001211858f,0.001218897f,0.001226337f,0.001234237f,0.001242610f,0.001251471f,
        /* [264] */ 0.001260836f,0.001270724f,0.001281152f,0.001292059f,0.001303540f,0.001315615f,0.001328213f,0.001341445f,
        /* [272] */ 0.001355338f,0.001369921f,0.001385099f,0.001401018f,0.001417707f,0.001435061f,0.001453242f,0.001472293f,
        /* [280] */ 0.001468495f,0.001360431f,0.001248875f,0.001155281f,0.001074920f,0.001005166f,0.000943989f,0.000889417f,
        /* [288] */ 0.000840583f,0.000796462f,0.000756490f,0.000720495f,0.000687493f,0.000657181f,0.000629536f,0.000603848f,
        /* [296] */ 0.000579944f,0.000558081f,0.000537688f,0.000518644f,0.000500850f,0.000484205f,0.000468630f,0.000453891f,
        /* [304] */ 0.000440084f,0.000427141f,0.000415003f,0.000403616f,0.000392805f,0.000382537f,0.000372784f,0.000363516f,
        /* [312] */ 0.000354813f,0.000346539f,0.000338670f,0.000331279f,0.000324157f,0.000317380f,0.000311018f,0.000304881f,
        /* [320] */ 0.000299042f,0.000293488f,0.000288202f,0.000283175f,0.000278324f,0.000273711f,0.000269326f,0.000265096f,
        /* [328] */ 0.000261076f,0.000257259f,0.000253578f,0.000250084f,0.000246769f,0.000243576f,0.000240502f,0.000237590f,
        /* [336] */ 0.000234833f,0.000232182f,0.000229636f,0.000227231f,0.000224924f,0.000222712f,0.000220630f,0.000218639f,
        /* [344] */ 0.000216767f,0.000214981f,0.000213279f,0.000211660f,0.000210123f,0.000208691f,0.000207336f,0.000206079f,
        /* [352] */ 0.000204896f,0.000203766f,0.000202728f,0.000201761f,0.000200865f,0.000200052f,0.000199294f,0.000198604f,
        /* [360] */ 0.000197993f,0.000197448f,0.000196960f,0.000196538f,0.000196188f,0.000195903f,0.000195681f,0.000195523f,
        /* [368] */ 0.000195426f,0.000195394f,0.000195426f,0.000195521f,0.000195680f,0.000195904f,0.000196193f,0.000196544f,
        /* [376] */ 0.000196961f,0.000197440f,0.000197984f,0.000198603f,0.000199288f,0.000200039f,0.000200856f,0.000201756f,
        /* [384] */ 0.000202724f,0.000203762f,0.000204868f,0.000206043f,0.000207311f,0.000208673f,0.000210110f,0.000211623f,
        /* [392] */ 0.000213238f,0.000214932f,0.000216706f,0.000218559f,0.000220525f,0.000222608f,0.000224777f,0.000227071f,
        /* [400] */ 0.000229494f,0.000232012f,0.000234626f,0.000237380f,0.000240279f,0.000243284f,0.000246442f,0.000249761f,
        /* [408] */ 0.000253247f,0.000256906f,0.000260693f,0.000264609f,0.000268772f,0.000273135f,0.000277647f,0.000282436f,
        /* [416] */ 0.000287452f,0.000292638f,0.000298068f,0.000303825f,0.000309852f,0.000316141f,0.000322493f,0.000328788f,
        /* [424] */ 0.000335081f,0.000341282f,0.000347371f,0.000353332f,0.000359223f,0.000365293f,0.000371601f,0.000378226f,
        /* [432] */ 0.000385038f,0.000392117f,0.000399551f,0.000407277f,0.000415309f,0.000423660f,0.000432343f,0.000441374f,
        /* [440] */ 0.000450860f,0.000460731f,0.000471007f,0.000481705f,0.000492849f,0.000504566f,0.000516781f,0.000529520f,
        /* [448] */ 0.000542811f,0.000556684f,0.000571172f,0.000586310f,0.000602273f,0.000618831f,0.000636165f,0.000654471f,
        /* [456] */ 0.000673504f,0.000693468f,0.000714422f,0.000736437f,0.000759583f,0.000783746f,0.000808986f,0.000835586f,
        /* [464] */ 0.000863645f,0.000893041f,0.000923863f,0.000956208f,0.000990456f,0.001026471f,0.001055767f,0.001032279f,
        /* [472] */ 0.001001055f,0.000971636f,0.000943697f,0.000917144f,0.000892041f,0.000868136f,0.000845486f,0.000824006f,
        /* [480] */ 0.000803483f,0.000783981f,0.000765426f,0.000747757f,0.000730910f,0.000714835f,0.000699479f,0.000684798f,
        /* [488] */ 0.000670751f,0.000657375f,0.000644624f,0.000632387f,0.000620701f,0.000609532f,0.000598785f,0.000588556f,
        /* [496] */ 0.000578755f,0.000569356f,0.000560337f,0.000551677f,0.000543404f,0.000535496f,0.000527933f,0.000520651f,
        /* [504] */ 0.000513680f,0.000507003f,0.000500605f,0.000494473f,0.000488594f,0.000482958f,0.000477586f,0.000473707f
    };
    memcpy(d.gamut_LUT, gamut_LUT, sizeof(gamut_LUT));

    /* input_matrix - copied from DT (pre-computed) */
    dt_colormatrix_t input_matrix = {
        { 0.406808585f, 0.617819786f, 0.045817737f, 0.f },
        { 0.067756824f, 0.748962402f, 0.100109622f, 0.f },
        { 0.022140553f, -0.015321352f, 0.587274075f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    /* output_matrix - copied from DT (pre-computed) */
    dt_colormatrix_t output_matrix = {
        { 1.662934422f, -0.321330518f, -0.237917423f, 0.f },
        { -0.681079328f, 1.609099507f, 0.035052136f, 0.f },
        { 0.029973516f, -0.075743161f, 0.961853564f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    /* Process */
    std::vector<float> output(input.size());
    colorbalancergb_process(input.data(), output.data(), width, height,
                            input_matrix, output_matrix, &d);

    /* Compare RGB only - 1e-2 for complex module (gamut mapping edge cases) */
    const float tolerance = 1e-2f;
    int mismatches = 0;
    float max_diff = 0.0f;
    size_t max_diff_pixel = 0;
    int max_diff_ch = 0;
    size_t npixels = (size_t)width * height;

    for (size_t i = 0; i < npixels; i++) {
        for (int ch = 0; ch < 3; ch++) {
            size_t idx = i * 4 + ch;
            float diff = fabsf(output[idx] - ref[idx]);
            if (diff > max_diff) {
                max_diff = diff;
                max_diff_pixel = i;
                max_diff_ch = ch;
            }
            if (diff > tolerance) {
                if (mismatches < 5) {
                    printf("Mismatch [%zu ch%d]: got %f, expected %f (diff %e)\n",
                           i, ch, output[idx], ref[idx], diff);
                }
                mismatches++;
            }
        }
    }

    printf("\nMax diff at pixel %zu ch%d: got %f, expected %f\n",
           max_diff_pixel, max_diff_ch, output[max_diff_pixel*4+max_diff_ch], ref[max_diff_pixel*4+max_diff_ch]);
    printf("  input RGB: %f %f %f\n", input[max_diff_pixel*4], input[max_diff_pixel*4+1], input[max_diff_pixel*4+2]);
    printf("  output RGB: %f %f %f\n", output[max_diff_pixel*4], output[max_diff_pixel*4+1], output[max_diff_pixel*4+2]);
    printf("  ref RGB: %f %f %f\n", ref[max_diff_pixel*4], ref[max_diff_pixel*4+1], ref[max_diff_pixel*4+2]);
    /* Count by magnitude */
    int count_1e1 = 0, count_1e2 = 0, count_1e3 = 0, count_1e4 = 0;
    for (size_t i = 0; i < npixels; i++) {
        for (int ch = 0; ch < 3; ch++) {
            float diff = fabsf(output[i*4+ch] - ref[i*4+ch]);
            if (diff > 0.1f) count_1e1++;
            else if (diff > 0.01f) count_1e2++;
            else if (diff > 0.001f) count_1e3++;
            else if (diff > 1e-4f) count_1e4++;
        }
    }
    printf("Diff distribution: >0.1: %d, >0.01: %d, >0.001: %d, >1e-4: %d\n",
           count_1e1, count_1e2, count_1e3, count_1e4);
    printf("Total RGB: %zu, Mismatches: %d, Max diff: %e\n",
           npixels * 3, mismatches, max_diff);

    if (mismatches == 0) {
        printf("PASS\n");
        return 0;
    }
    return 1;
}
