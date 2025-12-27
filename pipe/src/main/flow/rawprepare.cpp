// rawprepare.cpp - Black level subtraction and normalization
//
// Matches darktable's rawprepare module:
// - Subtracts black level per bayer channel
// - Normalizes to [0, 1] range using (pixel - black) / (white - black)

#include "../../../inc/pipe.hpp"
#include <cmath>

namespace flow
{

// -------------------------------------------------------------------------
// RawprepareImpl - Black level subtraction
// -------------------------------------------------------------------------

class RawprepareImpl : public Rawprepare
{
public:
    std::string name() const override { return "rawprepare"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();

        // Get dimensions
        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());

        // Get black/white levels from metadata
        float black = root.leaf(BLACK).dial();  // 512 for Sony
        float white = root.leaf(WHITE).dial();  // 15360 from XMP

        // For now, use uniform black level for all channels
        // TODO: Support per-channel black levels if XMP specifies them
        float sub[4] = { black, black, black, black };
        float div[4] = { white - black, white - black, white - black, white - black };

        // Get input/output buffers
        const uint16_t* in = flow.data();
        float* out = flow.fdata();

        // Process each pixel
        // Darktable algorithm: out = (in - sub[id]) / div[id]
        // where id = ((row & 1) << 1) + (col & 1) for RGGB bayer
        for (int row = 0; row < height; row++)
        {
            for (int col = 0; col < width; col++)
            {
                size_t idx = static_cast<size_t>(row) * width + col;

                // Bayer position: 0=R, 1=Gr, 2=Gb, 3=B for RGGB
                int id = ((row & 1) << 1) + (col & 1);

                float val = static_cast<float>(in[idx]);
                val = (val - sub[id]) / div[id];

                // NO clamping - DT allows values outside [0,1]
                out[idx] = val;
            }
        }
    }
};

// -------------------------------------------------------------------------
// Factory
// -------------------------------------------------------------------------

std::unique_ptr<Rawprepare> makeRawprepare()
{
    return std::make_unique<RawprepareImpl>();
}

} // namespace flow
