// flow.cpp - Flow orchestration
//
// Thin wrapper that composes the parts: tree, load, swap, head

#include "flow.hpp"
#include "sony.h"

namespace flow
{

// Forward declarations from load.cpp
struct LoadResult
{
    std::vector<uint16_t> data;
    std::vector<uint8_t> view;
    std::unique_ptr<Tree> info;
    sony::RawMetadata metadata;
};
LoadResult load(const std::string &name, const uint8_t *bits, size_t size);

// Forward declaration from head.cpp
Task makeHeadTask(const uint16_t *data, int w, int h,
                  const sony::RawMetadata &metadata,
                  const uint8_t *view, size_t viewSize,
                  int viewWidth, int viewHeight);

// -------------------------------------------------------------------------
// FlowImpl
// -------------------------------------------------------------------------

class FlowImpl : public Flow
{
    std::vector<uint16_t> data_;
    std::vector<uint8_t> view_;      // JPEG bytes (for saving)
    std::vector<uint8_t> viewRgb_;   // Decoded RGB (for diff)
    int viewWidth_ = 0;
    int viewHeight_ = 0;
    std::unique_ptr<Tree> info_;
    sony::RawMetadata metadata_;

public:
    FlowImpl(const std::string &name, uint16_t *bits, size_t size)
    {
        auto result = load(name, reinterpret_cast<const uint8_t *>(bits), size);
        data_ = std::move(result.data);
        view_ = std::move(result.view);
        info_ = std::move(result.info);
        metadata_ = std::move(result.metadata);

        // Store decoded RGB for diff
        if (!metadata_.preview.data.empty())
        {
            viewRgb_ = std::move(metadata_.preview.data);
            viewWidth_ = metadata_.preview.width;
            viewHeight_ = metadata_.preview.height;
        }
    }

    Tree &info() override { return *info_; }
    uint16_t *data() override { return data_.data(); }
    uint8_t *view() override { return view_.data(); }
    size_t viewSize() override { return view_.size(); }

    Task head(void *device) override
    {
        (void)device; // TODO: pass device to GPU pipeline

        return makeHeadTask(data_.data(), metadata_.width, metadata_.height,
                            metadata_, viewRgb_.data(), viewRgb_.size(),
                            viewWidth_, viewHeight_);
    }

    Task tune(void *device) override
    {
        // TODO: tune includes head + profile learning
        // For now, just return head
        return head(device);
    }
};

// -------------------------------------------------------------------------
// Factory
// -------------------------------------------------------------------------

std::unique_ptr<Flow> make(std::string name, uint16_t *bits, size_t size)
{
    return std::make_unique<FlowImpl>(name, bits, size);
}

} // namespace flow
