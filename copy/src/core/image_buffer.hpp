#pragma once

#include "types.hpp"
#include <vector>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace copy::core {

    template <typename T>
    class ImageBuffer {
    public:
        ImageBuffer(int width, int height, int channels = 1)
            : width_(width), height_(height), channels_(channels) {
            size_t size = static_cast<size_t>(width) * height * channels * sizeof(T);
            // aligned_alloc requires size to be a multiple of alignment
            size_t alignment = 64;
            size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
            
            void* ptr = std::aligned_alloc(alignment, aligned_size);
            if (!ptr) throw std::runtime_error("Failed to allocate aligned memory");
            
            data_.reset(static_cast<T*>(ptr));
            // Initialize to zero
            std::memset(data_.get(), 0, aligned_size);
        }

        T* data() { return data_.get(); }
        const T* data() const { return data_.get(); }

        int width() const { return width_; }
        int height() const { return height_; }
        int channels() const { return channels_; }
        size_t count() const { return static_cast<size_t>(width_) * height_ * channels_; }

    private:
        struct AlignedDeleter {
            void operator()(T* ptr) { std::free(ptr); }
        };

        int width_;
        int height_;
        int channels_;
        std::unique_ptr<T, AlignedDeleter> data_;
    };

}
