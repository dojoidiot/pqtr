// pipe.cpp
// Implementation of pipe.hpp HEAD/TAIL functions
// Abstracts decoder selection and output transforms

#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>

// Internal decoder (not exposed in public API)
#include "sony.h"

namespace pipe
{

// HEAD: Decode RAW → scene-linear RGB
bool open(pqtr::Sink& sink, const std::string& decoderName, Head& head)
{
    // Dispatch to appropriate decoder based on name
    if (decoderName == decoder::SONY_ARW2 || decoderName == "sony")
    {
        // Use Sony ARW2 decoder
        cv::UMat bayer;
        sony::Info sonyInfo;
        sony::RawMetadata metadata;

        // Decode RAW header and extract Bayer data
        if (!sony::Decoder::prepare(sink, bayer, sonyInfo, metadata))
        {
            return false;
        }

        // Process to scene-linear RGB
        if (!sony::Decoder::process_linear(bayer, metadata, head.view))
        {
            return false;
        }

        // Convert sony::Info to pipe::Info
        head.info = sonyInfo;

        // Add structured metadata
        head.info["decoder"] = decoderName;
        head.info["width"] = std::to_string(metadata.crop_width);
        head.info["height"] = std::to_string(metadata.crop_height);
        head.info["camera_make"] = metadata.camera_make;
        head.info["camera_model"] = metadata.camera_model;
        head.info["lens_model"] = metadata.lens_model;

        std::ostringstream oss;
        oss << metadata.iso;
        head.info["iso"] = oss.str();

        oss.str("");
        oss << metadata.shutter_speed;
        head.info["shutter_speed"] = oss.str();

        oss.str("");
        oss << metadata.aperture;
        head.info["aperture"] = oss.str();

        oss.str("");
        oss << metadata.focal_length;
        head.info["focal_length"] = oss.str();

        head.info["orientation"] = std::to_string(metadata.orientation);

        return true;
    }

    // Unknown decoder
    return false;
}

// TAIL: Apply gamma and save to PNG file
bool save(const View& linear, const std::string& path)
{
    // Apply sRGB gamma (OETF)
    cv::UMat gamma_encoded;
    if (!sony::Decoder::apply_gamma(linear, gamma_encoded))
    {
        return false;
    }

    // Convert to 8-bit for PNG
    cv::UMat output8bit;
    gamma_encoded.convertTo(output8bit, CV_8UC3, 255.0);

    // Copy to CPU for imwrite
    cv::Mat cpuMat;
    output8bit.copyTo(cpuMat);

    // Save PNG
    return cv::imwrite(path, cpuMat);
}

} // namespace pipe
