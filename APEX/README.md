# APEX

Core infrastructure library for PQTR projects.

## Provides

| Component | Description |
|-----------|-------------|
| `pqtr::Hold<T>` | Smart pointer (header-only) |
| `pqtr::Sink` | Write-many, read-many buffer |
| `pqtr::sink()` | Factory to create Sink |

## Usage

```cpp
#include <hold.hpp>
#include <sink.hpp>

// Create a sink and push data
auto s = pqtr::sink();
char* data = new char[100];
s->push(data, 100);

// Read back
char* out;
int n = s->take(out, 50);
delete[] out;
```

## Build

```bash
make        # Build lib/apex.a
make tidy   # Clean
```

## Include

```makefile
INCLUDES = -I./inc/APEX
LIBS = ./lib/APEX/apex.a
```
