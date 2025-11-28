// labs.cpp
// Pipe runner: Processes RAW through pipeline with optional edit settings
//
// WORKFLOW:
//   labs <source.ARW> --output tail.png [--edit edit.json]
//   → Loads RAW through HEAD (decode + color science)
//   → If edit.json provided, loads Link settings into BODY
//   → Runs BODY → TAIL → saves output image
//
// Usage:
//   labs <source.ARW> --output <image.png> [options]
//
// Options:
//   --output <image.png>    Output file (required)
//   --edit <edit.json>      Apply Link settings from tune
//   --size <pixels>         Max output dimension (default: 0 = full resolution)
//
// Examples:
//   labs photo.ARW --output photo.png                    # Baseline (no edits)
//   labs photo.ARW --output photo.png --edit edit.json   # With optimized settings
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
    std::cerr << "  --edit <edit.json>      Apply Link settings from tune\n";
    std::cerr << "  --size <pixels>         Max output dimension (default: full res)\n";
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printUsage(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string sourcePath = argv[1];
    std::string outputPath;
    std::string editPath;
    int maxSize = 0;

    for (int i = 2; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "--edit" && i + 1 < argc) editPath = argv[++i];
        else if (arg == "--size" && i + 1 < argc) maxSize = std::stoi(argv[++i]);
        else if (arg == "--help" || arg == "-h") { printUsage(argv[0]); return 0; }
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
        if (!editPath.empty()) std::cout << "Edit: " << editPath << std::endl;
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

        // Load edit settings if provided
        if (!editPath.empty())
        {
            std::cout << "  Loading edit: " << editPath << std::endl;

            // Check file exists
            std::ifstream check(editPath);
            if (!check.good())
            {
                throw std::runtime_error("Edit file not found: " + editPath);
            }
            check.close();

            // Add link and load settings
            pipe::Body::Link& link = body.add("edit");
            if (!data::link::load(link, editPath))
            {
                throw std::runtime_error("Failed to load edit: " + editPath);
            }
            std::cout << "  Applied!" << std::endl;
        }
        else
        {
            std::cout << "  No edit file (baseline output)" << std::endl;
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
