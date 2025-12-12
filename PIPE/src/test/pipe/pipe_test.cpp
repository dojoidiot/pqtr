// labs.cpp
// Apply edit.json to RAW → output tail.png
//
// Separation of concerns:
//   - tune: optimizes parameters, outputs edit.json
//   - labs: applies edit.json, outputs tail.png
//
// This test simulates the production workflow:
//   1. Load a RAW file
//   2. Load optimized parameters from edit.json (from tune)
//   3. Apply the full pipeline (HEAD → BODY with edit → TAIL)
//   4. Output the final processed image as tail.png
//
// Usage: test_labs <input.ARW> <edit.json> <output_dir>

#include <pipe.hpp>
#include <data.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <tool.hpp>
#include <iostream>
#include <fstream>
#include <chrono>

using namespace std::chrono;

void printUsage(const char* prog)
{
    std::cerr << "Usage: " << prog << " <input.ARW> <edit.json> <output_dir>\n\n";
    std::cerr << "Loads RAW, applies edit.json parameters, outputs tail.png\n";
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string rawPath = argv[1];
    std::string editPath = argv[2];
    std::string outputDir = argv[3];

    std::cout << "=== PIPE Test ===" << std::endl;
    std::cout << "Input: " << rawPath << std::endl;
    std::cout << "Edit:  " << editPath << std::endl;
    std::cout << "Output: " << outputDir << std::endl;
    std::cout << std::endl;

    try {
        auto start = high_resolution_clock::now();

        // Verify edit.json exists
        std::ifstream editCheck(editPath);
        if (!editCheck.good()) {
            std::cerr << "Error: Cannot read edit file: " << editPath << std::endl;
            return 1;
        }
        editCheck.close();

        // Create pipeline and load RAW
        std::cout << "[1] Loading RAW..." << std::endl;
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
        pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(rawPath));

        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));
        if (!head) {
            std::cerr << "Error: Failed to decode RAW" << std::endl;
            return 1;
        }

        pipe::Info info = head->data().info();
        std::cout << "    Decoded: " << info["width"] << "x" << info["height"] << std::endl;
        std::cout << "    Camera: " << info["camera_model"] << std::endl;

        // Create body at full resolution
        std::cout << "\n[2] Creating body..." << std::endl;
        pipe::Body& body = head->body(0);  // 0 = full resolution

        // Load edit.json into link
        std::cout << "\n[3] Loading edit.json..." << std::endl;
        pipe::Body::Link& link = body.add("edit");

        if (!data::link::load(link, editPath)) {
            std::cerr << "Error: Failed to load edit file: " << editPath << std::endl;
            return 1;
        }

        // Check if LUT was loaded
        if (link.lutCurve().isEstimated()) {
            std::cout << "    Loaded: dials + LUT" << std::endl;
        } else {
            std::cout << "    Loaded: dials only" << std::endl;
        }

        // Output via TAIL
        std::cout << "\n[4] Processing and saving..." << std::endl;
        std::string outputPath = outputDir + "/tail.png";

        if (!body.tail().save(outputPath, 0)) {  // 0 = full resolution
            std::cerr << "Error: Failed to save output" << std::endl;
            return 1;
        }

        // Verify output
        std::ifstream check(outputPath, std::ios::binary | std::ios::ate);
        if (!check.good()) {
            std::cerr << "Error: Output file not created" << std::endl;
            return 1;
        }

        size_t size = check.tellg();
        check.close();

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();

        std::cout << "    Saved: " << outputPath << std::endl;
        std::cout << "    Size: " << (size / 1024) << " KB" << std::endl;
        std::cout << "    Time: " << duration << " ms" << std::endl;

        std::cout << "\n✓ labs: PASSED" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
