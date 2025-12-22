// test_tune.cpp - Test VIBE parameter optimizer
//
// Loads RAW + reference JPEG, runs optimizer to find optimal VIBE params
//
// Usage: ./test_tune [input.ARW]
//   Looks for embedded JPEG as reference, or external .jpg file with same stem

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

static std::string get_dir(const std::string &path)
{
    size_t slash = path.rfind('/');
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

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

    std::cout << "=== VIBE Parameter Optimizer (TUNE) ===" << std::endl;
    std::cout << "Loading: " << input << std::endl;

    auto raw = read_file(input);
    if (raw.empty())
    {
        std::cerr << "Failed to read: " << input << std::endl;
        return 1;
    }

    std::cout << "Size: " << raw.size() / 1024 << " KB" << std::endl;

    std::string name = get_stem(input);
    std::string dir = get_dir(input);
    auto bits = reinterpret_cast<uint16_t *>(raw.data());
    auto f = flow::make(name, bits, raw.size());

    auto &root = f->info().root();
    int w = static_cast<int>(root.leaf(flow::WIDTH).dial());
    int h = static_cast<int>(root.leaf(flow::HEIGHT).dial());

    std::cout << "Dimensions: " << w << "x" << h << std::endl;

    // Get reference JPEG - prefer embedded, fall back to external file
    const uint8_t* ref_jpg = nullptr;
    size_t ref_jpg_size = 0;
    int ref_width = 0, ref_height = 0, orientation = 1;
    std::vector<uint8_t> external_jpg;

    if (f->view() && f->viewSize() > 0)
    {
        // Use embedded JPEG
        ref_jpg = f->view();
        ref_jpg_size = f->viewSize();

        if (root.test("view"))
        {
            auto &view = root.next("view");
            ref_width = static_cast<int>(view.leaf(flow::WIDTH).dial());
            ref_height = static_cast<int>(view.leaf(flow::HEIGHT).dial());
        }
        if (root.test("exif"))
        {
            auto &exif = root.next("exif");
            if (exif.test("orientation"))
                orientation = static_cast<int>(exif.leaf("orientation").dial());
        }

        std::cout << "Reference: embedded JPEG " << ref_width << "x" << ref_height
                  << " (orientation " << orientation << ")" << std::endl;
    }
    else
    {
        // Try external JPEG
        std::string jpg_path = dir + "/" + name + ".jpg";
        external_jpg = read_file(jpg_path.c_str());
        if (external_jpg.empty())
        {
            jpg_path = dir + "/" + name + ".JPG";
            external_jpg = read_file(jpg_path.c_str());
        }

        if (!external_jpg.empty())
        {
            ref_jpg = external_jpg.data();
            ref_jpg_size = external_jpg.size();

            // Decode to get dimensions
            auto decoded = flow::swap(ref_jpg, ref_jpg_size, 0, flow::JPG, flow::BIN);
            // Parse JPEG header for dimensions (simplified - assume 6000x4000 for Sony A7)
            ref_width = 1616;  // Preview size
            ref_height = 1080;

            std::cout << "Reference: external JPEG " << jpg_path << std::endl;
        }
        else
        {
            std::cerr << "No reference JPEG found" << std::endl;
            return 1;
        }
    }

    // =========================================================================
    // HEAD - RAW decode to scene-linear RGB
    // =========================================================================

    std::cout << "\n=== HEAD Stage ===" << std::endl;

    auto task = f->head(nullptr);
    task.post();
    auto result = task.done();

    if (result.rgb.empty())
    {
        std::cerr << "HEAD failed - no output" << std::endl;
        return 1;
    }

    std::cout << "Output: " << result.width << "x" << result.height << std::endl;

    // Save HEAD output for comparison
    std::string headpath = "tmp/var/flow/" + name + ".tune.head.png";
    save_png(headpath, result.rgb.data(), result.width, result.height);

    // =========================================================================
    // TUNE - Learn optimal VIBE parameters
    // =========================================================================

    std::cout << "\n=== TUNE Stage ===" << std::endl;

    auto &vibeNode = root.next("vibe");

    if (!flow::tune(result, ref_jpg, ref_jpg_size, ref_width, ref_height, orientation,
                    vibeNode, 200))
    {
        std::cerr << "TUNE failed" << std::endl;
        return 1;
    }

    // Display learned parameters
    std::cout << "\nLearned parameters:" << std::endl;
    if (vibeNode.test("linear"))
    {
        auto &linear = vibeNode.next("linear");

        if (linear.test("colorCorrection"))
        {
            auto &cc = linear.next("colorCorrection");
            if (cc.test("whiteBalance"))
            {
                auto &wb = cc.next("whiteBalance");
                if (wb.test("temperature"))
                    std::cout << "  colorCorrection.whiteBalance.temperature: " << wb.leaf("temperature").dial() << " K" << std::endl;
                if (wb.test("tint"))
                    std::cout << "  colorCorrection.whiteBalance.tint: " << wb.leaf("tint").dial() << std::endl;
            }
            if (cc.test("exposure"))
                std::cout << "  colorCorrection.exposure: " << cc.leaf("exposure").dial() << " EV" << std::endl;
        }

        if (linear.test("toneMapping"))
        {
            auto &tm = linear.next("toneMapping");
            if (tm.test("contrast"))
                std::cout << "  toneMapping.contrast: " << tm.leaf("contrast").dial() << std::endl;
            if (tm.test("skew"))
                std::cout << "  toneMapping.skew: " << tm.leaf("skew").dial() << std::endl;
        }

        if (linear.test("globalColor"))
        {
            auto &gc = linear.next("globalColor");
            if (gc.test("saturation"))
                std::cout << "  globalColor.saturation: " << gc.leaf("saturation").dial() << std::endl;
            if (gc.test("vibrance"))
                std::cout << "  globalColor.vibrance: " << gc.leaf("vibrance").dial() << std::endl;
        }

        if (linear.test("baseCurve"))
        {
            auto &bc = linear.next("baseCurve");
            std::cout << "  baseCurve.R: y1=" << bc.leaf("r_y1").dial()
                      << " y2=" << bc.leaf("r_y2").dial() << std::endl;
            std::cout << "  baseCurve.G: y1=" << bc.leaf("g_y1").dial()
                      << " y2=" << bc.leaf("g_y2").dial() << std::endl;
            std::cout << "  baseCurve.B: y1=" << bc.leaf("b_y1").dial()
                      << " y2=" << bc.leaf("b_y2").dial() << std::endl;
        }

        if (linear.test("splitTone"))
        {
            auto &st = linear.next("splitTone");
            std::cout << "  splitTone.shadow: hue=" << st.leaf("shadow_hue").dial()
                      << " sat=" << st.leaf("shadow_sat").dial() << std::endl;
            std::cout << "  splitTone.highlight: hue=" << st.leaf("highlight_hue").dial()
                      << " sat=" << st.leaf("highlight_sat").dial() << std::endl;
        }
    }

    // =========================================================================
    // VIBE - Apply learned parameters
    // =========================================================================

    std::cout << "\n=== VIBE Stage (with learned params) ===" << std::endl;

    // Re-run HEAD to get fresh scene-linear data (tune used the original result)
    auto task2 = f->head(nullptr);
    task2.post();
    auto vibeResult = task2.done();

    if (flow::vibe(vibeResult, vibeNode, 3))
    {
        std::string vibepath = "tmp/var/flow/" + name + ".tune.vibe.png";
        save_png(vibepath, vibeResult.rgb.data(), vibeResult.width, vibeResult.height);
    }
    else
    {
        std::cerr << "VIBE failed" << std::endl;
    }

    // Save reference for comparison
    if (ref_jpg && ref_jpg_size > 0)
    {
        auto ref_png = flow::swap(ref_jpg, ref_jpg_size, 0, flow::JPG, flow::PNG);
        if (!ref_png.empty())
        {
            std::string refpath = "tmp/var/flow/" + name + ".tune.ref.png";
            FILE *fp = fopen(refpath.c_str(), "wb");
            if (fp)
            {
                fwrite(ref_png.data(), 1, ref_png.size(), fp);
                fclose(fp);
                std::cout << "  " << refpath << std::endl;
            }
        }
    }

    // Save learned profile as JSON
    std::string jsonpath = "tmp/var/flow/" + name + ".tune.json";
    std::ofstream out(jsonpath);
    out << f->info().json() << std::endl;
    out.close();
    std::cout << "  " << jsonpath << std::endl;

    std::cout << "\n=== Done ===" << std::endl;

    return 0;
}
