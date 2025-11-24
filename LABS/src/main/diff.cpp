// diff.cpp
// Headless tool: Computes perceptual difference between two images
//
// Usage: diff <candidate.png> <target.png> [options]
// Options:
//   --visual-diff <output.png>  Generate visual difference image
//   --scale <factor>            Amplification for visual diff (default: 5.0)
//   --grid-report <NxM>         Generate grid-based loss report
//
// Output: Loss score and optional visual diff / grid report

#include <iostream>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

// Placeholder for diff part
// #include "diff.h"

void printUsage(const char* programName)
{
    std::cerr << "Usage: " << programName << " <candidate.png> <target.png> [options]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --visual-diff <output.png>  Generate visual difference image" << std::endl;
    std::cerr << "  --scale <factor>            Amplification for visual diff (default: 5.0)" << std::endl;
    std::cerr << "  --grid-report <NxM>         Generate grid-based loss report" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printUsage(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string candidatePath;
    std::string targetPath;
    std::string visualDiffPath;
    float scale = 5.0f;
    std::string gridReport;

    int argIndex = 1;
    candidatePath = argv[argIndex++];
    targetPath = argv[argIndex++];

    while (argIndex < argc)
    {
        std::string arg = argv[argIndex++];
        if (arg == "--visual-diff" && argIndex < argc)
        {
            visualDiffPath = argv[argIndex++];
        }
        else if (arg == "--scale" && argIndex < argc)
        {
            scale = std::stof(argv[argIndex++]);
        }
        else if (arg == "--grid-report" && argIndex < argc)
        {
            gridReport = argv[argIndex++];
        }
        else
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    try
    {
        std::cout << "Diff Tool" << std::endl;
        std::cout << "  Candidate: " << candidatePath << std::endl;
        std::cout << "  Target: " << targetPath << std::endl;

        // Load images
        cv::Mat candidate = cv::imread(candidatePath);
        cv::Mat target = cv::imread(targetPath);

        if (candidate.empty())
        {
            throw std::runtime_error("Failed to load candidate image: " + candidatePath);
        }
        if (target.empty())
        {
            throw std::runtime_error("Failed to load target image: " + targetPath);
        }

        std::cout << "  Candidate size: " << candidate.cols << "x" << candidate.rows << std::endl;
        std::cout << "  Target size: " << target.cols << "x" << target.rows << std::endl;

        // TODO: Implement diff computation
        // For now, just report placeholder values
        float loss = 0.0f;
        std::cout << "\nLoss: " << loss << " (0.00%)" << std::endl;

        if (!visualDiffPath.empty())
        {
            std::cout << "\nVisual diff: " << visualDiffPath << " (scale: " << scale << "x)" << std::endl;
            // TODO: Generate and save visual diff
            std::cout << "  [Not yet implemented]" << std::endl;
        }

        if (!gridReport.empty())
        {
            std::cout << "\nGrid report: " << gridReport << std::endl;
            // TODO: Generate grid report
            std::cout << "  [Not yet implemented]" << std::endl;
        }

        std::cout << "\n✓ Diff complete!" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
