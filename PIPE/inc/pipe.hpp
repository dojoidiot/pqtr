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

    using Name = std::string; // Identifier

    template <typename T>
    using List = std::vector<T>;

    template <typename T>
    using Hold = std::unique_ptr<T>;

    template <typename K, typename V>
    using Dict = std::map<K, V>;

    using Page = void*;       // GPU context (wgpu::Device* when using WebGPU)

    // ============================================================
    // Node - Tree-structured metadata
    // ============================================================

    class Node
    {
    public:
        Node();
        explicit Node(const Name &tag);
        ~Node();

        Node(Node &&);
        Node &operator=(Node &&);
        Node(const Node &) = delete;
        Node &operator=(const Node &) = delete;

        Name tag() const;

        List<Node *> list();
        List<const Node *> list() const;
        Node &make(const Name &tag);
        Node *find(const Name &tag);
        const Node *find(const Name &tag) const;

        void dial(const Name &key, float value);
        float dial(const Name &key) const;

        void text(const Name &key, const Name &value);
        Name text(const Name &key) const;

        void data(const Name &key, const float *values, size_t size);
        const float *data(const Name &key) const;
        size_t size(const Name &key) const;

        bool test(const Name &key) const;
        void tidy();

        // JSON persistence
        Name save() const;           // Serialize to JSON string
        bool load(const Name &json); // Deserialize from JSON string

    private:
        struct Impl;
        Hold<Impl> m_impl;
    };

    // ============================================================
    // Data - Image + Metadata + GPU context
    // ============================================================

    using Info = Node;

    class Data
    {
    public:
        Page page;  // GPU context (optional)
        Info info;  // Metadata

        Data();
        Data(Page p, Info i);
        ~Data();

        Data(Data &&);
        Data &operator=(Data &&);
        Data(const Data &) = delete;
        Data &operator=(const Data &) = delete;
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
        virtual Name type() const = 0;

        virtual Data flow(Data in) = 0;
    };

    // ============================================================
    // Pipe - Link chain
    // ============================================================
    //
    // Example:
    //   auto pipe = pipe::make();
    //   pipe->link(gear::link());
    //   pipe->link(vibe::link("style1"));
    //   Data out = pipe->flow(in);

    class Pipe
    {
    public:
        virtual ~Pipe() = default;

        virtual Pipe &link(Hold<Link> link) = 0;
        virtual Data flow(Data in) = 0;

        virtual size_t size() const = 0;
        virtual Link &link(size_t i) = 0;

        virtual Link *find(const Name &name) = 0;
        virtual List<Link *> type(const Name &type) = 0;
    };

    Hold<Pipe> make();

} // namespace pipe
