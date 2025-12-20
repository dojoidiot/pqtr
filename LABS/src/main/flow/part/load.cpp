// load.cpp - RAW decoder dispatcher
//
// Master RAW loader that dispatches to format-specific decoders (sony.cpp, etc.)

#include "flow.hpp"
#include "pipe.hpp"

#include <sstream>
#include <iomanip>

// Forward declare warp (lens distortion correction)
namespace flow
{
    void warp(uint16_t *data, int w, int h, const float *params, int count);
}

// Forward declare sony decoder
namespace sony
{
    struct BayerBuffer
    {
        std::vector<uint16_t> data;
        int width;
        int height;
        int black_level;
        int white_level;
        std::vector<uint8_t> preview;
        int preview_width;
        int preview_height;
    };

    pipe::Flow decode(const char *raw_data, size_t raw_size);
}

namespace flow
{
    // Factory from tree.cpp
    std::unique_ptr<Tree> makeTree();

    using Text = std::string;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    static Text formatArray(const float *arr, size_t count)
    {
        std::ostringstream oss;
        oss << std::setprecision(6);
        for (size_t i = 0; i < count; ++i)
        {
            if (i > 0)
                oss << ", ";
            oss << arr[i];
        }
        return oss.str();
    }

    // -------------------------------------------------------------------------
    // load - decode RAW file and populate tree
    // -------------------------------------------------------------------------

    struct LoadResult
    {
        std::vector<uint16_t> data;
        std::vector<uint8_t> view;
        std::unique_ptr<Tree> info;
    };

    LoadResult load(const std::string &name, const uint8_t *bits, size_t size)
    {
        LoadResult result;
        result.info = makeTree();

        auto &root = result.info->root();
        Text n = name;
        root.leaf(NAME).text(n);

        // Try Sony decoder
        const char *raw = reinterpret_cast<const char *>(bits);
        pipe::Flow decoded = sony::decode(raw, size);

        auto *buf = static_cast<sony::BayerBuffer *>(decoded.data);
        if (!buf)
            return result;

        // Copy Bayer data
        result.data = std::move(buf->data);
        result.view = std::move(buf->preview);

        // Populate info tree
        root.leaf(WIDTH).dial(static_cast<float>(buf->width));
        root.leaf(HEIGHT).dial(static_cast<float>(buf->height));
        root.leaf(BLACK).dial(static_cast<float>(buf->black_level));
        root.leaf(WHITE).dial(static_cast<float>(buf->white_level));

        auto &preview = root.next("view");
        preview.leaf(WIDTH).dial(static_cast<float>(buf->preview_width));
        preview.leaf(HEIGHT).dial(static_cast<float>(buf->preview_height));

        delete buf;

        // Transfer EXIF
        auto &exif = root.next("exif");
        pipe::Info &pi = decoded.info;

        Text make = pi.text("gear_make");
        Text model = pi.text("gear_model");
        Text lens = pi.text("gear_lens");
        if (!make.empty())
            exif.leaf("make").text(make);
        if (!model.empty())
            exif.leaf("model").text(model);
        if (!lens.empty())
            exif.leaf("lens").text(lens);

        float iso = pi.dial("gear_iso");
        float shutter = pi.dial("gear_shutter");
        float aperture = pi.dial("gear_aperture");
        float focal = pi.dial("gear_focal_length");
        int orientation = static_cast<int>(pi.dial("gear_orientation"));

        if (iso > 0)
            exif.leaf("iso").dial(iso);
        if (shutter > 0)
            exif.leaf("shutter").dial(shutter);
        if (aperture > 0)
            exif.leaf("aperture").dial(aperture);
        if (focal > 0)
            exif.leaf("focal_length").dial(focal);
        if (orientation > 0)
            exif.leaf("orientation").dial(static_cast<float>(orientation));

        // Transfer Maker Notes
        auto &maker = root.next("maker");

        Text style = pi.text("gear_creative_style");
        Text dro = pi.text("gear_dro");
        if (!style.empty())
            maker.leaf("creative_style").text(style);
        if (!dro.empty())
            maker.leaf("dro").text(dro);

        maker.leaf("contrast").dial(pi.dial("gear_contrast"));
        maker.leaf("saturation").dial(pi.dial("gear_saturation"));
        maker.leaf("sharpness").dial(pi.dial("gear_sharpness"));

        auto &wb = maker.next("white_balance");
        wb.leaf("r").dial(pi.dial("gear_wb_r"));
        wb.leaf("g").dial(pi.dial("gear_wb_g"));
        wb.leaf("b").dial(pi.dial("gear_wb_b"));

        const float *matrix = pi.data("gear_color_matrix");
        size_t matrixSize = pi.size("gear_color_matrix");
        if (matrix && matrixSize == 9)
        {
            Text matrixStr = formatArray(matrix, matrixSize);
            maker.leaf("color_matrix").text(matrixStr);
        }

        int bayer = static_cast<int>(pi.dial("gear_bayer_pattern"));
        if (bayer > 0)
            maker.leaf("bayer_pattern").dial(static_cast<float>(bayer));

        auto &crop = maker.next("crop");
        crop.leaf("left").dial(pi.dial("gear_crop_left"));
        crop.leaf("top").dial(pi.dial("gear_crop_top"));
        crop.leaf("width").dial(pi.dial("gear_crop_width"));
        crop.leaf("height").dial(pi.dial("gear_crop_height"));

        const float *distortion = pi.data("gear_distortion");
        size_t distCount = pi.size("gear_distortion");
        if (distortion && distCount > 0)
        {
            Text distStr = formatArray(distortion, distCount);
            maker.leaf("distortion").text(distStr);
            // Note: distortion correction applied after demosaic, not on Bayer
        }

        Text decoder = pi.text("gear_decoder");
        if (!decoder.empty())
            maker.leaf("decoder").text(decoder);

        return result;
    }

} // namespace flow
