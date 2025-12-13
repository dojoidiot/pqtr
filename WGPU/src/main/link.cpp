// link.cpp - WGPU Link implementations
//
// OpenLink: CPU → GPU (after GEAR)
// ShutLink: GPU → CPU (end of pipe)

#include "wgpu.hpp"
#include <cstring>
#include <vector>

// BayerBuffer from GEAR (must match gear's definition)
namespace gear {
namespace sony {
    struct BayerBuffer {
        std::vector<uint16_t> data;
        int width;
        int height;
        int black_level;
        int white_level;
    };
}}

// Output buffer returned by shut()
struct OutputBuffer {
    std::vector<uint8_t> data;  // RGB8 or RGBA8
    int width;
    int height;
    int channels;
};

namespace wgpu {

// ============================================================
// OpenLink - CPU → GPU transfer
// ============================================================

class OpenLink : public pipe::Link {
    ::wgpu::Device m_device;  // Cached device (reused across calls)

public:
    pipe::Name name() const override { return "wgpu.open"; }
    pipe::Name type() const override { return "transfer"; }

    pipe::Data flow(pipe::Data in) override {
        // Get input BayerBuffer
        auto* bayer = static_cast<gear::sony::BayerBuffer*>(in.page);
        if (!bayer || bayer->data.empty()) {
            in.info.text("error", "no bayer buffer");
            return in;
        }

        // Initialize device if needed
        if (!m_device) {
            ::wgpu::Instance inst = dawn::instance();
            ::wgpu::Adapter adapt = dawn::adapter(inst);
            m_device = dawn::device(adapt);

            if (!m_device) {
                in.info.text("error", "failed to create wgpu device");
                return in;
            }
        }

        // Create GPU buffer for Bayer data
        size_t bufferSize = bayer->data.size() * sizeof(uint16_t);

        ::wgpu::BufferDescriptor bufDesc{};
        bufDesc.size = bufferSize;
        bufDesc.usage = ::wgpu::BufferUsage::Storage |
                        ::wgpu::BufferUsage::CopyDst |
                        ::wgpu::BufferUsage::CopySrc;
        bufDesc.mappedAtCreation = true;

        ::wgpu::Buffer gpuBuffer = m_device.CreateBuffer(&bufDesc);

        // Copy data to GPU
        void* mapped = gpuBuffer.GetMappedRange(0, bufferSize);
        std::memcpy(mapped, bayer->data.data(), bufferSize);
        gpuBuffer.Unmap();

        // Create context
        Context* ctx = new Context();
        ctx->device = m_device;
        ctx->buffer = gpuBuffer;
        ctx->width = bayer->width;
        ctx->height = bayer->height;
        ctx->channels = 1;  // Bayer = single channel

        // Free CPU buffer
        delete bayer;

        // Output
        pipe::Data out;
        out.page = ctx;
        out.info = std::move(in.info);
        out.info.text("gpu", "wgpu");
        out.info.dial("buffer_size", static_cast<float>(bufferSize));

        return out;
    }
};

// ============================================================
// ShutLink - GPU → CPU transfer + cleanup
// ============================================================

class ShutLink : public pipe::Link {
public:
    pipe::Name name() const override { return "wgpu.shut"; }
    pipe::Name type() const override { return "transfer"; }

    pipe::Data flow(pipe::Data in) override {
        auto* ctx = static_cast<Context*>(in.page);
        if (!ctx || !ctx->buffer) {
            in.info.text("error", "no gpu context");
            return in;
        }

        // Calculate output size
        size_t pixelCount = ctx->width * ctx->height;
        size_t bufferSize = pixelCount * ctx->channels * sizeof(float);

        // Create staging buffer for readback
        ::wgpu::BufferDescriptor stagingDesc{};
        stagingDesc.size = bufferSize;
        stagingDesc.usage = ::wgpu::BufferUsage::CopyDst |
                            ::wgpu::BufferUsage::MapRead;

        ::wgpu::Buffer staging = ctx->device.CreateBuffer(&stagingDesc);

        // Copy GPU → staging
        ::wgpu::CommandEncoder encoder = ctx->device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(ctx->buffer, 0, staging, 0, bufferSize);

        ::wgpu::CommandBuffer commands = encoder.Finish();
        ctx->device.GetQueue().Submit(1, &commands);

        // Map and read back
        bool done = false;
        staging.MapAsync(
            ::wgpu::MapMode::Read, 0, bufferSize,
            ::wgpu::CallbackMode::AllowSpontaneous,
            [&](::wgpu::MapAsyncStatus status, ::wgpu::StringView) {
                done = true;
            });

        // Wait for map
        while (!done) {
            ctx->device.GetAdapter().GetInstance().ProcessEvents();
        }

        // Create output buffer
        OutputBuffer* output = new OutputBuffer();
        output->width = ctx->width;
        output->height = ctx->height;
        output->channels = ctx->channels;

        // Convert float → uint8 (assuming [0,1] range)
        const float* src = static_cast<const float*>(staging.GetConstMappedRange(0, bufferSize));
        output->data.resize(pixelCount * ctx->channels);

        for (size_t i = 0; i < pixelCount * ctx->channels; i++) {
            float v = src[i];
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            output->data[i] = static_cast<uint8_t>(v * 255.0f);
        }

        staging.Unmap();

        // Cleanup GPU resources
        ctx->buffer.Destroy();
        delete ctx;

        // Output
        pipe::Data out;
        out.page = output;
        out.info = std::move(in.info);
        out.info.dial("output_size", static_cast<float>(output->data.size()));

        return out;
    }
};

// ============================================================
// Factories
// ============================================================

pipe::Hold<pipe::Link> open() {
    return pipe::Hold<pipe::Link>(new OpenLink());
}

pipe::Hold<pipe::Link> shut() {
    return pipe::Hold<pipe::Link>(new ShutLink());
}

} // namespace wgpu
