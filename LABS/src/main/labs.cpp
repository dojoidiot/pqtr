// labs.cpp
// Main executable: Processes RAW file → PNG + .labs.json sidecar
//
// Usage: labs <input.ARW> [options] [output_dir]
// Options:
//   --default-display  Apply minimal display-referred processing (color matrix + tone map)
// Output: <input.png> and <input.ARW.labs.json>
// If output_dir specified: copies RAW there (if not present), creates outputs in output_dir

#include <tool.hpp>
#include <sink.hpp>
#include <pipe.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <opencv2/core.hpp>

// Pipe modules for display-referred processing
#include "pipe/mods/mods.h"

namespace fs = std::filesystem;

// Generate .labs.json with camera info from metadata
std::string generateLabsJson(const pipe::Info& info, bool defaultDisplay = false)
{
    // Helper to get value with default
    auto get = [&info](const std::string& key, const std::string& def = "") {
        auto it = info.find(key);
        return it != info.end() ? it->second : def;
    };

    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": \"1.0\",\n";
    json << "  \"decoder\": \"" << get("decoder", "sony_arw2") << "\",\n";
    json << "  \"camera\": {\n";
    json << "    \"make\": \"" << get("camera_make") << "\",\n";
    json << "    \"model\": \"" << get("camera_model") << "\",\n";
    json << "    \"lens\": \"" << get("lens_model") << "\"\n";
    json << "  },\n";
    json << "  \"exif\": {\n";
    json << "    \"iso\": " << get("iso", "0") << ",\n";
    json << "    \"shutter_speed\": " << get("shutter_speed", "0") << ",\n";
    json << "    \"aperture\": " << get("aperture", "0") << ",\n";
    json << "    \"focal_length\": " << get("focal_length", "0") << ",\n";
    json << "    \"orientation\": " << get("orientation", "1") << "\n";
    json << "  },\n";
    json << "  \"image\": {\n";
    json << "    \"width\": " << get("width", "0") << ",\n";
    json << "    \"height\": " << get("height", "0") << "\n";
    json << "  },\n";

    if (defaultDisplay)
    {
        json << "  \"links\": [\n";
        json << "    {\n";
        json << "      \"name\": \"default-display\",\n";
        json << "      \"modules\": {\n";
        json << "        \"tone_mapping\": { \"white_point\": 1.0, \"contrast\": 1.0 }\n";
        json << "      }\n";
        json << "    }\n";
        json << "  ]\n";
    }
    else
    {
        json << "  \"links\": []\n";
    }

    json << "}";
    return json.str();
}

// Get output filenames from input RAW path and optional output directory
struct OutputPaths
{
    std::string rawPath;  // Path to RAW file to process (may be copied)
    std::string png;      // Output PNG path
    std::string sidecar;  // Output .labs.json path
    bool needsCopy;       // True if RAW needs to be copied to output_dir
};

