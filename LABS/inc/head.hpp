#pragma once

#include <memory>
#include <vector>
#include <cstdint>

namespace flow
{
    class Tree;
}

/**
 * Head - GPU RAW processing (PIMPL)
 *
 * Usage:
 *   Head head(&device, &pipeline);
 *   Task task = head.open(info, data);
 *   task.post();
 *   Done result = head.shut();
 */
namespace head
{

    // =========================================================================
    // Done - final CPU result
    // =========================================================================

    struct Done
    {
        std::vector<float> rgb; // interleaved RGB, linear scene-referred
        int width = 0;
        int height = 0;
    };

    // =========================================================================
    // Task - GPU processing job (PIMPL)
    // =========================================================================

    class Task
    {
    public:
        ~Task();

        void post();              // dispatch GPU work
        void *view() const;       // GPU buffer (valid after post)
        int width() const;
        int height() const;
    };

    // =========================================================================
    // Head - RAW to linear RGB processor (PIMPL)
    // =========================================================================

    class Head
    {
    public:
        Head(void *device_ptr, void *pipeline_ptr);
        ~Head();
        Task open(flow::Tree &info, uint16_t *data);
        Done shut();
    };

} // namespace head
