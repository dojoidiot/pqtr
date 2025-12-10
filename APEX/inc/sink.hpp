#pragma once

#include <hold.hpp>
#include <vector>
#include <cstring>
#include <cstddef>

namespace pqtr
{
    using Size = size_t;
    using Bits = char *;
    // Sink: Write-many, then read-many buffer
    // User manages write/read phases - Sink is stateless
    // Stores data as chunks for efficient network/async I/O
    class Sink
    {
    public:
        Sink();
        virtual ~Sink();
        // No copy
        Sink(const Sink &) = delete;
        Sink &operator=(const Sink &) = delete;
        virtual void push(Bits bits, Size size) = 0;

        // READ PHASE: Take up to 'size' bytes
        // Returns actual bytes read (may be less if not enough data)
        // Allocates new buffer, caller must delete[]
        virtual int take(Bits &bits, Size size);

        // Get total bytes stored
        virtual Size size() = 0;

        virtual void tidy() = 0;
    };

    // Factory: create Sink from file path
    Hold<Sink> sink();

} // namespace pqtr
