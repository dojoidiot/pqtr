# PQTR Plan

## Rules

- pipe/ is read-only - link to it, never modify
- labs.hpp is minimal - declarations only, PIMPL
- Plug classes declared in labs.hpp, implemented in src/main/plug/*.cpp
- Only pqtr::pipe() factory - plugs use std::make_unique<>()
- Use extern "C" {} for pipe/ function declarations
- Unit tests (test-unit): no pipe/ linkage
- Integration tests (test-int): can link pipe/

## Pipe Model

Only `pqtr::pipe()` is a factory. Plugs use `std::make_unique<>()`.

Heads, bodies, and tails are all chained in order:

```cpp
auto pipe = pqtr::pipe();
pipe->head(std::make_unique<pqtr::SonyHead>())
    .body("step-1", std::make_unique<MyStep>())
    .tail(std::make_unique<pqtr::JsonTail>("out.json"))
    .tail(std::make_unique<pqtr::PngTail>("out.png"));

pipe->pump(data, size);
```

On each pump, a new Flow is created and passed through all heads → bodies → tails.

## Step 1 - Mock pipe test (DONE)

Unit test with no pipe/ linkage: `src/test/unit/pipe.cpp`

Run: `make test-unit`

## Step 2 - Sony head metadata (DONE)

Integration test: `src/test/labs/labs.cpp`

```cpp
auto pipe = pqtr::pipe();
pipe->head(std::make_unique<pqtr::SonyHead>())
    .tail(std::make_unique<pqtr::JsonTail>("tmp/var/labs.json"));

pipe->pump((void*)arw_filename, strlen(arw_filename));
```

Head populates flow with camera metadata for future Step adapters:
- width, height, black, white, filters, exposure_bias
- wb (r, g1, b, g2), d65 coeffs
- color_matrix, xyz_to_cam, sony_curve

Run: `make test-int` → saves `tmp/var/labs.json`
