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
    //
    // NOTE: Embedded JPEG preview is NOT included in Flow.
    // Sony ARW format varies by camera generation - older cameras store
    // full JPEG separately, newer ones embed JPEG+thumb in ARW.
    // Preview extraction is a separate concern outside the pipe.

    class Flow
    {
    public:
        virtual ~Flow() = default;
        virtual Tree &info() = 0;      // metadata tree
        virtual uint16_t *data() = 0;  // raw bayer data (width * height)
    };
    // ============================================================
    // Link - Processing unit
    // ============================================================
    //
    // Modules contribute Links to the pipe.
    // Links have a name and type for identification.

    class Link
    {
    public:
        virtual ~Link() = default;

        virtual std::string name() const = 0;
        // Save link as Json.  If link has links - like the pipe does - then recurse.
        virtual std::string save() = 0;
        // Load link from Json.  If link has links - like the pipe does - then recurse.
        virtual void load(std::string json) = 0;
    };

    // The head link; takes the raw arw file bytes, and makes the flow object for the pipe.
    class Head : public Link
    {
    public:
        virtual std::unique_ptr<Flow> decode(const uint8_t *bytes, size_t size) = 0;
    };

    // ============================================================================
    // Factory & Utilities
    // ============================================================================

    // Create a Tree
    std::unique_ptr<Tree> makeTree();

    // Create a Head (Sony ARW decoder)
    std::unique_ptr<Head> makeHead();

    // Format types
    enum Swap
    {
        JPG, // JPEG compressed
        PNG, // PNG compressed
        BIN, // RGB uint8 (w*h*3 bytes)
        LIN  // RGB float linear (w*h*3 floats, scene-referred)
    };

    // Universal format conversion:
    //   from BIN/LIN: w=width, h=height
    //   from PNG/JPG: w=byte_size, h=ignored
    std::vector<uint8_t> swap(const void *data, int w, int h, Swap from, Swap into);
    // Swap tree into json

}
