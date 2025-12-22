// flow test - HEAD pipeline test
//
// Pipeline:
//   1. HEAD - GPU RAW decode (BLC, WB, demosaic, CST) -> scene-linear RGB
//
// Usage: ./flow [input.ARW]

#include "flow.hpp"

#include <fstream>
#include <iostream>
#include <cstdio>
#include <vector>

namespace {

static std::vector<uint8_t> read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);
    return data;
}

static std::string get_stem(const std::string &path)
{
    size_t slash = path.rfind('/');
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    std::string filename = path.substr(start);
    size_t dot = filename.rfind('.');
    if (dot != std::string::npos)
        filename = filename.substr(0, dot);
    return filename;
}

// Save float RGB as PNG
static void save_png(const std::string &path, const float *rgb, int width, int height)
{
    auto png = flow::swap(rgb, width, height, flow::LIN, flow::PNG);
    if (!png.empty())
    {
        FILE *fp = fopen(path.c_str(), "wb");
        if (fp)
        {
            fwrite(png.data(), 1, png.size(), fp);
            fclose(fp);
            std::cout << "  " << path << " (" << png.size() / 1024 << " KB)" << std::endl;
        }
    }
}

} // namespace


int main(int argc, char **argv)
{
    const char *input = "src/test/flow/DSC00144.ARW";
    if (argc > 1)
        input = argv[1];

    std::cout << "=== FLOW Pipeline Test (HEAD) ===" << std::endl;
    std::cout << "Loading: " << input << std::endl;

    auto raw = read_file(input);
    if (raw.empty())
    {
        std::cerr << "Failed to read: " << input << std::endl;
        return 1;
    }

    std::cout << "Size: " << raw.size() / 1024 << " KB" << std::endl;

    std::string name = get_stem(input);
    auto bits = reinterpret_cast<uint16_t *>(raw.data());
    auto f = flow::make(name, bits, raw.size());

    auto &root = f->info().root();
    int w = static_cast<int>(root.leaf(flow::WIDTH).dial());
    int h = static_cast<int>(root.leaf(flow::HEIGHT).dial());

    std::cout << "Dimensions: " << w << "x" << h << std::endl;

    // Save reference view (camera JPEG, orientation-corrected)
    if (f->view() && f->viewSize() > 0)
    {
        // Get view dimensions and orientation
        int vw = 0, vh = 0, orientation = 1;
        if (root.test("view"))
        {
            auto &view = root.next("view");
            vw = static_cast<int>(view.leaf(flow::WIDTH).dial());
            vh = static_cast<int>(view.leaf(flow::HEIGHT).dial());
        }
        if (root.test("exif"))
        {
            auto &exif = root.next("exif");
            if (exif.test("orientation"))
                orientation = static_cast<int>(exif.leaf("orientation").dial());
        }
        std::cout << "Reference: " << vw << "x" << vh << " (orientation " << orientation << ")" << std::endl;

        // Decode JPEG to RGB, apply orientation, save as PNG
        auto rgb8 = flow::swap(f->view(), f->viewSize(), 0, flow::JPG, flow::BIN);
        if (!rgb8.empty() && vw > 0 && vh > 0)
        {
            // Apply orientation (simplified - just handle 6 and 8 for 90° rotations)
            std::vector<uint8_t> rotated;
            int outW = vw, outH = vh;

            if (orientation == 6)  // 90° CW
            {
                outW = vh; outH = vw;
                rotated.resize(outW * outH * 3);
                for (int y = 0; y < vh; ++y)
                {
                    for (int x = 0; x < vw; ++x)
                    {
                        int srcIdx = (y * vw + x) * 3;
                        int dstIdx = (x * outW + (outW - 1 - y)) * 3;
                        rotated[dstIdx + 0] = rgb8[srcIdx + 0];
                        rotated[dstIdx + 1] = rgb8[srcIdx + 1];
                        rotated[dstIdx + 2] = rgb8[srcIdx + 2];
                    }
                }
            }
            else if (orientation == 8)  // 90° CCW
            {
                outW = vh; outH = vw;
                rotated.resize(outW * outH * 3);
                for (int y = 0; y < vh; ++y)
                {
                    for (int x = 0; x < vw; ++x)
                    {
                        int srcIdx = (y * vw + x) * 3;
                        int dstIdx = ((outH - 1 - x) * outW + y) * 3;
                        rotated[dstIdx + 0] = rgb8[srcIdx + 0];
                        rotated[dstIdx + 1] = rgb8[srcIdx + 1];
                        rotated[dstIdx + 2] = rgb8[srcIdx + 2];
                    }
                }
            }
            else
            {
                rotated = std::move(rgb8);
            }

            // Save as PNG
            auto png = flow::swap(rotated.data(), outW, outH, flow::BIN, flow::PNG);
            std::string viewpath = "tmp/var/flow/" + name + ".0.view.png";
            FILE *fp = fopen(viewpath.c_str(), "wb");
            if (fp)
            {
                fwrite(png.data(), 1, png.size(), fp);
                fclose(fp);
                std::cout << "  " << viewpath << " (" << outW << "x" << outH << ")" << std::endl;
            }
        }
    }

    // =========================================================================
    // COPY - Load XMP sidecar if present
    // =========================================================================

    std::string xmppath = std::string(input) + ".xmp";
    auto xmp = read_file(xmppath.c_str());
    if (!xmp.empty())
    {
        std::cout << "\n=== COPY Stage ===" << std::endl;
        std::cout << "Loading: " << xmppath << " (" << xmp.size() << " bytes)" << std::endl;

        if (flow::copy(reinterpret_cast<const char *>(xmp.data()), xmp.size(), f->info()))
        {
            auto &vibe = f->info().root().next("vibe");
            int count = static_cast<int>(vibe.leaf("_modules").dial());
            std::cout << "Mapped " << count << " darktable modules to vibe format:" << std::endl;

            // Display vibe.linear structure
            if (vibe.test("linear"))
            {
                auto &linear = vibe.next("linear");

                // ColorCorrection
                if (linear.test("colorCorrection"))
                {
                    auto &cc = linear.next("colorCorrection");
                    if (cc.test("exposure"))
                        std::cout << "  colorCorrection.exposure: " << cc.leaf("exposure").dial() << " EV" << std::endl;
                    if (cc.test("whiteBalance"))
                    {
                        auto &wb = cc.next("whiteBalance");
                        std::cout << "  colorCorrection.whiteBalance: " << wb.leaf("temperature").dial() << "K" << std::endl;
                    }
                }

                // ToneMapping
                if (linear.test("toneMapping"))
                {
                    auto &tm = linear.next("toneMapping");
                    if (tm.test("contrast"))
                        std::cout << "  toneMapping.contrast: " << tm.leaf("contrast").dial() << std::endl;
                    if (tm.test("skew"))
                        std::cout << "  toneMapping.skew: " << tm.leaf("skew").dial() << std::endl;
                    if (tm.test("greyPoint"))
                        std::cout << "  toneMapping.greyPoint: " << tm.leaf("greyPoint").dial() << std::endl;
                    if (tm.test("clippingPoint"))
                    {
                        auto &clip = tm.next("clippingPoint");
                        std::cout << "  toneMapping.clippingPoint: ["
                                  << clip.leaf("black").dial() << ", "
                                  << clip.leaf("white").dial() << "] EV" << std::endl;
                    }
                }

                // GlobalColor
                if (linear.test("globalColor"))
                {
                    auto &gc = linear.next("globalColor");
                    if (gc.test("saturation"))
                        std::cout << "  globalColor.saturation: " << gc.leaf("saturation").dial() << std::endl;
                    if (gc.test("vibrance"))
                        std::cout << "  globalColor.vibrance: " << gc.leaf("vibrance").dial() << std::endl;
                }

                // Detail
                if (linear.test("detail"))
                {
                    auto &detail = linear.next("detail");
                    if (detail.test("localContrast"))
                    {
                        auto &lc = detail.next("localContrast");
                        std::cout << "  detail.localContrast.amount: " << lc.leaf("amount").dial() << std::endl;
                        std::cout << "  detail.localContrast.radius: " << lc.leaf("radius").dial() << std::endl;
                    }
                }
            }
        }
        else
        {
            std::cout << "Failed to parse XMP" << std::endl;
        }
    }

    // Save metadata (including vibe settings if XMP was loaded)
    std::string jsonpath = "tmp/var/flow/" + name + ".flow.json";
    std::ofstream out(jsonpath);
    out << f->info().json() << std::endl;
    out.close();
    std::cout << "  " << jsonpath << std::endl;

    // =========================================================================
    // HEAD - RAW decode to scene-linear RGB
    // =========================================================================

    std::cout << "\n=== HEAD Stage ===" << std::endl;

    auto task = f->head(nullptr);  // nullptr = use default device

    std::cout << "Dispatching GPU work..." << std::endl;
    task.post();

    std::cout << "Reading back result..." << std::endl;
    auto result = task.done();

    if (result.rgb.empty())
    {
        std::cerr << "HEAD failed - no output" << std::endl;
        return 1;
    }

    std::cout << "Output: " << result.width << "x" << result.height << std::endl;

    // Save HEAD output (scene-linear, before VIBE)
    std::string headpath = "tmp/var/flow/" + name + ".1.head.png";
    save_png(headpath, result.rgb.data(), result.width, result.height);

    // Compute and save diff (HEAD vs reference)
    auto diffResult = task.diff();
    if (!diffResult.rgb.empty())
    {
        std::string diffpath = "tmp/var/flow/" + name + ".1.head.diff.png";
        save_png(diffpath, diffResult.rgb.data(), diffResult.width, diffResult.height);
    }

    // =========================================================================
    // VIBE - Apply creative style settings
    // =========================================================================

    if (root.test("vibe"))
    {
        std::cout << "\n=== VIBE Stage (stepwise) ===" << std::endl;

        auto &vibeNode = root.next("vibe");

        // Stage 1: Exposure only
        flow::Done stage1 = result;  // copy
        if (flow::vibe(stage1, vibeNode, 1))
        {
            std::string path = "tmp/var/flow/" + name + ".2a.exposure.png";
            save_png(path, stage1.rgb.data(), stage1.width, stage1.height);
        }

        // Stage 2: Exposure + Tonemap
        flow::Done stage2 = result;  // copy
        if (flow::vibe(stage2, vibeNode, 2))
        {
            std::string path = "tmp/var/flow/" + name + ".2b.tonemap.png";
            save_png(path, stage2.rgb.data(), stage2.width, stage2.height);
        }

        // Stage 3: Full (Exposure + Tonemap + Color)
        if (flow::vibe(result, vibeNode, 3))
        {
            std::string vibepath = "tmp/var/flow/" + name + ".2c.vibe.png";
            save_png(vibepath, result.rgb.data(), result.width, result.height);

            // Compute diff against reference (both orientation-corrected)
            if (f->view() && f->viewSize() > 0)
            {
                int vw = 0, vh = 0, orientation = 1;
                if (root.test("view"))
                {
                    auto &view = root.next("view");
                    vw = static_cast<int>(view.leaf(flow::WIDTH).dial());
                    vh = static_cast<int>(view.leaf(flow::HEIGHT).dial());
                }
                if (root.test("exif"))
                {
                    auto &exif = root.next("exif");
                    if (exif.test("orientation"))
                        orientation = static_cast<int>(exif.leaf("orientation").dial());
                }

                // Convert reference JPEG to linear RGB
                auto refRgb = flow::swap(f->view(), f->viewSize(), 0, flow::JPG, flow::LIN);
                if (!refRgb.empty() && vw > 0 && vh > 0)
                {
                    float *refPtr = reinterpret_cast<float *>(refRgb.data());

                    // Apply orientation to reference
                    int refW = vw, refH = vh;
                    std::vector<float> refRotated;
                    if (orientation == 6 || orientation == 8)
                    {
                        refW = vh; refH = vw;
                        refRotated.resize(refW * refH * 3);
                        for (int y = 0; y < vh; ++y)
                        {
                            for (int x = 0; x < vw; ++x)
                            {
                                int srcIdx = (y * vw + x) * 3;
                                int dstIdx;
                                if (orientation == 6)  // 90° CW
                                    dstIdx = (x * refW + (refW - 1 - y)) * 3;
                                else  // 90° CCW
                                    dstIdx = ((refH - 1 - x) * refW + y) * 3;
                                refRotated[dstIdx + 0] = refPtr[srcIdx + 0];
                                refRotated[dstIdx + 1] = refPtr[srcIdx + 1];
                                refRotated[dstIdx + 2] = refPtr[srcIdx + 2];
                            }
                        }
                        refPtr = refRotated.data();
                    }

                    // Downsample vibe result to reference size
                    std::vector<float> vibeDown(refW * refH * 3);
                    float xScale = static_cast<float>(result.width) / refW;
                    float yScale = static_cast<float>(result.height) / refH;

                    for (int y = 0; y < refH; ++y)
                    {
                        for (int x = 0; x < refW; ++x)
                        {
                            int sx = static_cast<int>(x * xScale);
                            int sy = static_cast<int>(y * yScale);
                            sx = std::min(sx, result.width - 1);
                            sy = std::min(sy, result.height - 1);

                            int srcIdx = (sy * result.width + sx) * 3;
                            int dstIdx = (y * refW + x) * 3;
                            vibeDown[dstIdx + 0] = result.rgb[srcIdx + 0];
                            vibeDown[dstIdx + 1] = result.rgb[srcIdx + 1];
                            vibeDown[dstIdx + 2] = result.rgb[srcIdx + 2];
                        }
                    }

                    // Compute false-color diff
                    std::vector<float> diff(refW * refH * 3);
                    for (int i = 0; i < refW * refH; ++i)
                    {
                        float vr = vibeDown[i * 3 + 0], vg = vibeDown[i * 3 + 1], vb = vibeDown[i * 3 + 2];
                        float rr = refPtr[i * 3 + 0], rg = refPtr[i * 3 + 1], rb = refPtr[i * 3 + 2];

                        // Luminance diff
                        float vLum = 0.299f * vr + 0.587f * vg + 0.114f * vb;
                        float rLum = 0.299f * rr + 0.587f * rg + 0.114f * rb;
                        float lumDiff = vLum - rLum;

                        // False color: red = vibe brighter, blue = ref brighter
                        float scale = 3.0f;
                        diff[i * 3 + 0] = std::max(0.0f, std::min(1.0f, 0.5f + lumDiff * scale));
                        diff[i * 3 + 1] = std::max(0.0f, std::min(1.0f, 0.5f - std::abs(lumDiff) * scale * 0.5f));
                        diff[i * 3 + 2] = std::max(0.0f, std::min(1.0f, 0.5f - lumDiff * scale));
                    }

                    std::string diffpath = "tmp/var/flow/" + name + ".2.vibe.diff.png";
                    save_png(diffpath, diff.data(), refW, refH);
                }
            }
        }
        else
        {
            std::cerr << "VIBE failed" << std::endl;
        }
    }

    std::cout << "\n=== Done ===" << std::endl;

    return 0;
}
