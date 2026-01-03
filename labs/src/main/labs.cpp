// labs.cpp - Labs CLI (skeleton)
//
// Future: GPU shaders, CMA-ES tuning
// Decoders are in pipe/src/main/labs/sony.c (pure C)

#include "pqtr.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    std::cout << "labs - image processing laboratory\n";
    std::cout << "\n";
    std::cout << "Decoders: ../pipe/src/main/labs/sony.c\n";
    std::cout << "Modules:  ../pipe/src/main/labs/mods/*.c\n";
    std::cout << "\n";
    std::cout << "Future:\n";
    std::cout << "  - WGSL GPU shaders\n";
    std::cout << "  - CMA-ES parameter tuning\n";

    (void)argc;
    (void)argv;
    return 0;
}
