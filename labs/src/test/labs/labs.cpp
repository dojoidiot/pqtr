// Integration test - Sony ARW head test

#include "pqtr.hpp"
#include <iostream>
#include <cstring>

namespace pqtr::Labs {
    std::unique_ptr<Head> sonyHead();
    std::unique_ptr<Tail> jsonTail(const std::string &path);
}

using namespace pqtr::Labs;

int main() {
    std::cout << "=== integration test: sony ===\n\n";

    auto pipe = pqtr::pipe();
    pipe->head(sonyHead())
        .tail(jsonTail("tmp/var/labs.json"));

    // Pass ARW filename to pump
    const char* arw = "src/test/raws/sony.ARW";
    pipe->pump((void*)arw, strlen(arw));

    std::cout << "\n=== pass ===\n";
    return 0;
}
