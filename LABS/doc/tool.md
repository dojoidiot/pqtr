# Tool - File I/O Utilities

[back](../README.md)

## Purpose

`tool.hpp` provides filesystem I/O utilities that bridge between files and Sinks. It is the **only** place in LABS where file operations occur, maintaining a clean separation of concerns.

## Architecture Principle: Separation of I/O

**PIMPL Rule**: All file I/O is isolated in `tool.hpp`. No other component directly touches the filesystem.

**Design Pattern:**
```
Filesystem ← Tool → Sink ← Pipe → Data Processing
```

- **Tool**: File I/O gateway (reads files → Sinks, writes Sinks → files)
- **Sink**: Universal byte buffer (no filesystem knowledge)
- **Pipe**: Processing pipeline (no filesystem knowledge)

This separation allows:
- **Testability**: Mock Sinks for unit tests without touching disk
- **Flexibility**: Async I/O, streaming, network sources handled by caller
- **Portability**: Platform-specific I/O isolated to Tool

---

## API Reference

### Tool::read()

```cpp
static Sink* read(const std::string& filename)
```

**Purpose**: Load an entire file into a Sink.

**Parameters:**
- `filename`: Path to file (relative or absolute)

**Returns**: Pointer to Sink containing file contents (caller owns, must delete)

**Throws**: `std::runtime_error` if file cannot be opened

**Example:**
```cpp
#include <tool.hpp>

pqtr::Sink* rawData = pqtr::Tool::read("image.ARW");
// Use rawData...
delete rawData;
```

**Implementation Notes:**
- Reads file in 4KB chunks
- Each byte stored as individual `char*` in Sink circular buffer
- Caller responsible for deleting returned Sink

---

### Tool::save()

```cpp
static void save(Sink& sink, const std::string& filename)
```

**Purpose**: Write Sink contents to a file.

**Parameters:**
- `sink`: Sink containing data to write
- `filename`: Output file path

**Throws**: `std::runtime_error` if file cannot be opened for writing

**Example:**
```cpp
#include <tool.hpp>

pqtr::Sink outputData;
// Fill outputData...
pqtr::Tool::save(outputData, "output.png");
```

**Implementation Notes:**
- Takes bytes from sink HEAD (FIFO order)
- Sink is emptied during save process
- Creates file if it doesn't exist, overwrites if it does

---

## Integration with LABS Components

### HEAD: RAW Decoding

```cpp
// In labs.cpp or pipe HEAD implementation
pqtr::Sink* rawSink = pqtr::Tool::read("image.ARW");
decoder.decode(*rawSink, outputImage, metadata);
delete rawSink;
```

The decoder reads from Sink, never directly from filesystem.

### TAIL: Output Writing

```cpp
// Convert processed image to PNG bytes in Sink
pqtr::Sink outputSink;
encodePNG(processedImage, outputSink);

// Write to file
pqtr::Tool::save(outputSink, "output.png");
```

The encoder writes to Sink, Tool handles filesystem.

### pipe.hpp Contract

```cpp
virtual pqtr::Hold<Head> open(pqtr::Hold<pqtr::Sink> sink) = 0;
```

Pipe never touches files - it only works with Sinks. The caller (e.g., `labs` executable) uses Tool to load files into Sinks.

---

## Why No Link?

**Previous Design (Deleted)**:
```cpp
// WRONG: Too many abstractions
FILE* → Link → Sink → Decoder
```

**Current Design**:
```cpp
// RIGHT: Single I/O abstraction
Tool::read() → Sink → Decoder
```

**Rationale:**
- Link was redundant - Sink already provides the buffer abstraction
- Async I/O happens **before** Sink creation (caller's responsibility)
- Sink is sufficient for all I/O needs (streaming, buffering, etc.)

---

## Example: Full Pipeline

```cpp
#include <tool.hpp>
#include <pipe.hpp>

int main() {
    // 1. Load RAW into Sink
    pqtr::Sink* rawSink = pqtr::Tool::read("input.ARW");

    // 2. Process through pipe
    pipe::Pipe pipe;
    auto head = pipe.open(pqtr::Hold<pqtr::Sink>(rawSink));
    auto body = head->body();
    // ... add processing links ...
    auto tail = body.tail();
    tail->save();  // Internally uses Sink

    // 3. Tool handles final output
    // (Tail populates output Sink, Tool writes to file)

    return 0;
}
```

---

## Design Constraints

**Rule**: Only `tool.hpp` may use:
- `<fstream>`
- `fopen()`, `fread()`, `fwrite()`
- Any filesystem operations

**Enforcement**: Code reviews and documentation emphasize this constraint.

---

## Future Extensions

### Async I/O

```cpp
// Caller handles async, Tool remains synchronous
std::future<pqtr::Sink*> asyncRead = std::async([]() {
    return pqtr::Tool::read("image.ARW");
});

pqtr::Sink* result = asyncRead.get();
```

### Network Streams

```cpp
// Caller populates Sink from network
pqtr::Sink* networkData = new pqtr::Sink();
while (socket.has_data()) {
    char* byte = new char;
    *byte = socket.read_byte();
    networkData->push(byte);
}

// Decoder doesn't know/care about source
decoder.decode(*networkData, output, metadata);
```

### Memory-Mapped Files

```cpp
// Future: Add Tool::mmap() for large files
pqtr::Sink* mmapData = pqtr::Tool::mmap("huge.ARW");
```

---

## Summary

**tool.hpp** is the **single source of truth** for filesystem I/O in LABS. It maintains clean architecture by:

1. ✅ Isolating all file operations in one place
2. ✅ Providing simple API (`read()`, `save()`)
3. ✅ Working with universal Sink abstraction
4. ✅ Enabling testability and flexibility
5. ✅ Following PIMPL principle (no filesystem knowledge leaks to other components)

[back](../README.md)
