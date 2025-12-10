// sink.cpp - Sink PIMPL implementation

#include <sink.hpp>

namespace pqtr
{

// Base class stubs
Sink::Sink() {}
Sink::~Sink() {}
int Sink::take(Bits &bits, Size size) { bits = nullptr; return 0; }

// Implementation class
class SinkImpl : public Sink
{
public:
    SinkImpl() : totalSize_(0), readOffset_(0) {}

    ~SinkImpl()
    {
        for (auto &chunk : chunks_)
            delete[] chunk.data;
    }

    void push(Bits bits, Size size) override
    {
        chunks_.push_back({bits, size});
        totalSize_ += size;
    }

    int take(Bits &bits, Size size) override
    {
        if (readOffset_ >= totalSize_ || size == 0)
        {
            bits = nullptr;
            return 0;
        }

        Size available = totalSize_ - readOffset_;
        Size toRead = (size < available) ? size : available;

        bits = new char[toRead];
        Size copied = 0;
        Size currentOffset = readOffset_;

        for (const auto &chunk : chunks_)
        {
            if (currentOffset >= chunk.size)
            {
                currentOffset -= chunk.size;
                continue;
            }

            Size chunkOffset = currentOffset;
            Size available_in_chunk = chunk.size - chunkOffset;
            Size toCopy = (toRead - copied < available_in_chunk)
                              ? (toRead - copied)
                              : available_in_chunk;

            memcpy(bits + copied, chunk.data + chunkOffset, toCopy);
            copied += toCopy;
            currentOffset = 0;

            if (copied >= toRead)
                break;
        }

        readOffset_ += copied;
        return static_cast<int>(copied);
    }

    Size size() override
    {
        return totalSize_;
    }

    void tidy() override
    {
        totalSize_ = 0;
        readOffset_ = 0;
    }

private:
    struct Chunk
    {
        Bits data;
        Size size;
    };

    std::vector<Chunk> chunks_;
    Size totalSize_;
    Size readOffset_;
};

// Factory
Hold<Sink> sink()
{
    return Hold<Sink>(new SinkImpl());
}

} // namespace pqtr
