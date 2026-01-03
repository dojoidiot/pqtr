#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// PipeState from pipe/ for execution
extern "C" {
#include "../../pipe/src/main/labs/pipe_state.h"
}

/**
 * This is the labs namespace.  This is designed for me to read - it must be maintained as minimal interface,
 * PIMPL.  No boilerplate, low cognitive load.
 *
 * DO NOT ADD CODE HERE.
 *
 * DO NOT CHANGE THIS COMMENT.
 */
namespace pqtr
{

    // common fields
    constexpr const char *WIDTH = "width";
    constexpr const char *HEIGHT = "height";

    // ============================================================================
    // Head Field Keys
    // ============================================================================

    constexpr const char *NAME = "name";
    constexpr const char *BLACK = "black";
    constexpr const char *WHITE = "white";

    // ============================================================================
    // Tail Field Keys
    // ============================================================================
    constexpr const char *PNG = "png";
    constexpr const char *JPG = "jpg";

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

    // ============================================================================
    // Flow - Flow data container
    // ============================================================================
    //
    class Flow
    {
    public:
        virtual ~Flow() = default;
        virtual PipeState &state() = 0;          // execution state (C struct)
        virtual Stem &head() = 0;                // persistence (tree)
        virtual Stem &flow() = 0;                // persistence (tree)
        virtual Stem &tail() = 0;                // persistence (tree)
        virtual void *data() = 0;                // current flow data
        virtual void resize(size_t bytes) = 0;   // resize data buffer
        virtual std::string json() = 0;          // serialize to JSON
        virtual void read(std::string json) = 0; // deserialize from JSON
    };
    // ============================================================
    // Link - Processing unit
    // ============================================================
    //
    // Modules contribute Links to the pipe.
    // Links have a name and type for identification.

    class Link
    {
    };

    // The head link; takes the raw arw file bytes, and makes the flow object for the pipe, filling the tree, and setting the Flow data.
    class Head : public Link
    {
    public:
        virtual std::unique_ptr<Flow> load(Flow &flow, const void *bytes, size_t size) = 0;
    };

    // Save the tail
    class Tail : public Link
    {
    public:
        virtual void *save(Flow &flow) = 0;
    };

    // A step - modules will be steps that adapt the copied code to the pipe model.
    class Step : public Link
    {
    public:
        virtual void *exec(Flow &flow) = 0;
    };

    class Pipe
    {
    public:
        virtual ~Pipe() = default;

        // Fluent builder API
        virtual Pipe& head(std::unique_ptr<Head> head) = 0;
        virtual Pipe& body(std::string name, std::unique_ptr<Step> step) = 0;
        virtual Pipe& tail(std::unique_ptr<Tail> tail) = 0;

        // pump the pipe from head to tail through the step body.
        virtual void *pump(void *data, size_t size) = 0;
    };

    // Make a pipe.
    std::unique_ptr<Pipe> pipe();

    // ============================================================================
    // Plugs - Heads, Tails (use std::make_unique to create)
    // ============================================================================

    // Sony ARW head - loads metadata and raw data from ARW file
    class SonyHead : public Head
    {
    public:
        std::unique_ptr<Flow> load(Flow &flow, const void *bytes, size_t size) override;
    };

    // JSON tail - saves flow metadata to JSON file
    class JsonTail : public Tail
    {
        std::string path_;
    public:
        explicit JsonTail(const std::string &path) : path_(path) {}
        void *save(Flow &flow) override;
    };

    // PNG tail - saves flow data as PNG
    class PngTail : public Tail
    {
        std::string path_;
    public:
        explicit PngTail(const std::string &path) : path_(path) {}
        void *save(Flow &flow) override;
    };

    // ============================================================================
    // Steps - Module adapters (use std::make_unique to create)
    // ============================================================================

    // rawprepare: uint16 bayer → float bayer (normalized)
    class RawprepareStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // temperature: white balance on bayer
    class TemperatureStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // highlights: highlight recovery
    class HighlightsStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // demosaic: bayer → RGB
    class DemosaicStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // exposure: exposure compensation
    class ExposureStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // colorin: Camera RGB → Rec2020
    class ColorinStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // channelmixerrgb: chromatic adaptation
    class ChannelMixerStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // colorbalancergb: color grading
    class ColorBalanceStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // filmicrgb: tone mapping
    class FilmicStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // bilat: local contrast
    class BilatStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // colorout: Rec2020 → sRGB
    class ColoroutStep : public Step
    {
    public:
        void *exec(Flow &flow) override;
    };

    // dump: write buffer to binary file for comparison
    class DumpStep : public Step
    {
        std::string path_;
        size_t elem_size_;  // bytes per element (4 for float, 1 for bayer float)
        int channels_;      // 1 for bayer, 4 for RGBA
    public:
        // elem_size: 4 for float, channels: 1 for bayer, 4 for RGBA
        DumpStep(const std::string &path, int channels = 4)
            : path_(path), elem_size_(sizeof(float)), channels_(channels) {}
        void *exec(Flow &flow) override;
    };
}
