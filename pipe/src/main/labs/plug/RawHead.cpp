// RawHead.cpp - labs::Head implementation for RAW file decoding
//
// Detects format via magic numbers and calls appropriate decoder:
//   - Sony ARW: TIFF + Sony MakerNotes
//   - Canon CR2: TIFF + "CR" signature at offset 8
//
// Usage: auto head = std::make_unique<RawHead>();

#include "../../../../inc/labs.hpp"
#include "sony/sony.h"
#include "canon/canon.h"
#include <memory>
#include <cstring>
#include <iostream>

using namespace pqtr;

// -------------------------------------------------------------------------
// Format detection via magic numbers
// -------------------------------------------------------------------------

namespace {

enum class RawFormat { UNKNOWN, SONY_ARW, CANON_CR2 };

RawFormat detect_format(const uint8_t* data, size_t size) {
    if (size < 16) return RawFormat::UNKNOWN;

    // Check TIFF header (little-endian "II" + 0x002A)
    bool is_tiff_le = (data[0] == 'I' && data[1] == 'I' &&
                       data[2] == 0x2A && data[3] == 0x00);

    if (!is_tiff_le) return RawFormat::UNKNOWN;

    // Canon CR2: "CR" at offset 8
    if (data[8] == 'C' && data[9] == 'R') {
        return RawFormat::CANON_CR2;
    }

    // Sony ARW: Default for TIFF-based RAW
    return RawFormat::SONY_ARW;
}

void fill_sony_metadata(Stem& head, const sony::RawMetadata& meta)
{
    // Image dimensions
    head.leaf(WIDTH).dial(static_cast<float>(meta.width));
    head.leaf(HEIGHT).dial(static_cast<float>(meta.height));
    head.leaf(BLACK).dial(static_cast<float>(meta.black_level));
    head.leaf(WHITE).dial(static_cast<float>(meta.white_level));

    // Camera info
    auto& camera = head.next("camera");
    std::string make = meta.camera_make;
    std::string model = meta.camera_model;
    camera.leaf("make").text(make);
    camera.leaf("model").text(model);

    // EXIF
    auto& exif = head.next("exif");
    exif.leaf("iso").dial(meta.iso);
    exif.leaf("shutter").dial(meta.shutter_speed);
    exif.leaf("aperture").dial(meta.aperture);
    exif.leaf("focal_length").dial(meta.focal_length);
    std::string lens = meta.lens_model;
    exif.leaf("lens").text(lens);
    exif.leaf("orientation").dial(static_cast<float>(meta.orientation));

    // White balance
    auto& wb = head.next("wb");
    wb.leaf("r").dial(static_cast<float>(meta.wb_rggb[0]));
    wb.leaf("g1").dial(static_cast<float>(meta.wb_rggb[1]));
    wb.leaf("b").dial(static_cast<float>(meta.wb_rggb[2]));
    wb.leaf("g2").dial(static_cast<float>(meta.wb_rggb[3]));

    // Bayer pattern
    const char* patterns[] = {"RGGB", "GRBG", "BGGR", "GBRG"};
    std::string pattern = patterns[meta.bayer_pattern & 3];
    head.leaf("bayer").text(pattern);

    // Filters (dcraw uint32 bitmask for FC() macro)
    const uint32_t filters_lut[] = {
        0x94949494, 0x61616161, 0x16161616, 0x49494949
    };
    head.leaf("filters").dial(static_cast<float>(filters_lut[meta.bayer_pattern & 3]));

    // Crop info
    auto& crop = head.next("crop");
    crop.leaf("left").dial(static_cast<float>(meta.crop_left));
    crop.leaf("top").dial(static_cast<float>(meta.crop_top));
    crop.leaf("width").dial(static_cast<float>(meta.crop_width));
    crop.leaf("height").dial(static_cast<float>(meta.crop_height));
}

void fill_canon_metadata(Stem& head, const canon::RawMetadata& meta)
{
    // Image dimensions
    head.leaf(WIDTH).dial(static_cast<float>(meta.width));
    head.leaf(HEIGHT).dial(static_cast<float>(meta.height));
    head.leaf(BLACK).dial(static_cast<float>(meta.black_level));
    head.leaf(WHITE).dial(static_cast<float>(meta.white_level));

    // Camera info
    auto& camera = head.next("camera");
    std::string make = meta.camera_make;
    std::string model = meta.camera_model;
    camera.leaf("make").text(make);
    camera.leaf("model").text(model);

    // EXIF
    auto& exif = head.next("exif");
    exif.leaf("iso").dial(meta.iso);
    exif.leaf("shutter").dial(meta.shutter_speed);
    exif.leaf("aperture").dial(meta.aperture);
    exif.leaf("focal_length").dial(meta.focal_length);
    std::string lens = meta.lens_model;
    exif.leaf("lens").text(lens);
    exif.leaf("orientation").dial(static_cast<float>(meta.orientation));

    // White balance
    auto& wb = head.next("wb");
    wb.leaf("r").dial(static_cast<float>(meta.wb_rggb[0]));
    wb.leaf("g1").dial(static_cast<float>(meta.wb_rggb[1]));
    wb.leaf("g2").dial(static_cast<float>(meta.wb_rggb[2]));
    wb.leaf("b").dial(static_cast<float>(meta.wb_rggb[3]));

    // Bayer pattern
    const char* patterns[] = {"RGGB", "GRBG", "BGGR", "GBRG"};
    std::string pattern = patterns[meta.bayer_pattern & 3];
    head.leaf("bayer").text(pattern);

    // Filters
    const uint32_t filters_lut[] = {
        0x94949494, 0x61616161, 0x16161616, 0x49494949
    };
    head.leaf("filters").dial(static_cast<float>(filters_lut[meta.bayer_pattern & 3]));

    // Crop info
    auto& crop = head.next("crop");
    crop.leaf("left").dial(static_cast<float>(meta.crop_left));
    crop.leaf("top").dial(static_cast<float>(meta.crop_top));
    crop.leaf("width").dial(static_cast<float>(meta.crop_width));
    crop.leaf("height").dial(static_cast<float>(meta.crop_height));
}

} // anonymous namespace

