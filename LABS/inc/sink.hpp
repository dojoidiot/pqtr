#pragma once

#include <vector>
#include <cstring>
#include <cstddef>

namespace pqtr
{

    // Sink: Write-many, then read-many buffer
    // User manages write/read phases - Sink is stateless
    // Stores data as chunks for efficient network/async I/O
    class Sink
    {
    public:
        Sink() : totalSize_(0), readOffset_(0) {}

        ~Sink()
        {
            // Free all chunk allocations
            for (auto& chunk : chunks_)
            {
                delete[] chunk.data;
            }
        }

        // WRITE PHASE: Push a chunk of data
        // Sink takes ownership of the chunk pointer
        void push(char* data, size_t size)
        {
            chunks_.push_back({data, size});
            totalSize_ += size;
        }

        // READ PHASE: Take up to 'size' bytes
        // Returns actual bytes read (may be less if not enough data)
        // Allocates new buffer, caller must delete[]
        int take(char*& buffer, size_t size)
        {
            if (readOffset_ >= totalSize_ || size == 0)
            {
                buffer = nullptr;
                return 0;
            }

            // Determine how many bytes we can actually read
            size_t available = totalSize_ - readOffset_;
            size_t toRead = (size < available) ? size : available;

            // Allocate output buffer
            buffer = new char[toRead];
            size_t copied = 0;
            size_t currentOffset = readOffset_;

            // Copy from chunks
            for (const auto& chunk : chunks_)
            {
                if (currentOffset >= chunk.size)
                {
                    // Skip chunks we've already read past
                    currentOffset -= chunk.size;
                    continue;
                }

                // How much to copy from this chunk
                size_t chunkOffset = currentOffset;
                size_t available_in_chunk = chunk.size - chunkOffset;
                size_t toCopy = (toRead - copied < available_in_chunk)
                    ? (toRead - copied)
                    : available_in_chunk;

                memcpy(buffer + copied, chunk.data + chunkOffset, toCopy);
                copied += toCopy;
                currentOffset = 0;  // After first chunk, start from beginning of next chunks

                if (copied >= toRead)
                {
                    break;
                }
            }

            // Update read offset
            readOffset_ += copied;

            return copied;
        }

        // Get total bytes stored
        size_t size() const
        {
            return totalSize_;
        }

        // Reset for reuse (keeps allocations)
        void tidy()
        {
            // Don't free chunks - keep for reuse
            totalSize_ = 0;
            readOffset_ = 0;
            // Note: chunks_ vector keeps pointers but they can be reused
        }

        // No copy
        Sink(const Sink&) = delete;
        Sink& operator=(const Sink&) = delete;

    private:
        struct Chunk
        {
            char* data;
            size_t size;
        };

        std::vector<Chunk> chunks_;
        size_t totalSize_;
        size_t readOffset_;
    };

} // namespace pqtr
