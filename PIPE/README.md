# PIPE

Processing pipeline for PQTR. Link-based architecture where projects contribute processing units.

## How PIPE Works

PIPE is a chain of Links. Data flows through each Link in sequence, transforming as it goes.

### Data Flow

```
                        ┌─────────────────────────────────────────────────────────┐
                        │                         PIPE                             │
                        │                                                          │
  RAW file ──►  Data ──►│──► Link ──► Link ──► Link ──► Link ──► Link ──►│──► Data ──► Output
               (in)     │    GEAR     WGPU     LUTE     VIBE     WGPU    │    (out)
                        │    decode   open     profile  style    shut    │
                        │                                                          │
                        │    Page:    Page:    Page:    Page:    Page:            │
                        │    raw buf  Bayer*   Ctx*     Ctx*     Ctx*    Output*  │
                        │                                                          │
                        │    Info accumulates metadata through each link ──────►  │
                        └─────────────────────────────────────────────────────────┘
```

### Core Concept

Each Link:
1. Receives `Data` (Page + Info)
2. Processes the Page (transforms buffer)
3. Enriches Info (adds metadata)
4. Returns new `Data`

```cpp
Data out = link.flow(in);  // Page transformed, Info accumulated
```

### Page vs Info

| Component | What it is | How it changes |
|-----------|------------|----------------|
| **Page** | Buffer pointer (`void*`) | Replaced at each Link (raw → Bayer → GPU → output) |
| **Info** | Metadata tree (Node) | Accumulated - each Link adds its metadata |

Info is a tree of key-value pairs (dials, text, arrays) that travels with the image through the pipeline.

## Model

```
Data = Page (buffer) + Info (metadata)
Link = processing unit with flow(Data) → Data
Pipe = chain of Links
```

**Page** flows through as buffer pointer (CPU or GPU).
**Info** accumulates metadata as tree structure.

## Link Contributions

| Project | Link | Input Page | Output Page |
|---------|------|------------|-------------|
| GEAR | `gear::link()` | raw file buffer | BayerBuffer* |
| WGPU | `wgpu::open()` | BayerBuffer* | Context* (GPU) |
| LUTE | `lute::tune()` | Context* | Context* (learns profile) |
| LUTE | `lute::view()` | Context* | Context* (applies profile) |
| VIBE | `vibe::tune()` | Context* | Context* (learns style) |
| VIBE | `vibe::view()` | Context* | Context* (applies style) |
| WGPU | `wgpu::shut()` | Context* | OutputBuffer* |

## Pipe Configurations

### tune pipe - Learning

```cpp
auto tune = pipe::make();
tune->link(gear::link());    // raw → Bayer (CPU)
tune->link(wgpu::open());    // Bayer → GPU
tune->link(lute::tune());    // learn camera profile
tune->link(vibe::tune());    // learn style
tune->link(wgpu::shut());    // GPU → output (CPU)
```

Outputs learned parameters in Info.

### view pipe - Production

```cpp
auto view = pipe::make();
view->link(gear::link());    // raw → Bayer (CPU)
view->link(wgpu::open());    // Bayer → GPU
view->link(lute::view());    // apply camera profile
view->link(vibe::view());    // apply style
view->link(wgpu::shut());    // GPU → output (CPU)
```

Outputs processed pixels.

## API

### pipe.hpp

```cpp
namespace pipe {
    using Page = void*;       // Buffer pointer
    using Info = Node;        // Metadata tree
    using Name = std::string;

    class Data {
        Page page;
        Info info;
    };

    class Link {
        virtual Name name() const = 0;
        virtual Name type() const = 0;
        virtual Data flow(Data in) = 0;
    };

    class Pipe {
        virtual Pipe& link(Hold<Link> link) = 0;
        virtual Data flow(Data in) = 0;
        virtual Link* find(const Name& name) = 0;
    };

    Hold<Pipe> make();
}
```

### Info (Node) Methods

```cpp
info.dial("key", 1.5f);           // set float
float v = info.dial("key");       // get float

info.text("key", "value");        // set string
Name s = info.text("key");        // get string

info.data("key", floats, count);  // set array
const float* a = info.data("key");// get array

Node& child = info.make("child"); // create child node
Node* found = info.find("child"); // find child node

Name json = info.save();          // serialize
info.load(json);                  // deserialize
```

## Contributing a Link

```cpp
// myproject.hpp
namespace myproject {
    pipe::Hold<pipe::Link> link();
}

// myproject.cpp
class MyLink : public pipe::Link {
public:
    pipe::Name name() const override { return "myproject"; }
    pipe::Name type() const override { return "process"; }

    pipe::Data flow(pipe::Data in) override {
        // Get input
        auto* input = static_cast<InputType*>(in.page);

        // Process
        auto* output = process(input);

        // Return
        pipe::Data out;
        out.page = output;
        out.info = std::move(in.info);
        out.info.text("processed_by", "myproject");
        return out;
    }
};

pipe::Hold<pipe::Link> link() {
    return pipe::Hold<pipe::Link>(new MyLink());
}
```

## Structure

```
PIPE/
├── inc/pipe.hpp       # Public API
├── src/main/
│   └── pipe.cpp       # Pipe implementation
├── src/test/
│   └── pipe.cpp       # Tests
└── README.md
```

## Building

```bash
g++ -std=c++17 -I./inc src/main/pipe.cpp src/main/part/pipe/node.cpp -c
```

## See Also

- [GEAR](../GEAR/README.md) - `gear::link()` RAW decoder
- [WGPU](../WGPU/README.md) - `wgpu::open()/shut()` GPU lifecycle
- [LUTE](../LUTE/README.md) - `lute::tune()/view()` camera profiles
- [VIBE](../VIBE/README.md) - `vibe::tune()/view()` style processing
