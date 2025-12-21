// pipe.hpp - DAWN Compute Pipeline for VIBE
//
// Simple GPU compute runner for testing WGSL shaders against theory.
// Each mod is a compute shader that processes BGR float32 images.

#pragma once

#include <dawn/webgpu_cpp.h>

// Dawn helper functions (defined in wgpu.cpp)
namespace dawn {
    wgpu::Instance instance();
    wgpu::Adapter adapter(wgpu::Instance instance);
    wgpu::Device device(wgpu::Adapter adapter);
    wgpu::ComputePipeline pipeline(wgpu::Device device, const char* wgsl);
}
#include <vector>
#include <string>
#include <cstdint>

namespace dawn_pipe
{
    // Image dimensions
    struct Dims
    {
        uint32_t width;
        uint32_t height;
    };

    // Compute pipeline for image processing
    class Pipe
    {
    public:
        Pipe();
        ~Pipe();

        // Initialize WebGPU
        bool init();

        // Create pipeline from WGSL shader source
        bool load(const char* wgsl);

        // Set input image (BGR float32, row-major)
        void set_input(const float* data, uint32_t width, uint32_t height);

        // Set uniform data (shader parameters)
        void set_uniforms(const void* data, size_t size);

        // Set additional data buffer (curves, LUTs, etc) at binding(3)
        void set_data(const void* data, size_t size);

        // Run compute shader
        bool dispatch();

        // Get output image (BGR float32)
        std::vector<float> get_output();

        // Get dimensions
        Dims dims() const { return dims_; }

    private:
        wgpu::Instance instance_;
        wgpu::Adapter adapter_;
        wgpu::Device device_;
        wgpu::ComputePipeline pipeline_;

        wgpu::Buffer input_buffer_;
        wgpu::Buffer output_buffer_;
        wgpu::Buffer uniform_buffer_;
        wgpu::Buffer data_buffer_;
        wgpu::Buffer staging_buffer_;
        wgpu::BindGroup bind_group_;

        Dims dims_;
        size_t buffer_size_;
        size_t uniform_size_;
        size_t data_size_;
        bool ready_;
    };

} // namespace dawn_pipe
