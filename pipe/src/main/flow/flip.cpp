// flip.cpp - Image orientation (rotation/mirror)
//
// CLEAN COPY from darktable src/iop/flip.c
// Handles EXIF orientation and user-specified rotations.

#include "../../../inc/pipe.hpp"
#include <cstring>
#include <algorithm>
#include <vector>

namespace flow
{

// Orientation values from DT common/image.h
enum Orientation {
    ORIENTATION_NULL    = -1,  // autodetect from EXIF
    ORIENTATION_NONE    = 0,   // no rotation
    ORIENTATION_FLIP_Y  = 1,   // flip vertically
    ORIENTATION_FLIP_X  = 2,   // flip horizontally
    ORIENTATION_180     = 3,   // rotate 180°
    ORIENTATION_SWAP_XY = 4,   // transpose
    ORIENTATION_CW_90   = 5,   // rotate clockwise 90°
    ORIENTATION_CCW_90  = 6,   // rotate counter-clockwise 90°
    ORIENTATION_TRANSVERSE = 7 // transverse
};

class FlipImpl : public Flip
{
    int orientation_ = ORIENTATION_NULL;

public:
    std::string name() const override { return "flip"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setOrientation(int orientation) override
    {
        orientation_ = orientation;
    }

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();
        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());

        // Determine effective orientation
        int orient = orientation_;
        if (orient == ORIENTATION_NULL) {
            // Auto-detect from EXIF
            // For Sony A7III, orientation is typically 1 (normal) or 6 (90° CW)
            // Check if we have EXIF orientation in metadata
            if (root.test("exif")) {
                auto& exif = root.next("exif");
                if (exif.test("orientation")) {
                    int exif_orient = static_cast<int>(exif.leaf("orientation").dial());
                    // EXIF orientation to DT flip bits mapping
                    switch (exif_orient) {
                        case 1: orient = ORIENTATION_NONE; break;      // Normal
                        case 2: orient = ORIENTATION_FLIP_X; break;    // Mirror horizontal
                        case 3: orient = ORIENTATION_180; break;       // Rotate 180
                        case 4: orient = ORIENTATION_FLIP_Y; break;    // Mirror vertical
                        case 5: orient = ORIENTATION_SWAP_XY; break;   // Transpose
                        case 6: orient = ORIENTATION_CW_90; break;     // Rotate 90 CW
                        case 7: orient = ORIENTATION_TRANSVERSE; break;// Transverse
                        case 8: orient = ORIENTATION_CCW_90; break;    // Rotate 90 CCW
                        default: orient = ORIENTATION_NONE; break;
                    }
                } else {
                    orient = ORIENTATION_NONE;
                }
            } else {
                orient = ORIENTATION_NONE;
            }
        }

        // No-op if no rotation needed
        if (orient == ORIENTATION_NONE) {
            return;
        }

        size_t npixels = static_cast<size_t>(width) * height;
        float* rgb = flow.rgb();

        // Allocate temp buffer for rotation
        std::vector<float> temp(npixels * 4);

        // Determine output dimensions
        int out_width = width;
        int out_height = height;
        bool swap_dims = (orient & ORIENTATION_SWAP_XY) != 0;
        if (swap_dims) {
            out_width = height;
            out_height = width;
        }

        // Apply transformation
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int src_idx = (y * width + x) * 4;

                // Calculate destination coordinates
                int dx = x, dy = y;

                if (orient & ORIENTATION_FLIP_X) {
                    dx = width - 1 - dx;
                }
                if (orient & ORIENTATION_FLIP_Y) {
                    dy = height - 1 - dy;
                }
                if (orient & ORIENTATION_SWAP_XY) {
                    std::swap(dx, dy);
                }

                int dst_idx = (dy * out_width + dx) * 4;

                temp[dst_idx + 0] = rgb[src_idx + 0];
                temp[dst_idx + 1] = rgb[src_idx + 1];
                temp[dst_idx + 2] = rgb[src_idx + 2];
                temp[dst_idx + 3] = rgb[src_idx + 3];
            }
        }

        // Copy back
        memcpy(rgb, temp.data(), npixels * 4 * sizeof(float));

        // Update dimensions if swapped
        if (swap_dims) {
            root.leaf(WIDTH).dial(static_cast<float>(out_width));
            root.leaf(HEIGHT).dial(static_cast<float>(out_height));
        }
    }
};

std::unique_ptr<Flip> makeFlip()
{
    return std::make_unique<FlipImpl>();
}

} // namespace flow
