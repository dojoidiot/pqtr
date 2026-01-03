// Dump.cpp - DumpStep that writes buffer to binary file for comparison

#include "pqtr.hpp"
#include <iostream>
#include <fstream>

namespace pqtr::Labs {

class DumpStep : public Step
{
    std::string path_;
    size_t elem_size_;
    int channels_;
public:
    DumpStep(const std::string &path, int channels = 4)
        : path_(path), elem_size_(sizeof(float)), channels_(channels) {}
    void *exec(Flow &flow) override;
};

void* DumpStep::exec(Flow& flow) {
    int width = static_cast<int>(flow.head().leaf(WIDTH).dial());
    int height = static_cast<int>(flow.head().leaf(HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;
    size_t bytes = npixels * channels_ * elem_size_;

    std::ofstream f(path_, std::ios::binary);
    if (f) {
        f.write(static_cast<const char*>(flow.data()), bytes);
        std::cout << "DumpStep: " << path_ << " (" << bytes << " bytes)\n";
    } else {
        std::cerr << "DumpStep: failed to open " << path_ << "\n";
    }

    return flow.data();
}

std::unique_ptr<Step> dumpStep(const std::string &path, int channels) { return std::make_unique<DumpStep>(path, channels); }

}  // namespace pqtr::Labs
