#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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
        virtual Stem &head() = 0;                // the head information
        virtual Stem &flow() = 0;                // the flow step information
        virtual Stem &tail() = 0;                // the tail information
        virtual void *data() = 0;                // current flow data - width/height are in info tree
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
        // Tail type needs to be one of PNG or JPG
        virtual void join(std::unique_ptr<Head> head, std::unique_ptr<Tail> tail) = 0;

        // join a module step to the body step list.  Module name is the module params tree stem name
        virtual void join(std::string name, std::unique_ptr<Step> step) = 0;

        // pump the pipe from head to tail through the step body.
        virtual void *pump(void *data, size_t size) = 0;
    };

    // Make a pipe.
    std::unique_ptr<Pipe> make();

    // Make a flow.
    std::unique_ptr<Flow> makeFlow();
}
