// Integration test - Sony ARW head test

#include "labs.hpp"
#include <iostream>
#include <cstring>

int main() {
    std::cout << "=== integration test: sony ===\n\n";

    auto pipe = pqtr::pipe();
    pipe->head(std::make_unique<pqtr::SonyHead>())
        .tail(std::make_unique<pqtr::JsonTail>("tmp/var/labs.json"));

    // Pass ARW filename to pump
    const char* arw = "src/test/raws/sony.ARW";
    pipe->pump((void*)arw, strlen(arw));

    std::cout << "\n=== pass ===\n";
    return 0;
}
