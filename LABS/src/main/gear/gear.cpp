// link.cpp - GEAR Link implementation
//
// GearLink: First link in pipe
//   Input:  Data = raw file buffer, Info.dial("size") = buffer size
//   Output: Data = Bayer buffer, Info = camera metadata
//
// Detects format from magic bytes, dispatches to manufacturer decoder.

#include "gear.hpp"
#include <cstring>

namespace gear {

// ============================================================
// Format detection from magic bytes
// ============================================================

enum class Format {
    Unknown,
    SonyARW,    // TIFF-based, Sony MakerNotes
    CanonCR2,   // TIFF-based (future)
    CanonCR3,   // ISO BMFF (future)
    NikonNEF,   // TIFF-based (future)
    AdobeDNG,   // TIFF-based (future)
};

static Format detectFormat(const char* data, size_t size) {
    if (size < 16) return Format::Unknown;

    // TIFF-based formats (ARW, NEF, CR2, DNG)
    // Little-endian: "II" + 42 (0x2A00)
    // Big-endian: "MM" + 42 (0x002A)
    bool isTiff = (data[0] == 'I' && data[1] == 'I' && data[2] == 42 && data[3] == 0) ||
                  (data[0] == 'M' && data[1] == 'M' && data[2] == 0 && data[3] == 42);

    if (isTiff) {
        // For now, assume TIFF = Sony ARW
        // TODO: Parse MakerNotes to distinguish Sony/Nikon/Canon/DNG
        return Format::SonyARW;
    }

    // Canon CR3 (ISO BMFF)
    if (size >= 12 && memcmp(data + 4, "ftyp", 4) == 0) {
        return Format::CanonCR3;
    }

    return Format::Unknown;
}

// ============================================================
// Forward declarations for manufacturer decoders
// ============================================================

// Sony decoder - implemented in sony.cpp
// Returns: data = Bayer buffer (allocated), info = metadata
namespace sony {
    pipe::Flow decode(const char* data, size_t size);
}

// ============================================================
// GearLink - Decode raw buffer to Bayer + metadata
// ============================================================

class GearLink : public pipe::Link {
public:
    pipe::Name name() const override { return "gear"; }

    pipe::Flow flow(pipe::Flow in) override {
        // Get input buffer from Data
        const char* data = static_cast<const char*>(in.data);
        size_t size = static_cast<size_t>(in.info.dial("size"));

        if (!data || size == 0) {
            // No input - return as-is
            in.info.text("error", "no input buffer");
            return in;
        }

        // Detect format
        Format fmt = detectFormat(data, size);

        // Dispatch to manufacturer decoder
        switch (fmt) {
            case Format::SonyARW:
                return sony::decode(data, size);

            case Format::CanonCR2:
            case Format::CanonCR3:
            case Format::NikonNEF:
            case Format::AdobeDNG:
                // Future: implement these
                in.info.text("error", "format not yet supported");
                return in;

            default:
                in.info.text("error", "unknown format");
                return in;
        }
    }
};

// ============================================================
// Factory
// ============================================================

pipe::Hold<pipe::Link> link() {
    return pipe::Hold<pipe::Link>(new GearLink());
}

} // namespace gear
