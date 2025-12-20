#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * This is the flow namespace.  This is designed for me to read - it must be maintained as minimal interface,
 * PIMPL.  No boilerplate, low cognitive load.
 *
 * DO NOT ADD CODE HERE.
 *
 * DO NOT CHANGE THIS COMMENT.
 */
namespace flow
{

    // ============================================================================
    // Field Keys
    // ============================================================================

    constexpr const char *NAME = "name";
    constexpr const char *WIDTH = "width";
    constexpr const char *HEIGHT = "height";
    constexpr const char *BLACK = "black";
    constexpr const char *WHITE = "white";

    // ============================================================================
    // Tree - hierarchical metadata (PIMPL)
    // ============================================================================

    // Base class for tree nodes
    class Node
    {
    public:
        virtual ~Node() = default;
    };

    // Leaf node - holds a single value (dial or text)
    class Leaf : public Node
    {
    public:
        virtual float dial() = 0;                // get numeric value
        virtual void dial(float val) = 0;        // set numeric value
        virtual std::string &text() = 0;         // get string value
        virtual void text(std::string &val) = 0; // set string value
        virtual bool live() = 0;                 // true if value has been set
    };

    // Stem node - holds child nodes (branches and leaves)
    class Stem : public Node
    {
    public:
        virtual Stem &next(const std::string &name) = 0;      // get/create child stem
        virtual Leaf &leaf(const std::string &name) = 0;      // get/create child leaf
        virtual Node *find(const std::string &name) = 0;      // find child (null if missing)
        virtual size_t size() const = 0;                      // number of children
        virtual bool test(const std::string &name) const = 0; // check if child exists
        virtual void tidy() = 0;                              // remove empty children
        virtual std::vector<std::string> list() = 0;          // list child names
    };

    // Tree root - owns the hierarchy
    class Tree
    {
    public:
        virtual ~Tree() = default;
        virtual Stem &root() = 0;                // get root stem
        virtual std::string json() = 0;          // serialize to JSON
        virtual void read(std::string json) = 0; // deserialize from JSON
    };

    // ============================================================================
    // Flow - RAW image container
    // ============================================================================

    class Flow
    {
    public:
        virtual ~Flow() = default;
        virtual Tree &info() = 0;      // metadata tree
        virtual uint16_t *data() = 0;  // raw bayer data (width * height)
        virtual uint8_t *view() = 0;   // preview RGB (from embedded JPEG)
        virtual size_t viewSize() = 0; // size of view buffer in bytes
    };

    // ============================================================================
    // Head - GPU RAW processing (PIMPL)
    // ============================================================================

    // Final CPU result
    struct Done
    {
        std::vector<float> rgb; // interleaved RGB, linear scene-referred
        int width = 0;
        int height = 0;
    };

    // GPU processing job
    class Task
    {
    public:
        ~Task();
        void post();        // dispatch GPU work
        void *view() const; // GPU buffer (valid after post)
        int width() const;
        int height() const;
    };

    // RAW to linear RGB processor
    class Head
    {
    public:
        Head(void *device_ptr, void *pipeline_ptr);
        ~Head();
        Task open(Tree &info, uint16_t *data);
        Done shut();
    };

    // ============================================================================
    // Factory & Utilities
    // ============================================================================

    // Load RAW file into Flow container
    std::unique_ptr<Flow> make(std::string name, uint16_t *bits, size_t size);

    // Format conversion
    enum Swap
    {
        JPG,
        PNG,
        BIN
    };

    // Convert between formats:
    //   BIN->PNG/JPG: data is w*h*3 RGB, size ignored
    //   PNG/JPG->BIN: data is compressed, size is data length, w/h extracted from image
    std::vector<uint8_t> swap(uint8_t *data, size_t size, int w, int h, Swap into);

    // Lens distortion correction (operates on RGB float data, w*h*3)
    void warp(float *rgb, int w, int h, const float *params, int count);

}
