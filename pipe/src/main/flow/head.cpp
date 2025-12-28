// head.cpp - Head implementation (RAW decoder -> Flow)
//
// Detects format via magic numbers and calls appropriate decoder:
//   - Sony ARW: TIFF + Sony MakerNotes
//   - Canon CR2: TIFF + "CR" signature at offset 8

#include "../../../inc/pipe.hpp"
#include "sony.h"
#include "canon/canon.h"
#include <memory>
#include <cstring>

namespace flow
{

// Forward declaration from tree.cpp
std::unique_ptr<Tree> makeTree();

// -------------------------------------------------------------------------
// Format detection via magic numbers
// -------------------------------------------------------------------------

enum class RawFormat { UNKNOWN, SONY_ARW, CANON_CR2 };

static RawFormat detect_format(const uint8_t* data, size_t size) {
    if (size < 16) return RawFormat::UNKNOWN;

    // Check TIFF header (little-endian "II" + 0x002A)
    bool is_tiff_le = (data[0] == 'I' && data[1] == 'I' &&
                       data[2] == 0x2A && data[3] == 0x00);

    if (!is_tiff_le) return RawFormat::UNKNOWN;

    // Canon CR2: "CR" at offset 8
    if (data[8] == 'C' && data[9] == 'R') {
        return RawFormat::CANON_CR2;
    }

    // Sony ARW: Check for Sony in maker string (simplified detection)
    // Full detection happens in sony::Decoder::prepare
    return RawFormat::SONY_ARW;
}

// -------------------------------------------------------------------------
// FlowImpl - wraps decoded bayer data + metadata
// -------------------------------------------------------------------------

class FlowImpl : public Flow
{
    std::vector<uint16_t> bayer_;
    int bayer_width_ = 0;
    int bayer_height_ = 0;
    std::unique_ptr<Tree> info_;
    std::vector<float> fbayer_;  // normalized float bayer (filled by rawprepare)
    std::vector<float> rgb_;     // RGB float data (filled by demosaic)

public:
    // Constructor for Sony ARW
    FlowImpl(sony::BayerU16&& bayer, std::unique_ptr<Tree> info)
        : bayer_(std::move(bayer.data))
        , bayer_width_(bayer.width)
        , bayer_height_(bayer.height)
        , info_(std::move(info))
    {}

    // Constructor for Canon CR2
    FlowImpl(canon::BayerU16&& bayer, std::unique_ptr<Tree> info)
        : bayer_(std::move(bayer.data))
        , bayer_width_(bayer.width)
        , bayer_height_(bayer.height)
        , info_(std::move(info))
    {}

    Tree& info() override { return *info_; }
    uint16_t* data() override { return bayer_.data(); }

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
// HeadImpl - Multi-format RAW decoder
// -------------------------------------------------------------------------

class HeadImpl : public Head
{
public:
    std::string name() const override { return "head"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    std::unique_ptr<Flow> decode(const uint8_t* bytes, size_t size) override
    {
        RawFormat format = detect_format(bytes, size);

        switch (format) {
        case RawFormat::SONY_ARW:
            return decode_sony(bytes, size);
        case RawFormat::CANON_CR2:
            return decode_canon(bytes, size);
        default:
            return nullptr;
        }
    }

private:
    std::unique_ptr<Flow> decode_sony(const uint8_t* bytes, size_t size)
    {
        sony::BayerU16 bayer;
        sony::Info info_map;
        sony::RawMetadata meta;

        if (!sony::Decoder::prepare(bytes, size, bayer, info_map, meta))
            return nullptr;

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
        auto& cam_xyz = root.next("cam_xyz");
        if (make == "SONY" && model == "ILCE-7M3") {
            float m[9] = {
                 0.7374f, -0.2389f, -0.0551f,
                -0.5435f,  1.3162f,  0.2519f,
                -0.1006f,  0.1795f,  0.6552f
            };
            for (int i = 0; i < 9; i++) {
                cam_xyz.leaf(std::to_string(i)).dial(m[i]);
            }
        } else {
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

        return std::make_unique<FlowImpl>(std::move(bayer), std::move(tree));
    }

    std::unique_ptr<Flow> decode_canon(const uint8_t* bytes, size_t size)
    {
        canon::BayerU16 bayer;
        canon::RawMetadata meta;

        if (!canon::Decoder::prepare(bytes, size, bayer, meta))
            return nullptr;

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

        // White balance (Canon ColorData stores RGGB = [R, G1, G2, B])
        auto& wb = root.next("wb");
        wb.leaf("r").dial(static_cast<float>(meta.wb_rggb[0]));
        wb.leaf("g1").dial(static_cast<float>(meta.wb_rggb[1]));
        wb.leaf("g2").dial(static_cast<float>(meta.wb_rggb[2]));
        wb.leaf("b").dial(static_cast<float>(meta.wb_rggb[3]));

        // Bayer pattern (Canon is typically RGGB)
        const char* patterns[] = {"RGGB", "GRBG", "BGGR", "GBRG"};
        std::string pattern = patterns[meta.bayer_pattern & 3];
        root.leaf("bayer").text(pattern);

        // Camera-to-XYZ matrix - from LibRaw/dcraw database
        auto& cam_xyz = root.next("cam_xyz");
        if (model.find("EOS 40D") != std::string::npos) {
            // Canon EOS 40D - from LibRaw cam_xyz
            float m[9] = {
                 0.6071f, -0.0747f, -0.0763f,
                -0.4468f,  1.2227f,  0.2509f,
                -0.0500f,  0.0903f,  0.6174f
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

        return std::make_unique<FlowImpl>(std::move(bayer), std::move(tree));
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
