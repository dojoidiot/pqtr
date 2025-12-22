// flow.cpp - Flow orchestration
//
// Thin wrapper that composes the parts: tree, load, swap, head

#include "flow.hpp"

namespace flow
{
    // Forward declarations from parts
    struct LoadResult
    {
        std::vector<uint16_t> data;
        std::vector<uint8_t> view;
        std::unique_ptr<Tree> info;
    };

    LoadResult load(const std::string &name, const uint8_t *bits, size_t size);

    // GPU helpers (from head.cpp)
    Task headOpen(Tree &info, uint16_t *data, void *device);
    Task headTune(Tree &info, uint16_t *data, uint8_t *view, size_t viewSize, void *device);

    // -------------------------------------------------------------------------
    // FlowImpl
    // -------------------------------------------------------------------------

    class FlowImpl : public Flow
    {
        std::vector<uint16_t> data_;
        std::vector<uint8_t> view_;
        std::unique_ptr<Tree> info_;

    public:
        FlowImpl(const std::string &name, uint16_t *bits, size_t size)
        {
            auto result = load(name, reinterpret_cast<const uint8_t *>(bits), size);
            data_ = std::move(result.data);
            view_ = std::move(result.view);
            info_ = std::move(result.info);
        }

        Tree &info() override { return *info_; }
        uint16_t *data() override { return data_.data(); }
        uint8_t *view() override { return view_.data(); }
        size_t viewSize() override { return view_.size(); }

        Task head(void *device) override
        {
            return headOpen(*info_, data_.data(), device);
        }

        Task tune(void *device) override
        {
            return headTune(*info_, data_.data(), view_.data(), view_.size(), device);
        }
    };

    // -------------------------------------------------------------------------
    // Factory
    // -------------------------------------------------------------------------

    std::unique_ptr<Flow> make(std::string name, uint8_t *bits, size_t size)  downscaled  bits
    {
        return std::make_unique<FlowImpl>(name, bits, size);
    }

} // namespace flow
