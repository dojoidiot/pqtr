// labs.cpp
// Pipe runner: Processes RAW through pipeline with optional tune settings
//
// WORKFLOW:
//   labs <source.ARW> --output tail.png [--tune tune.json]
//   → Loads RAW through HEAD (decode + color science)
//   → If tune.json provided, loads dial settings + 3D LUT into BODY
//   → Runs BODY → TAIL → saves output image
//
// Usage:
//   labs <source.ARW> --output <image.png> [options]
//
// Options:
//   --output <image.png>    Output file (required)
//   --tune <tune.json>      Apply settings from tune (dials + 3D LUT)
//   --size <pixels>         Max output dimension (default: 0 = full resolution)
//
// Examples:
//   labs photo.ARW --output photo.png                    # Baseline (no edits)
//   labs photo.ARW --output photo.png --tune style.json  # With optimized settings
//   labs photo.ARW --output thumb.png --size 1080        # Social media size

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <data.hpp>
#include <iostream>
#include <fstream>

void printUsage(const char* prog)
{
    std::cerr << "Usage: " << prog << " <source.ARW> --output <image.png> [options]\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --output <image.png>    Output file (required)\n";
    std::cerr << "  --tune <tune.json>      Apply settings from tune (dials + 3D LUT)\n";
    std::cerr << "  --size <pixels>         Max output dimension (default: full res)\n";
}

int main(int argc, char** argv)
{
    // Handle --help before minimum argument check
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { printUsage(argv[0]); return 0; }
    }

    if (argc < 2)
    {
        printUsage(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string sourcePath = argv[1];
    std::string outputPath;
    std::string tunePath;
    int maxSize = 0;

    for (int i = 2; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "--tune" && i + 1 < argc) tunePath = argv[++i];
        else if (arg == "--size" && i + 1 < argc) maxSize = std::stoi(argv[++i]);
        else if (arg == "--help" || arg == "-h") { /* handled above */ }
        else { std::cerr << "Unknown option: " << arg << "\n"; printUsage(argv[0]); return 1; }
    }

    if (outputPath.empty())
    {
        std::cerr << "Error: --output is required\n";
        printUsage(argv[0]);
        return 1;
    }

    try
    {
        std::cout << "=== LABS ===" << std::endl;
        std::cout << "Source: " << sourcePath << std::endl;
        std::cout << "Output: " << outputPath << std::endl;
        if (!tunePath.empty()) std::cout << "Tune: " << tunePath << std::endl;
        if (maxSize > 0) std::cout << "Size: " << maxSize << "px" << std::endl;

        // Create pipe and load RAW
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
        pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(sourcePath));

        std::cout << "\n[HEAD] Decoding..." << std::endl;
        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
        if (!head)
        {
            throw std::runtime_error("Failed to decode: " + sourcePath);
        }

        pipe::Info info = head->data().info();
        std::cout << "  Size: " << info["width"] << "x" << info["height"] << std::endl;
        std::cout << "  Camera: " << info["camera_model"] << std::endl;

        // Create body (no working size limit - we want full res for output)
        std::cout << "\n[BODY] Processing..." << std::endl;
        pipe::Body& body = head->body(0);

        // Load tune settings if provided
        if (!tunePath.empty())
        {
            std::cout << "  Loading tune: " << tunePath << std::endl;

            // Check file exists
            std::ifstream check(tunePath);
            if (!check.good())
            {
                throw std::runtime_error("Tune file not found: " + tunePath);
            }
            check.close();

            // Create links and load settings (two-link architecture: linear + display)
            pipe::Body::Link& linearLink = body.add("linear");
            pipe::Body::Link& displayLink = body.add("display");
            std::vector<pipe::Body::Link*> links = {&linearLink, &displayLink};

            if (!data::links::load(links, tunePath))
            {
                throw std::runtime_error("Failed to load tune: " + tunePath);
            }

            // Report what was loaded
            std::cout << "  Applied 2 links: linear + display" << std::endl;
            if (displayLink.lutCurve().isEstimated())
            {
                std::cout << "  Display link includes 3D LUT" << std::endl;
            }
        }
        else
        {
            std::cout << "  No tune file (baseline output)" << std::endl;
        }

        // Save via TAIL
        std::cout << "\n[TAIL] Saving..." << std::endl;
        if (!body.tail().save(outputPath, maxSize))
        {
            throw std::runtime_error("Failed to save: " + outputPath);
        }

        std::cout << "  Saved: " << outputPath << std::endl;
        std::cout << "\n[OK] Done" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
