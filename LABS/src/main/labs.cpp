// labs.cpp
// Pipe runner: Processes RAW through pipeline with tune settings
//
// WORKFLOW:
//   tune <source.ARW> <target> --save-area dir   → vibe.json
//   labs <source.ARW> --tune vibe.json --output out.png
//   labs ... --debug                             → saves pipeline stages
//
// Usage:
//   labs <source.ARW> --output <image.png> [--tune <vibe.json>] [--debug]
//
// Options:
//   --output <image.png>    Output file (required)
//   --tune <vibe.json>      Style settings from tune
//   --size <pixels>         Max output dimension (default: full res)
//   --debug                 Save intermediate pipeline stages

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <data.hpp>
#include <iostream>
#include <fstream>
#include <opencv2/imgcodecs.hpp>

void printUsage(const char* prog)
{
    std::cerr << "Usage: " << prog << " <source.ARW> --output <image.png> [options]\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --output <image.png>    Output file (required)\n";
    std::cerr << "  --tune <vibe.json>      Style settings from tune\n";
    std::cerr << "  --size <pixels>         Max output dimension (default: full res)\n";
    std::cerr << "  --debug                 Save intermediate pipeline stages\n";
}

// Save debug image helper
void saveDebug(const std::string& basePath, const std::string& stage, pipe::View view)
{
    // Convert to 8-bit for saving
    cv::Mat cpu;
    view.copyTo(cpu);

    // Clamp and convert to 8-bit
    cv::Mat out;
    cpu.convertTo(out, CV_8UC3, 255.0);

    std::string path = basePath + "_" + stage + ".png";
    cv::imwrite(path, out);
    std::cout << "  [debug] " << stage << " → " << path << std::endl;
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

    // Debug base path (strip extension from output)
    std::string debugBase = outputPath;
    size_t dotPos = debugBase.rfind('.');
    if (dotPos != std::string::npos) debugBase = debugBase.substr(0, dotPos);

    try
    {
        std::cout << "=== LABS ===" << std::endl;
        std::cout << "Source: " << sourcePath << std::endl;
        std::cout << "Output: " << outputPath << std::endl;
        if (!tunePath.empty()) std::cout << "Tune: " << tunePath << std::endl;
        if (maxSize > 0) std::cout << "Size: " << maxSize << "px" << std::endl;
        if (debug) std::cout << "Debug: ON" << std::endl;

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

        // Debug: save flat (scene-linear) data
        if (debug)
        {
            saveDebug(debugBase, "0_flat", head->data().view());
            saveDebug(debugBase, "0_preview", head->view().view());
        }

        // Create body
        std::cout << "\n[BODY] Processing..." << std::endl;
        pipe::Body& body = head->body(maxSize);

        // Create links (two-link architecture: linear + display)
        pipe::Body::Link& linearLink = body.add("linear");
        pipe::Body::Link& displayLink = body.add("display");
        std::vector<pipe::Body::Link*> links = {&linearLink, &displayLink};

        // Always apply base curve from camera
        if (head->hasBaseCurve())
        {
            linearLink.baseCurve().setCurve(head->baseCurve());
            std::cout << "  Applied base curve" << std::endl;
        }

        // Load tune settings if provided
        if (!tunePath.empty())
        {
            std::ifstream check(tunePath);
            if (!check.good())
            {
                throw std::runtime_error("Tune file not found: " + tunePath);
            }
            check.close();

            if (!data::links::load(links, tunePath))
            {
                throw std::runtime_error("Failed to load tune: " + tunePath);
            }
            std::cout << "  Applied tune settings" << std::endl;

            if (displayLink.lutCurve().isEstimated())
            {
                std::cout << "  Includes 3D LUT" << std::endl;
            }
        }
        else
        {
            std::cout << "  No tune (baseline)" << std::endl;
        }

        // Debug: save after each link
        if (debug)
        {
            // Get working data and apply links one at a time
            pipe::View working;
            body.data().view().copyTo(working);

            // After base curve (linear link partial)
            // Note: can't easily get intermediate - save full link results
            saveDebug(debugBase, "1_body", body.view(maxSize));
        }

        // Save output
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
