// head.cpp - Head implementation
//
// All implementation details hidden here. Uses static maps for PIMPL storage.

#include "head.hpp"
#include "flow.hpp"
#include <dawn/webgpu_cpp.h>
#include <cstring>
#include <unordered_map>

namespace head
{

    // =========================================================================
    // Uniforms - matches head.wgsl
    // =========================================================================

    struct HeadUniforms
    {
        uint32_t width;
        uint32_t height;
        float black_level;
        float white_level;
        float wb_r;
        float wb_b;
        uint32_t pattern;
        float _pad0;
        float m00, m01, m02, _p0;
        float m10, m11, m12, _p1;
        float m20, m21, m22, _p2;
    };

    // =========================================================================
    // Internal storage (hidden from header)
    // =========================================================================

    struct TaskData
    {
        wgpu::Device device;
        wgpu::ComputePipeline pipeline;
        wgpu::Buffer bayer_buf;
        wgpu::Buffer rgb_buf;
        wgpu::Buffer uniform_buf;
        wgpu::Buffer readback_buf;
        wgpu::BindGroup bind_group;
        int w = 0;
        int h = 0;
        size_t rgb_size = 0;
        bool is_posted = false;
    };

    struct HeadData
    {
        wgpu::Device device;
        wgpu::ComputePipeline pipeline;
        TaskData *current = nullptr;
    };

    // Single active task (API supports one at a time)
    static TaskData *g_active_task = nullptr;
    static std::unordered_map<const Head *, HeadData *> g_heads;

    // =========================================================================
    // Task implementation
    // =========================================================================

    Task::~Task()
    {
        // Cleanup is handled by Head::shut()
    }

    void Task::post()
    {
        if (!g_active_task || g_active_task->is_posted)
            return;

        TaskData &t = *g_active_task;
        wgpu::CommandEncoder enc = t.device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = enc.BeginComputePass();
        pass.SetPipeline(t.pipeline);
        pass.SetBindGroup(0, t.bind_group);
        pass.DispatchWorkgroups((t.w + 7) / 8, (t.h + 7) / 8, 1);
        pass.End();
        enc.CopyBufferToBuffer(t.rgb_buf, 0, t.readback_buf, 0, t.rgb_size);
        wgpu::CommandBuffer cmd = enc.Finish();
        t.device.GetQueue().Submit(1, &cmd);
        t.is_posted = true;
    }

    void *Task::view() const
    {
        if (!g_active_task)
            return nullptr;
        return const_cast<wgpu::Buffer *>(&g_active_task->rgb_buf);
    }

    int Task::width() const
    {
        return g_active_task ? g_active_task->w : 0;
    }

    int Task::height() const
    {
        return g_active_task ? g_active_task->h : 0;
    }

    // =========================================================================
    // Head implementation
    // =========================================================================

    Head::Head(void *device_ptr, void *pipeline_ptr)
    {
        auto *data = new HeadData();
        data->device = *static_cast<wgpu::Device *>(device_ptr);
        data->pipeline = *static_cast<wgpu::ComputePipeline *>(pipeline_ptr);
        g_heads[this] = data;
    }

    Head::~Head()
    {
        auto it = g_heads.find(this);
        if (it != g_heads.end())
        {
            delete it->second->current;
            delete it->second;
            g_heads.erase(it);
        }
    }

