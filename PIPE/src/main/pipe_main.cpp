// labs.cpp
// Applies a vibe to a RAW image
//
// Usage:
//   labs <source.ARW> --output <image.png> [--tune <vibe.json>] [--debug]
//
// Debug outputs (in same dir as output):
//   head.png - reference (camera preview)
//   tail.png - pipeline output
//   diff.png - difference (amplified 5x)

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <data.hpp>
#include <iostream>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

void printUsage(const char* prog)
{
    std::cerr << "Usage: " << prog << " <source.ARW> --output <image.png> [options]\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --output <image.png>    Output file (required)\n";
    std::cerr << "  --tune <vibe.json>      Style settings from tune\n";
    std::cerr << "  --size <pixels>         Max output dimension (default: full res)\n";
    std::cerr << "  --debug                 Save head.png, tail.png, diff.png\n";
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
    bool debug = false;

    for (int i = 2; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "--tune" && i + 1 < argc) tunePath = argv[++i];
        else if (arg == "--size" && i + 1 < argc) maxSize = std::stoi(argv[++i]);
        else if (arg == "--debug") debug = true;
        else if (arg == "--help" || arg == "-h") { /* handled above */ }
        else { std::cerr << "Unknown option: " << arg << "\n"; printUsage(argv[0]); return 1; }
    }

    if (outputPath.empty())
    {
        std::cerr << "Error: --output is required\n";
        printUsage(argv[0]);
        return 1;
    }

    // Debug directory (same as output)
    std::string debugDir = outputPath;
    size_t slashPos = debugDir.rfind('/');
    if (slashPos != std::string::npos) debugDir = debugDir.substr(0, slashPos);
    else debugDir = ".";

    try
    {
        std::cout << "=== PIPE ===" << std::endl;
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
        std::cout << "  BaseCurve: " << (head->hasBaseCurve() ? "yes" : "no") << std::endl;

        // Create body
        std::cout << "\n[BODY] Processing..." << std::endl;
        pipe::Body& body = head->body(maxSize);

        std::vector<pipe::Body::Link*> links;

        // Load tune settings if provided
        if (!tunePath.empty())
        {
            // Read tune file to count links
            std::ifstream file(tunePath);
            if (!file.good())
            {
                throw std::runtime_error("Tune file not found: " + tunePath);
            }
            std::string json((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            file.close();

            // Count links in JSON (look for "name": patterns)
            std::vector<std::string> linkNames;
            size_t pos = 0;
            while ((pos = json.find("\"name\":", pos)) != std::string::npos)
            {
                size_t start = json.find("\"", pos + 7);
                size_t end = json.find("\"", start + 1);
                if (start != std::string::npos && end != std::string::npos)
                {
                    linkNames.push_back(json.substr(start + 1, end - start - 1));
                }
                pos = end;
            }

            // Create links
            for (const auto& name : linkNames)
            {
                pipe::Body::Link& link = body.add(name);
                links.push_back(&link);
            }

            // BaseCurve is baked into tune.json if needed
            // Don't auto-apply here

            // Load settings
            if (!data::links::load(links, tunePath))
            {
                throw std::runtime_error("Failed to load tune: " + tunePath);
            }
            std::cout << "  Applied " << links.size() << " link(s)" << std::endl;
        }
        else
        {
            // No tune - create single baseline link
            // Skip estimated baseCurve - it's unreliable
            pipe::Body::Link& link = body.add("baseline");
            links.push_back(&link);
            std::cout << "  No tune (flat baseline)" << std::endl;
        }

        // Save output
        std::cout << "\n[TAIL] Saving..." << std::endl;
        if (!body.tail().save(outputPath, maxSize))
        {
            throw std::runtime_error("Failed to save: " + outputPath);
        }
        std::cout << "  " << outputPath << std::endl;

        // Debug: head.png, tail.png, diff.png
        if (debug)
        {
            // Get reference (camera preview) - already 8-bit BGR
            cv::Mat head8;
            head->view().view().copyTo(head8);

            // Get tail (pipeline output) - already 8-bit from toDisplayView
            cv::Mat tail8;
            body.view(maxSize).copyTo(tail8);

            // Resize head to match tail dimensions
            cv::Mat headResized;
            cv::resize(head8, headResized, cv::Size(tail8.cols, tail8.rows), 0, 0, cv::INTER_AREA);

            // Compute diff (amplified 5x)
            cv::Mat diff8;
            cv::absdiff(headResized, tail8, diff8);
            diff8.convertTo(diff8, -1, 5.0);

            // Save
            cv::imwrite(debugDir + "/head.png", headResized);
            cv::imwrite(debugDir + "/tail.png", tail8);
            cv::imwrite(debugDir + "/diff.png", diff8);
            std::cout << "  " << debugDir << "/head.png (reference)" << std::endl;
            std::cout << "  " << debugDir << "/tail.png (output)" << std::endl;
            std::cout << "  " << debugDir << "/diff.png (difference x5)" << std::endl;
        }

        std::cout << "\n[OK]" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
