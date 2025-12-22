// load.cpp - RAW decoder dispatcher
//
// Master RAW loader that dispatches to format-specific decoders (sony.cpp, etc.)

#include "flow.hpp"
#include "sony.h"

#include <sstream>
#include <iomanip>

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
        sony::RawMetadata metadata;  // Keep metadata for GPU processing
    };

    LoadResult load(const std::string &name, const uint8_t *bits, size_t size)
    {
        LoadResult result;
        result.info = makeTree();

        auto &root = result.info->root();
        Text n = name;
        root.leaf(NAME).text(n);

        // Decode using sony::Decoder::prepare()
        sony::BayerU16 bayer;
        sony::Info sonyInfo;

        if (!sony::Decoder::prepare(bits, size, bayer, sonyInfo, result.metadata))
            return result;

        // Copy Bayer data
        result.data = std::move(bayer.data);

        // Copy preview JPEG (original bytes for saving)
        if (!result.metadata.preview_jpeg.empty())
            result.view = std::move(result.metadata.preview_jpeg);

        // Populate info tree from metadata
        root.leaf(WIDTH).dial(static_cast<float>(result.metadata.width));
        root.leaf(HEIGHT).dial(static_cast<float>(result.metadata.height));
        root.leaf(BLACK).dial(static_cast<float>(result.metadata.black_level));
        root.leaf(WHITE).dial(static_cast<float>(result.metadata.white_level));

        auto &preview = root.next("view");
        preview.leaf(WIDTH).dial(static_cast<float>(result.metadata.preview.width));
        preview.leaf(HEIGHT).dial(static_cast<float>(result.metadata.preview.height));

        // Transfer EXIF
        auto &exif = root.next("exif");

        if (!result.metadata.camera_make.empty())
        {
            Text make = result.metadata.camera_make;
            exif.leaf("make").text(make);
        }
        if (!result.metadata.camera_model.empty())
        {
            Text model = result.metadata.camera_model;
            exif.leaf("model").text(model);
        }
        if (!result.metadata.lens_model.empty())
        {
            Text lens = result.metadata.lens_model;
            exif.leaf("lens").text(lens);
        }

        if (result.metadata.iso > 0)
            exif.leaf("iso").dial(result.metadata.iso);
        if (result.metadata.shutter_speed > 0)
            exif.leaf("shutter").dial(result.metadata.shutter_speed);
        if (result.metadata.aperture > 0)
            exif.leaf("aperture").dial(result.metadata.aperture);
        if (result.metadata.focal_length > 0)
            exif.leaf("focal_length").dial(result.metadata.focal_length);
        if (result.metadata.orientation > 0)
            exif.leaf("orientation").dial(static_cast<float>(result.metadata.orientation));

        // Transfer Maker Notes
        auto &maker = root.next("maker");

        if (!result.metadata.creative_style.empty())
        {
            Text style = result.metadata.creative_style;
            maker.leaf("creative_style").text(style);
        }
        if (!result.metadata.dro.empty())
        {
            Text dro = result.metadata.dro;
            maker.leaf("dro").text(dro);
        }

        maker.leaf("contrast").dial(static_cast<float>(result.metadata.contrast));
        maker.leaf("saturation").dial(static_cast<float>(result.metadata.saturation));
        maker.leaf("sharpness").dial(static_cast<float>(result.metadata.sharpness));

        auto &wb = maker.next("white_balance");
        float g_ref = result.metadata.wb_rggb[1] > 0 ? static_cast<float>(result.metadata.wb_rggb[1]) : 1024.0f;
        wb.leaf("r").dial(static_cast<float>(result.metadata.wb_rggb[0]) / g_ref);
        wb.leaf("g").dial(1.0f);
        wb.leaf("b").dial(static_cast<float>(result.metadata.wb_rggb[2]) / g_ref);

        Text matrixStr = formatArray(result.metadata.color_matrix, 9);
        maker.leaf("color_matrix").text(matrixStr);

        maker.leaf("bayer_pattern").dial(static_cast<float>(result.metadata.bayer_pattern));

        auto &crop = maker.next("crop");
        crop.leaf("left").dial(static_cast<float>(result.metadata.crop_left));
        crop.leaf("top").dial(static_cast<float>(result.metadata.crop_top));
        crop.leaf("width").dial(static_cast<float>(result.metadata.crop_width));
        crop.leaf("height").dial(static_cast<float>(result.metadata.crop_height));

        if (result.metadata.has_distortion_params)
        {
            std::ostringstream oss;
            for (int i = 0; i < result.metadata.distortion_knot_count; ++i)
            {
                if (i > 0) oss << ", ";
                oss << result.metadata.distortion_params[i];
            }
            Text distStr = oss.str();
            maker.leaf("distortion").text(distStr);
        }

        return result;
    }

} // namespace flow
