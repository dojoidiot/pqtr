// labs.cpp
// Pipe runner: Processes RAW through pipeline with optional tune settings
//
// WORKFLOW:
//   labs <source.ARW> --output tail.png [--link link.json]
//   → Loads RAW through HEAD (decode + color science)
//   → If link.json provided, loads Link settings into BODY
//   → Runs BODY → TAIL → saves output image
//
// Usage:
//   labs <source.ARW> --output <image.png> [options]
//
// Options:
//   --output <image.png>    Output file (required)
//   --link <link.json>      Apply Link settings from tune
//   --size <pixels>         Max output dimension (default: 0 = full resolution)
//
// Examples:
//   labs photo.ARW --output photo.png                    # Baseline (no edits)
//   labs photo.ARW --output photo.png --link style.json  # With optimized settings
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
    std::cerr << "  --link <link.json>      Apply Link settings from tune\n";
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
    std::string linkPath;
    int maxSize = 0;

    for (int i = 2; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "--link" && i + 1 < argc) linkPath = argv[++i];
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
        if (!linkPath.empty()) std::cout << "Link: " << linkPath << std::endl;
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

        // Load link settings if provided
        if (!linkPath.empty())
        {
            std::cout << "  Loading link: " << linkPath << std::endl;

            // Check file exists
            std::ifstream check(linkPath);
            if (!check.good())
            {
                throw std::runtime_error("Link file not found: " + linkPath);
            }
            check.close();

            // Add link and load settings
            pipe::Body::Link& link = body.add("tune");
            if (!data::link::load(link, linkPath))
            {
                throw std::runtime_error("Failed to load link: " + linkPath);
            }
            std::cout << "  Applied!" << std::endl;
        }
        else
        {
            std::cout << "  No link file (baseline output)" << std::endl;
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
