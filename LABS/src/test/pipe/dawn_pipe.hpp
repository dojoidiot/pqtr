// dawn_pipe.hpp - DAWN Compute Pipeline for shader testing
//
// Simple GPU compute runner for testing WGSL shaders.
// Supports both RGB (3-channel) and Bayer (1-channel) data.

#pragma once

#include <pipe.hpp>   // PIPE/inc/pipe.hpp (must come first)
#include <wgpu.hpp>   // WGPU/inc/wgpu.hpp
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

        // Set input image (BGR float32, row-major, 3 channels)
        void set_input(const float* data, uint32_t width, uint32_t height);

        // Set input Bayer (float32, row-major, 1 channel)
        void set_input_bayer(const float* data, uint32_t width, uint32_t height);

        // Set raw input (u32-packed u16 Bayer data)
        void set_input_raw(const uint32_t* data, uint32_t width, uint32_t height);

        // Set output buffer size (default: same as input)
        void set_output_channels(int channels);

        // Set uniform data (shader parameters)
        void set_uniforms(const void* data, size_t size);

        // Set additional data buffer (curves, LUTs, etc) at binding(3)
        void set_data(const void* data, size_t size);

        // Run compute shader
        bool dispatch();

        // Get output (float32)
        std::vector<float> get_output();

        // Get dimensions
        Dims dims() const { return dims_; }

    private:
        dawn::Instance instance_;
        dawn::Adapter adapter_;
        dawn::Device device_;
        dawn::ComputePipeline pipeline_;

        wgpu::Buffer input_buffer_;
        wgpu::Buffer output_buffer_;
        wgpu::Buffer uniform_buffer_;
        wgpu::Buffer data_buffer_;
        wgpu::Buffer staging_buffer_;
        wgpu::BindGroup bind_group_;

        Dims dims_;
        size_t input_size_;
        size_t output_size_;
        size_t uniform_size_;
        size_t data_size_;
        int output_channels_;
        bool ready_;
    };

} // namespace dawn_pipe
