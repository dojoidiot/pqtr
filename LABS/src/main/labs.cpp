// labs.cpp
// Main executable: Processes RAW file → PNG + .labs.json sidecar
//
// Usage: labs <input.ARW> [output_dir]
// Output: <input.png> and <input.ARW.labs.json>
// If output_dir specified: copies RAW there (if not present), creates outputs in output_dir

#include <tool.hpp>
#include <sink.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

// Direct integration of opt/raws decoder
#include "sony.h"

namespace fs = std::filesystem;

// Generate .labs.json with camera info from metadata
std::string generateLabsJson(const sony::RawMetadata& metadata, const std::string& decoder = "sony")
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": \"1.0\",\n";
    json << "  \"decoder\": \"" << decoder << "\",\n";
    json << "  \"camera\": {\n";
    json << "    \"make\": \"" << metadata.camera_make << "\",\n";
    json << "    \"model\": \"" << metadata.camera_model << "\",\n";
    json << "    \"lens\": \"" << metadata.lens_model << "\"\n";
    json << "  },\n";
    json << "  \"exif\": {\n";
    json << "    \"iso\": " << metadata.iso << ",\n";
    json << "    \"shutter_speed\": " << metadata.shutter_speed << ",\n";
    json << "    \"aperture\": " << metadata.aperture << ",\n";
    json << "    \"focal_length\": " << metadata.focal_length << ",\n";
    json << "    \"orientation\": " << metadata.orientation << "\n";
    json << "  },\n";
    json << "  \"image\": {\n";
    json << "    \"width\": " << metadata.width << ",\n";
    json << "    \"height\": " << metadata.height << "\n";
    json << "  },\n";
    json << "  \"links\": []\n";
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
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input.ARW> [output_dir]" << std::endl;
        std::cerr << "  If output_dir specified, copies RAW there and creates outputs in output_dir" << std::endl;
        return 1;
    }

    std::string inputRaw = argv[1];
    std::string outputDir = (argc == 3) ? argv[2] : "";

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

        // Decode using Sony ARW decoder
        cv::UMat bayerData;
        sony::Info info;
        sony::RawMetadata metadata;

        if (!sony::Decoder::prepare(*rawSink, bayerData, info, metadata))
        {
            delete rawSink;
            throw std::runtime_error("Failed to decode RAW file");
        }

        delete rawSink;  // Done with sink

        std::cout << "  Decoded: " << metadata.width << "x" << metadata.height << std::endl;
        std::cout << "  Camera: " << metadata.camera_make << " " << metadata.camera_model << std::endl;

        // === BODY: Process through Sony pipeline ===
        std::cout << "\n[BODY] Processing..." << std::endl;

        cv::UMat linearRgb;
        if (!sony::Decoder::process(bayerData, metadata, linearRgb))
        {
            throw std::runtime_error("Failed to process RAW file");
        }

        std::cout << "  Linear RGB: " << linearRgb.cols << "x" << linearRgb.rows << std::endl;

        // === TAIL: Output ===
        std::cout << "\n[TAIL] Saving PNG..." << std::endl;

        // Convert to 8-bit (linearRgb is already gamma corrected from process())
        cv::UMat output8bit;
        linearRgb.convertTo(output8bit, CV_8UC3, 255.0);

        // Save PNG
        if (!cv::imwrite(out.png, output8bit))
        {
            throw std::runtime_error("Failed to save PNG");
        }

        std::cout << "  Saved: " << out.png << std::endl;

        // === Save .labs.json sidecar ===
        std::cout << "\n[SIDECAR] Saving .labs.json..." << std::endl;

        std::string labsJson = generateLabsJson(metadata, "sony");
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