OutputPaths getOutputPaths(const std::string& inputRaw, const std::string& outputDir = "")
{
    OutputPaths out;
    fs::path inputPath(inputRaw);
    std::string baseName = inputPath.stem().string();
    std::string rawFileName = inputPath.filename().string();

    if (outputDir.empty())
    {
        // No output dir: create outputs next to input RAW
        out.rawPath = inputRaw;
        out.png = (inputPath.parent_path() / baseName).string() + ".png";
        out.sidecar = inputRaw + ".labs.json";
        out.needsCopy = false;
    }
    else
    {
        // Output dir specified
        fs::path outDir(outputDir);
        fs::path rawInOutDir = outDir / rawFileName;

        // Check if RAW already exists in output dir
        if (fs::exists(rawInOutDir))
        {
            out.rawPath = rawInOutDir.string();
            out.needsCopy = false;
        }
        else
        {
            out.rawPath = rawInOutDir.string();
            out.needsCopy = true;
        }

        out.png = (outDir / baseName).string() + ".png";
        out.sidecar = rawInOutDir.string() + ".labs.json";
    }

    return out;
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 4)
    {
        std::cerr << "Usage: " << argv[0] << " <input.ARW> [--default-display] [output_dir]" << std::endl;
        std::cerr << "  --default-display  Apply minimal display-referred processing" << std::endl;
        std::cerr << "  If output_dir specified, copies RAW there and creates outputs in output_dir" << std::endl;
        return 1;
    }

    // Parse arguments
    std::string inputRaw;
    std::string outputDir;
    bool defaultDisplay = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--default-display")
        {
            defaultDisplay = true;
        }
        else if (inputRaw.empty())
        {
            inputRaw = arg;
        }
        else
        {
            outputDir = arg;
        }
    }

    try
    {
        // Get output paths
        OutputPaths out = getOutputPaths(inputRaw, outputDir);

        std::cout << "Processing: " << inputRaw << std::endl;
        if (!outputDir.empty())
        {
            std::cout << "Output directory: " << outputDir << std::endl;
            fs::create_directories(outputDir);
        }

        // Copy RAW to output dir if needed
        if (out.needsCopy)
        {
            std::cout << "Copying RAW to: " << out.rawPath << std::endl;
            fs::copy_file(inputRaw, out.rawPath, fs::copy_options::none);
        }

        std::cout << "Output PNG: " << out.png << std::endl;
        std::cout << "Output sidecar: " << out.sidecar << std::endl;

        // === HEAD: Decode RAW ===
        std::cout << "\n[HEAD] Decoding RAW..." << std::endl;

        // Load RAW file into Sink (use the potentially copied path)
        pqtr::Sink* rawSink = pqtr::Tool::read(out.rawPath);

        // Decode using pipe interface (abstracts decoder selection)
        pipe::Head head;
        if (!pipe::open(*rawSink, pipe::decoder::SONY_ARW2, head))
        {
            delete rawSink;
            throw std::runtime_error("Failed to decode RAW file");
        }

        delete rawSink;  // Done with sink

        std::cout << "  Decoded: " << head.info["width"] << "x" << head.info["height"] << std::endl;
        std::cout << "  Camera: " << head.info["camera_make"] << " " << head.info["camera_model"] << std::endl;

        // === BODY: Process through pipeline ===
        std::cout << "\n[BODY] Processing..." << std::endl;

        cv::UMat outputRgb;

        if (defaultDisplay)
        {
            // Display-referred pipeline: linear sRGB → tone map → gamma
            std::cout << "  Mode: default-display (tone map)" << std::endl;
            std::cout << "  Scene-linear sRGB: " << head.view.cols << "x" << head.view.rows << std::endl;

            // Step 1: Apply tone mapping (HDR → SDR compression)
            cv::UMat toneMapped;
            if (!pipe::mods::tone_map(head.view, toneMapped, 1.0f, 1.0f))
            {
                throw std::runtime_error("Failed to apply tone mapping");
            }
            std::cout << "  Tone mapping applied" << std::endl;

            // Step 2: Apply gamma (sRGB OETF)
            if (!pipe::gamma(toneMapped, outputRgb))
            {
                throw std::runtime_error("Failed to apply gamma");
            }
            std::cout << "  Gamma applied" << std::endl;
        }
        else
        {
            // Standard pipeline: just apply gamma to linear RGB
            if (!pipe::gamma(head.view, outputRgb))
            {
                throw std::runtime_error("Failed to apply gamma");
            }
            std::cout << "  Output RGB: " << outputRgb.cols << "x" << outputRgb.rows << std::endl;
        }

        // === TAIL: Output ===
        std::cout << "\n[TAIL] Saving PNG..." << std::endl;

        // Save PNG using pipe interface
        if (!pipe::save(outputRgb, out.png))
        {
            throw std::runtime_error("Failed to save PNG");
        }

        std::cout << "  Saved: " << out.png << std::endl;

        // === Save .labs.json sidecar ===
        std::cout << "\n[SIDECAR] Saving .labs.json..." << std::endl;

        std::string labsJson = generateLabsJson(head.info, defaultDisplay);
        std::ofstream sidecarFile(out.sidecar);
        if (!sidecarFile)
        {
            throw std::runtime_error("Failed to create sidecar file");
        }
        sidecarFile << labsJson;
        sidecarFile.close();

        std::cout << "  Saved: " << out.sidecar << std::endl;

        std::cout << "\n✓ Processing complete!" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
