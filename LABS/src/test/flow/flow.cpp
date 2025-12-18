// flow test - load RAW and save sidecar
//
// Usage: ./flow [input.ARW]
// Output: tmp/var/flow/{name}.flow.json

#include "flow.hpp"

#include <fstream>
#include <iostream>
#include <cstdio>

static std::vector<uint8_t> read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return {};

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);

    return data;
}

static std::string basename(const std::string &path)
{
    size_t slash = path.rfind('/');
    size_t dot = path.rfind('.');
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    size_t end = (dot == std::string::npos || dot < start) ? path.size() : dot;
    return path.substr(start, end - start);
}

int main(int argc, char **argv)
{
    const char *input = "src/test/flow/DSC00144.ARW";
    if (argc > 1)
        input = argv[1];

    std::cout << "Loading: " << input << std::endl;

    // Read raw file
    auto raw = read_file(input);
    if (raw.empty())
    {
        std::cerr << "Failed to read: " << input << std::endl;
        return 1;
    }

    std::cout << "Size: " << raw.size() << " bytes" << std::endl;

    // Load via flow
    std::string name = basename(input);
    auto bits = reinterpret_cast<uint16_t *>(raw.data());
    auto f = flow::make(name, bits, raw.size());

    auto &root = f->info().root();
    int w = static_cast<int>(root.leaf(flow::WIDTH).dial());
    int h = static_cast<int>(root.leaf(flow::HEIGHT).dial());

    std::cout << "Data: " << w << "x" << h << std::endl;

    // Output sidecar JSON
    std::string jsonpath = "tmp/var/flow/" + name + ".flow.json";
    std::ofstream out(jsonpath);
    out << f->info().json() << std::endl;
    out.close();

    std::cout << "Saved: " << jsonpath << std::endl;

    // Output neg (raw bayer data)
    std::string negpath = "tmp/var/flow/" + name + ".neg";
    FILE *neg = fopen(negpath.c_str(), "wb");
    if (neg)
    {
        size_t pixels = static_cast<size_t>(w) * static_cast<size_t>(h);
        fwrite(f->data(), sizeof(uint16_t), pixels, neg);
        fclose(neg);
        std::cout << "Saved: " << negpath << " (" << pixels * 2 << " bytes)" << std::endl;
    }

    // Output preview JPEG
    auto &viewNode = root.next("view");
    int vw = static_cast<int>(viewNode.leaf(flow::WIDTH).dial());
    int vh = static_cast<int>(viewNode.leaf(flow::HEIGHT).dial());

    if (vw > 0 && vh > 0 && f->view())
    {
        auto jpg = flow::swap(f->view(), 0, vw, vh, flow::JPG);
        if (!jpg.empty())
        {
            std::string jpgpath = "tmp/var/flow/" + name + ".jpg";
            FILE *fp = fopen(jpgpath.c_str(), "wb");
            if (fp)
            {
                fwrite(jpg.data(), 1, jpg.size(), fp);
                fclose(fp);
                std::cout << "Saved: " << jpgpath << " (" << jpg.size() << " bytes)" << std::endl;
            }
        }
    }

    return 0;
}