// ============================================================================
// RawHead - Multi-format RAW decoder plugin
// ============================================================================

class RawHead : public Head
{
public:
    std::unique_ptr<Flow> load(Flow& flow, const void* bytes, size_t size) override
    {
        const uint8_t* data = static_cast<const uint8_t*>(bytes);
        RawFormat format = detect_format(data, size);

        switch (format) {
        case RawFormat::SONY_ARW:
            return decode_sony(flow, data, size);
        case RawFormat::CANON_CR2:
            return decode_canon(flow, data, size);
        default:
            return nullptr;
        }
    }

private:
    std::unique_ptr<Flow> decode_sony(Flow& flow, const uint8_t* bytes, size_t size)
    {
        sony::BayerU16 bayer;
        sony::Info info_map;
        sony::RawMetadata meta;

        if (!sony::Decoder::prepare(bytes, size, bayer, info_map, meta)) {
            return nullptr;
        }

        // Fill flow.head() with metadata
        fill_sony_metadata(flow.head(), meta);

        // Set flow dimensions for downstream steps
        flow.flow().leaf(WIDTH).dial(static_cast<float>(meta.width));
        flow.flow().leaf(HEIGHT).dial(static_cast<float>(meta.height));

        // TODO: Copy bayer data to flow buffer

        return makeFlow();  // Return non-null to indicate success
    }

    std::unique_ptr<Flow> decode_canon(Flow& flow, const uint8_t* bytes, size_t size)
    {
        canon::BayerU16 bayer;
        canon::RawMetadata meta;

        if (!canon::Decoder::prepare(bytes, size, bayer, meta))
            return nullptr;

        // Fill flow.head() with metadata
        fill_canon_metadata(flow.head(), meta);

        // Set flow dimensions for downstream steps
        flow.flow().leaf(WIDTH).dial(static_cast<float>(meta.width));
        flow.flow().leaf(HEIGHT).dial(static_cast<float>(meta.height));

        // Copy bayer data to flow buffer
        // TODO: Proper buffer management

        return makeFlow();  // Return new flow (placeholder)
    }
};

// Factory function
std::unique_ptr<Head> makeRawHead()
{
    return std::make_unique<RawHead>();
}