    Task Head::open(flow::Tree &info, uint16_t *data)
    {
        Task task;
        auto it = g_heads.find(this);
        if (it == g_heads.end() || !data)
            return task;

        HeadData &hd = *it->second;
        flow::Stem &root = info.root();

        int w = static_cast<int>(root.leaf(flow::WIDTH).dial());
        int h = static_cast<int>(root.leaf(flow::HEIGHT).dial());
        if (w <= 0 || h <= 0)
            return task;

        // Build uniforms
        HeadUniforms u{};
        u.width = static_cast<uint32_t>(w);
        u.height = static_cast<uint32_t>(h);
        u.black_level = root.leaf(flow::BLACK).dial();
        u.white_level = root.leaf(flow::WHITE).dial();
        if (u.white_level <= u.black_level)
        {
            u.black_level = 512;
            u.white_level = 16383;
        }

        u.wb_r = 1.0f;
        u.wb_b = 1.0f;
        if (root.test("maker"))
        {
            flow::Stem &maker = root.next("maker");
            if (maker.test("white_balance"))
            {
                flow::Stem &wb = maker.next("white_balance");
                if (wb.test("r"))
                    u.wb_r = wb.leaf("r").dial();
                if (wb.test("b"))
                    u.wb_b = wb.leaf("b").dial();
            }
            if (maker.test("bayer_pattern"))
            {
                int pat = static_cast<int>(maker.leaf("bayer_pattern").dial());
                if (pat >= 46 && pat <= 49)
                    u.pattern = static_cast<uint32_t>(pat - 46);
            }
        }

        u.m00 = 1; u.m01 = 0; u.m02 = 0;
        u.m10 = 0; u.m11 = 1; u.m12 = 0;
        u.m20 = 0; u.m21 = 0; u.m22 = 1;

        if (root.test("maker"))
        {
            flow::Stem &maker = root.next("maker");
            if (maker.test("color_matrix"))
            {
                std::string matStr = maker.leaf("color_matrix").text();
                float m[9];
                size_t pos = 0, idx = 0;
                while (pos < matStr.size() && idx < 9)
                {
                    size_t end = matStr.find(',', pos);
                    if (end == std::string::npos)
                        end = matStr.size();
                    std::string num = matStr.substr(pos, end - pos);
                    size_t start = num.find_first_not_of(" \t");
                    if (start != std::string::npos)
                        m[idx++] = std::stof(num.substr(start));
                    pos = end + 1;
                }
                if (idx == 9)
                {
                    u.m00 = m[0]; u.m01 = m[1]; u.m02 = m[2];
                    u.m10 = m[3]; u.m11 = m[4]; u.m12 = m[5];
                    u.m20 = m[6]; u.m21 = m[7]; u.m22 = m[8];
                }
            }
        }

        // Create TaskData
        auto *td = new TaskData();
        td->device = hd.device;
        td->pipeline = hd.pipeline;
        td->w = w;
        td->h = h;

        size_t bayer_size = static_cast<size_t>(w) * h * sizeof(uint16_t);
        size_t rgb_size = static_cast<size_t>(w) * h * 3 * sizeof(float);
        td->rgb_size = rgb_size;

        wgpu::BufferDescriptor desc{};

        desc.size = bayer_size;
        desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        td->bayer_buf = td->device.CreateBuffer(&desc);

        desc.size = rgb_size;
        desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
        td->rgb_buf = td->device.CreateBuffer(&desc);

        desc.size = sizeof(HeadUniforms);
        desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        td->uniform_buf = td->device.CreateBuffer(&desc);

        desc.size = rgb_size;
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
        td->readback_buf = td->device.CreateBuffer(&desc);

        wgpu::Queue queue = td->device.GetQueue();
        queue.WriteBuffer(td->bayer_buf, 0, data, bayer_size);
        queue.WriteBuffer(td->uniform_buf, 0, &u, sizeof(u));

        wgpu::BindGroupLayout layout = td->pipeline.GetBindGroupLayout(0);

        wgpu::BindGroupEntry entries[3]{};
        entries[0].binding = 0;
        entries[0].buffer = td->bayer_buf;
        entries[0].size = bayer_size;
        entries[1].binding = 1;
        entries[1].buffer = td->rgb_buf;
        entries[1].size = rgb_size;
        entries[2].binding = 2;
        entries[2].buffer = td->uniform_buf;
        entries[2].size = sizeof(HeadUniforms);

        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = layout;
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        td->bind_group = td->device.CreateBindGroup(&bgDesc);

        // Store (hd.current and g_active_task are the same)
        delete hd.current;
        hd.current = td;
        g_active_task = td;

        return task;
    }

    Done Head::shut()
    {
        Done out;
        auto it = g_heads.find(this);
        if (it == g_heads.end() || !it->second->current)
            return out;

        TaskData &t = *it->second->current;

        if (!t.is_posted)
        {
            wgpu::CommandEncoder enc = t.device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = enc.BeginComputePass();
            pass.SetPipeline(t.pipeline);
            pass.SetBindGroup(0, t.bind_group);
            pass.DispatchWorkgroups((t.w + 7) / 8, (t.h + 7) / 8, 1);
            pass.End();
            enc.CopyBufferToBuffer(t.rgb_buf, 0, t.readback_buf, 0, t.rgb_size);
            wgpu::CommandBuffer cmd = enc.Finish();
            t.device.GetQueue().Submit(1, &cmd);
        }

        bool done = false;
        t.readback_buf.MapAsync(
            wgpu::MapMode::Read, 0, t.rgb_size,
            wgpu::CallbackMode::AllowSpontaneous,
            [&](wgpu::MapAsyncStatus, wgpu::StringView) { done = true; });

        wgpu::Instance instance = t.device.GetAdapter().GetInstance();
        while (!done)
            instance.ProcessEvents();

        out.width = t.w;
        out.height = t.h;
        out.rgb.resize(t.w * t.h * 3);

        const float *mapped = static_cast<const float *>(t.readback_buf.GetConstMappedRange());
        std::memcpy(out.rgb.data(), mapped, t.rgb_size);
        t.readback_buf.Unmap();

        delete it->second->current;
        it->second->current = nullptr;
        g_active_task = nullptr;

        return out;
    }

} // namespace head
