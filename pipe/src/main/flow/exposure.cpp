// exposure.cpp - Scene-referred exposure adjustment
//
// Matches darktable's exposure module (manual mode).
// Simple linear scaling: out = (in - black) * exp2(-ev)

#include "../../../inc/pipe.hpp"
#include <cmath>

namespace flow
{

class ExposureImpl : public Exposure
{
    float ev_ = 0.0f;
    float black_ = 0.0f;

public:
    std::string name() const override { return "exposure"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setParams(float ev, float black) override
    {
        ev_ = ev;
        black_ = black;
    }

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();

        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());
        size_t npixels = static_cast<size_t>(width) * height;

        // CLEAN COPY from DT exposure.c lines 507-508, 562:
        // white = exp2f(-exposure)
        // scale = 1.0 / (white - black)
        // out = (in - black) * scale
        //
        // For exposure=0.7, black=0:
        //   white = exp2f(-0.7) ≈ 0.615
        //   scale = 1.0 / 0.615 ≈ 1.625
        //   Result: image becomes 1.625x brighter
        float white = std::exp2f(-ev_);
        float scale = 1.0f / (white - black_);

        float* rgb = flow.rgb();

        for (size_t i = 0; i < npixels; i++)
        {
            size_t idx = i * 4;
            rgb[idx + 0] = (rgb[idx + 0] - black_) * scale;
            rgb[idx + 1] = (rgb[idx + 1] - black_) * scale;
            rgb[idx + 2] = (rgb[idx + 2] - black_) * scale;
        }
    }
};

std::unique_ptr<Exposure> makeExposure()
{
    return std::make_unique<ExposureImpl>();
}

} // namespace flow
