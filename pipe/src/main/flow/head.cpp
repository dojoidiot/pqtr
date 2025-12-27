// head.cpp - Head implementation (Sony ARW decoder -> Flow)
//
// Wires sony::Decoder::prepare() to flow::Head/Flow API

#include "../../../inc/pipe.hpp"
#include "sony.h"
#include <memory>

namespace flow
{

// Forward declaration from tree.cpp
std::unique_ptr<Tree> makeTree();

// -------------------------------------------------------------------------
// FlowImpl - wraps decoded bayer data + metadata
// -------------------------------------------------------------------------

class FlowImpl : public Flow
{
    sony::BayerU16 bayer_;
    std::unique_ptr<Tree> info_;
    std::vector<float> fbayer_;  // normalized float bayer (filled by rawprepare)
    std::vector<float> rgb_;     // RGB float data (filled by demosaic)

public:
    FlowImpl(sony::BayerU16&& bayer, std::unique_ptr<Tree> info)
        : bayer_(std::move(bayer))
        , info_(std::move(info))
    {}

    Tree& info() override { return *info_; }
    uint16_t* data() override { return bayer_.ptr(); }

    float* fdata() override {
        // Allocate on first access if needed
        if (fbayer_.empty()) {
            auto& root = info_->root();
            int width = static_cast<int>(root.leaf(WIDTH).dial());
            int height = static_cast<int>(root.leaf(HEIGHT).dial());
            fbayer_.resize(static_cast<size_t>(width) * height);
        }
        return fbayer_.data();
    }

    float* rgb() override {
        // Allocate on first access (4 floats per pixel for RGBX)
        if (rgb_.empty()) {
            auto& root = info_->root();
            int width = static_cast<int>(root.leaf(WIDTH).dial());
            int height = static_cast<int>(root.leaf(HEIGHT).dial());
            rgb_.resize(static_cast<size_t>(width) * height * 4);
        }
        return rgb_.data();
    }
};

// -------------------------------------------------------------------------
// HeadImpl - Sony ARW decoder
// -------------------------------------------------------------------------

class HeadImpl : public Head
{
public:
    std::string name() const override { return "head"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    std::unique_ptr<Flow> decode(const uint8_t* bytes, size_t size) override
    {
        sony::BayerU16 bayer;
        sony::Info info_map;
        sony::RawMetadata meta;

        if (!sony::Decoder::prepare(bytes, size, bayer, info_map, meta))
            return nullptr;

        // Create Tree and populate from metadata
        auto tree = makeTree();
        auto& root = tree->root();

        // Image dimensions
        root.leaf(WIDTH).dial(static_cast<float>(meta.width));
        root.leaf(HEIGHT).dial(static_cast<float>(meta.height));
        root.leaf(BLACK).dial(static_cast<float>(meta.black_level));
        root.leaf(WHITE).dial(static_cast<float>(meta.white_level));

        // Camera info
        auto& camera = root.next("camera");
        std::string make = meta.camera_make;
        std::string model = meta.camera_model;
        camera.leaf("make").text(make);
        camera.leaf("model").text(model);

        // EXIF
        auto& exif = root.next("exif");
        exif.leaf("iso").dial(meta.iso);
        exif.leaf("shutter").dial(meta.shutter_speed);
        exif.leaf("aperture").dial(meta.aperture);
        exif.leaf("focal_length").dial(meta.focal_length);
        std::string lens = meta.lens_model;
        exif.leaf("lens").text(lens);
        exif.leaf("orientation").dial(static_cast<float>(meta.orientation));

        // White balance
        auto& wb = root.next("wb");
        wb.leaf("r").dial(static_cast<float>(meta.wb_rggb[0]));
        wb.leaf("g1").dial(static_cast<float>(meta.wb_rggb[1]));
        wb.leaf("b").dial(static_cast<float>(meta.wb_rggb[2]));
        wb.leaf("g2").dial(static_cast<float>(meta.wb_rggb[3]));

        // Bayer pattern
        const char* patterns[] = {"RGGB", "GRBG", "BGGR", "GBRG"};
        std::string pattern = patterns[meta.bayer_pattern & 3];
        root.leaf("bayer").text(pattern);

        // Color matrix (3x3) - Sony 0x7800 (camera->sRGB)
        auto& matrix = root.next("color_matrix");
        for (int i = 0; i < 9; i++) {
            matrix.leaf(std::to_string(i)).dial(meta.color_matrix[i]);
        }

        // Camera-to-XYZ matrix - from LibRaw/dcraw database
        // This converts camera RGB to XYZ
        // Used by colorin to go camera RGB → XYZ → Lab (D50)
        // TODO: Build camera database instead of hardcoding
        auto& cam_xyz = root.next("cam_xyz");
        if (make == "SONY" && model == "ILCE-7M3") {
            // Sony A7III - from LibRaw cam_xyz
            float m[9] = {
                 0.7374f, -0.2389f, -0.0551f,
                -0.5435f,  1.3162f,  0.2519f,
                -0.1006f,  0.1795f,  0.6552f
            };
            for (int i = 0; i < 9; i++) {
                cam_xyz.leaf(std::to_string(i)).dial(m[i]);
            }
        } else {
            // Fallback: use metadata if available, else identity
            for (int i = 0; i < 9; i++) {
                float val = meta.cam_xyz[i];
                if (val == 0.0f && (i == 0 || i == 4 || i == 8)) val = 1.0f;
                cam_xyz.leaf(std::to_string(i)).dial(val);
            }
        }

        // Crop info
        auto& crop = root.next("crop");
        crop.leaf("left").dial(static_cast<float>(meta.crop_left));
        crop.leaf("top").dial(static_cast<float>(meta.crop_top));
        crop.leaf("width").dial(static_cast<float>(meta.crop_width));
        crop.leaf("height").dial(static_cast<float>(meta.crop_height));

        return std::make_unique<FlowImpl>(
            std::move(bayer),
            std::move(tree)
        );
    }
};

// -------------------------------------------------------------------------
// Factory
// -------------------------------------------------------------------------

std::unique_ptr<Head> makeHead()
{
    return std::make_unique<HeadImpl>();
}

} // namespace flow
