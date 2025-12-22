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
    // GPU processing types
    // ============================================================================

    // Final CPU result
    struct Done
    {
        std::vector<float> rgb; // interleaved RGB, linear scene-referred
        int width = 0;
        int height = 0;
    };

    // GPU processing job (PIMPL)
    struct TaskImpl;
    class Task
    {
    public:
        Task(TaskImpl *impl);
        ~Task();
        Task(Task &&other) noexcept;
        Task &operator=(Task &&other) noexcept;
        Task(const Task &) = delete;
        Task &operator=(const Task &) = delete;

        void post();        // dispatch GPU work
        Done done();        // read back result (call after post)
        Done diff();        // spectral diff (HEAD vs JPEG, same size as JPEG)
        void *buff() const; // GPU buffer (valid after post)
        int width() const;
        int height() const;

    private:
        TaskImpl *impl_;
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

        // GPU processing
        virtual Task head(void *device) = 0;  // RAW → scene-linear RGB
        virtual Task tune(void *device) = 0;  // head + learn camera profile
    };

    // ============================================================================
    // Factory & Utilities
    // ============================================================================

    // Load RAW file into Flow container
    std::unique_ptr<Flow> make(std::string name, uint16_t *bits, size_t size);

    // Format types
    enum Swap
    {
        JPG,  // JPEG compressed
        PNG,  // PNG compressed
        BIN,  // RGB uint8 (w*h*3 bytes)
        LIN   // RGB float linear (w*h*3 floats, scene-referred)
    };

    // Universal format conversion:
    //   from BIN/LIN: w=width, h=height
    //   from PNG/JPG: w=byte_size, h=ignored
    std::vector<uint8_t> swap(const void *data, int w, int h, Swap from, Swap into);

    // Copy XMP sidecar settings to Tree (under "vibe" node)
    bool copy(const char *xmp_data, size_t xmp_size, Tree &info);

    // Apply vibe settings to Done result (in-place)
    // stages: 1=exposure only, 2=+tonemap, 3=+color (default)
    bool vibe(Done &img, Stem &vibeNode, int stages = 3);

    // Learn optimal VIBE parameters from reference JPEG
    // Writes optimal params to vibeNode: colorCorrection.exposure, toneMapping.*, globalColor.*
    bool tune(Done &scene_linear,
              const uint8_t *ref_jpg, size_t ref_jpg_size,
              int ref_width, int ref_height, int orientation,
              Stem &vibeNode,
              int max_iters = 100);

}
