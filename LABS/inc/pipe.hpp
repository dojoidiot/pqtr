// pipe.hpp
// Public API for the processing pipe
//
// Modules contribute Links to the pipe:
//   GEAR - decode RAW to flat
//   LUTE - camera profile LUT
//   VIBE - style dials
//
// User apps include only this header and link against pipe.a.
// Implementation details are hidden (PIMPL pattern).

#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

namespace pipe
{
    // ============================================================
    // Type Aliases
    // ============================================================

    using Size = size_t;      // flow sizes.
    using Name = std::string; // Identifier

    template <typename T>
    using List = std::vector<T>;

    template <typename T>
    using Hold = std::unique_ptr<T>;

    template <typename K, typename V>
    using Dict = std::map<K, V>;

    using Data = void *;
   

    // ============================================================
    // Node - Tree-structured metadata
    // ============================================================

    class Node
    {
    public:
        Node();
        explicit Node(const Name &name);
        ~Node();

        Node(Node &&);
        Node &operator=(Node &&);
        Node(const Node &) = delete;
        Node &operator=(const Node &) = delete;

        Name name() const;

        // Tree management
        List<Node *> list();
        List<const Node *> list() const;
        Node &make(const Name &tag);
        Node *find(const Name &tag);
        const Node *find(const Name &tag) const;
        Size size(const Name &key) const;
        bool test(const Name &key) const;
        void tidy();

        // Setters/Getters
        void dial(const Name &key, float value);
        float dial(const Name &key) const;

        void text(const Name &key, const Name &value);
        Name text(const Name &key) const;

        void data(const Name &key, const float *values, Size size);
        const float *data(const Name &key) const;

        // JSON persistence
        Name save() const;           // Serialize to JSON string
        bool load(const Name &json); // Deserialize from JSON string

    private:
        struct Impl;
        Hold<Impl> m_impl;
    };

    // ============================================================
    // Flow - Image + Metadata + GPU context
    // ============================================================

    using Info = Node;

    class Flow
    {
    public:
        Data data; // GPU context (optional)
        Info info; // Metadata

        Flow();
        Flow(Data data, Info info);
        ~Flow();

        Flow(Flow &&);
        Flow &operator=(Flow &&);
        Flow(const Flow &) = delete;
        Flow &operator=(const Flow &) = delete;
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

        virtual Name name() const = 0;

        virtual Flow flow(Flow in) = 0;
    };

    // ============================================================
    // Pipe - Link chain
    // ============================================================
    //
    // Example:
    //   auto pipe = pipe::make();
    //   pipe->link(gear::read());
    //   pipe->link(vibe::link("style1"));
    //   Flow out = pipe->flow(in);

    class Pipe
    {
    public:
        virtual ~Pipe() = default;

        virtual Pipe &link(Hold<Link> link) = 0;

        virtual Flow flow(Flow in) = 0;

        virtual Size size() const = 0;

        virtual Link &link(Size i) = 0;
    };

    Hold<Pipe> make();

    // ============================================================
    // Core Pipeline Links (RAW processing)
    // ============================================================

    Hold<Link> blc();      // Black level correction
    Hold<Link> wb();       // White balance (Bayer domain)
    Hold<Link> demosaic(); // Bayer → RGB
    Hold<Link> cst();      // Color space transform (matrix)
    Hold<Link> crop();     // Active area crop

    // ============================================================
    // View Link - PNG snapshot pushed to BASE
    // ============================================================
    //
    // Generates PNG on GPU and pushes to BASE server.
    // Runs in browser WASM - no filesystem access.
    //
    // Reads from Info:
    //   "name" - base filename (e.g., "DSC00144")
    //   "jwt"  - auth token for BASE push
    //
    // Writes to Info:
    //   "view" - filename pushed (e.g., "DSC00144.png")
    //
    // Example:
    //   pipe->link(pipe::view());  // → pushes {name}.png to BASE
    //
    // The fidelity rule: view quantizes to 8-bit for POST only.
    // Pipeline data (Page) passes through unchanged at full precision.

    Hold<Link> view();

    // ============================================================
    // Utility - Image encoding/decoding
    // ============================================================

    std::vector<uint8_t> encodePng(const uint8_t *rgb, int width, int height);
    std::vector<uint8_t> encodeJpeg(const uint8_t *rgb, int width, int height, int quality = 85);

    struct ImageResult
    {
        int width;
        int height;
        std::vector<uint8_t> rgb; // RGB8, empty on failure
    };

    using JpegResult = ImageResult;

    ImageResult decodeJpeg(const uint8_t *data, size_t size);
    ImageResult decodePng(const uint8_t *data, size_t size);

} // namespace pipe
