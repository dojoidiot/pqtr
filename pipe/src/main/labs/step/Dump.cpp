// logs.cpp - labs::Step that prints Flow JSON for debugging
//
// Usage: pipe->join("dump", std::make_unique<Dump>());

#include "../../../../inc/labs.hpp"
#include <iostream>
#include <memory>

using namespace pqtr;

class Dump : public Step
{
public:
    void* exec(Flow& flow) override
    {
        std::cout << "=== Flow JSON ===\n";
        std::cout << flow.json() << "\n";
        std::cout << "=================\n";
        return flow.data();
    }
};

// Factory function
std::unique_ptr<Step> makeDump()
{
    return std::make_unique<Dump>();
}
