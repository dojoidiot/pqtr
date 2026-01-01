// labs.cpp - Labs pipeline CLI
//
// Usage:
//   labs --head <raw_file> --pipe <pipe.json> --tail <output_file>
//   labs --head <raw_file> --test <test.xmp> --tail <output_file>
//
// Examples:
//   labs --head DSC00501.ARW --pipe config.json --tail output.png
//   labs --head DSC00501.ARW --test DSC00501.ARW.xmp --tail output.jpg

#include "../../../inc/labs.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <map>
#include <functional>

using namespace pqtr;

// ============================================================================
// Plugin includes (definitions are in plug/*.cpp, linked in)
// ============================================================================

// Forward declare plugin classes defined in plug/*.cpp
class RawHead;
class PngTail;
class JpgTail;
class Dump;

// Factory functions for plugins
std::unique_ptr<Head> makeRawHead();
std::unique_ptr<Tail> makePngTail();
std::unique_ptr<Tail> makeJpgTail();
std::unique_ptr<Step> makeDump();

// ============================================================================
// Mods table - maps step names to factory functions
// ============================================================================

using StepFactory = std::function<std::unique_ptr<Step>()>;

static std::map<std::string, StepFactory> mods = {
    {"dump", makeDump},
};

// ============================================================================
// Helpers
// ============================================================================

static void usage(const char* prog)
{
    std::cerr << "Usage:\n"
              << "  " << prog << " --head <raw_file> --pipe <pipe.json> --tail <output_file>\n"
              << "  " << prog << " --head <raw_file> --test <test.xmp> --tail <output_file>\n"
              << "\nOptions:\n"
              << "  --head <file>   Input RAW file (ARW, CR2)\n"
              << "  --pipe <file>   Pipe configuration JSON\n"
              << "  --test <file>   Darktable test XMP file\n"
              << "  --tail <file>   Output file (PNG or JPG based on extension)\n";
}

static std::vector<uint8_t> readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << path << "\n";
        return {};
    }

    auto size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

static bool endsWith(const std::string& str, const std::string& suffix)
{
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string toLower(const std::string& s)
{
    std::string result = s;
    for (char& c : result)
        if (c >= 'A' && c <= 'Z')
            c += 32;
    return result;
}

int main(int argc, char* argv[])
{
    std::string headFile, pipeFile, testFile, tailFile;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--head") == 0 && i + 1 < argc) {
            headFile = argv[++i];
        } else if (strcmp(argv[i], "--pipe") == 0 && i + 1 < argc) {
            pipeFile = argv[++i];
        } else if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            testFile = argv[++i];
        } else if (strcmp(argv[i], "--tail") == 0 && i + 1 < argc) {
            tailFile = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    // Validate arguments
    if (headFile.empty()) {
        std::cerr << "Error: --head is required\n";
        usage(argv[0]);
        return 1;
    }
    if (pipeFile.empty() && testFile.empty()) {
        std::cerr << "Error: --pipe or --test is required\n";
        usage(argv[0]);
        return 1;
    }

    // Read input RAW file
    std::cout << "Reading: " << headFile << "\n";
    auto rawData = readFile(headFile);
    if (rawData.empty()) {
        return 1;
    }
    std::cout << "  Size: " << rawData.size() << " bytes\n";

    // Read pipe config or test XMP
    std::string pipeJson;
    if (!pipeFile.empty()) {
        auto configData = readFile(pipeFile);
        if (configData.empty()) {
            return 1;
        }
        pipeJson = std::string(configData.begin(), configData.end());
        std::cout << "Pipe config: " << pipeFile << "\n";
    } else if (!testFile.empty()) {
        auto xmpData = readFile(testFile);
        if (xmpData.empty()) {
            return 1;
        }
        // TODO: Parse XMP and convert to pipe JSON
        std::cout << "Test XMP: " << testFile << "\n";
        std::cout << "  (XMP parsing not yet implemented)\n";
        pipeJson = "{}";  // Empty config for now
    }

    // Create pipe
    std::cout << "Creating pipe...\n";
    auto pipe = make();
    if (!pipe) {
        std::cerr << "Error: Failed to create pipe\n";
        return 1;
    }

    // Wire head
    std::cout << "Creating head...\n";
    std::unique_ptr<Head> head = makeRawHead();

    // Wire tail only if --tail specified
    std::unique_ptr<Tail> tail = nullptr;
    if (!tailFile.empty()) {
        std::string tailExt = toLower(tailFile);
        bool isJpg = endsWith(tailExt, ".jpg") || endsWith(tailExt, ".jpeg");
        std::cout << "Output format: " << (isJpg ? "JPG" : "PNG") << "\n";
        tail = isJpg ? makeJpgTail() : makePngTail();
    }

    std::cout << "Joining head" << (tail ? " and tail" : "") << "...\n";
    pipe->join(std::move(head), std::move(tail));

    // Parse pipe JSON and add steps from flow section
    std::cout << "Parsing pipe JSON...\n";
    auto flow = makeFlow();
    flow->read(pipeJson);

    // Get step names from flow.flow() and wire them
    std::cout << "Wiring steps...\n";
    auto stepNames = flow->flow().list();
    for (const auto& name : stepNames) {
        auto it = mods.find(name);
        if (it != mods.end()) {
            std::cout << "  Adding step: " << name << "\n";
            pipe->join(name, it->second());
        } else {
            std::cerr << "Warning: Unknown step '" << name << "' - skipping\n";
        }
    }

    std::cout << "Processing pipeline...\n";
    std::cout << "  Calling pump with " << rawData.size() << " bytes\n";

    // Pump data through pipe
    void* result = pipe->pump(rawData.data(), rawData.size());

    if (!tailFile.empty()) {
        std::cout << "  Pump returned: " << (result ? "data" : "null") << "\n";
        if (!result) {
            std::cerr << "Error: Pipeline processing failed\n";
            return 1;
        }
        // TODO: Get actual output size from tail and write file
        std::cout << "Writing: " << tailFile << "\n";
        // writeFile(tailFile, result, outputSize);
    }

    std::cout << "Done.\n";
    return 0;
}
