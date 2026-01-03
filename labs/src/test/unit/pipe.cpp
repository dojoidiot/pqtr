// Unit test - Mock pipe test (no pipe/ linkage)

#include "labs.hpp"
#include <iostream>
#include <cstring>

namespace {

// MockHead - creates flow with test data
class MockHead : public pqtr::Head {
public:
    std::unique_ptr<pqtr::Flow> load(pqtr::Flow& flow, const void* bytes, size_t size) override {
        std::string name = "mock-image";
        flow.head().leaf(pqtr::NAME).text(name);
        flow.head().leaf(pqtr::WIDTH).dial(100);
        flow.head().leaf(pqtr::HEIGHT).dial(100);

        std::cout << "MockHead: loaded " << size << " bytes\n";
        (void)bytes;
        return nullptr;
    }
};

// MockStep - logs execution
class MockStep : public pqtr::Step {
    std::string id_;
public:
    explicit MockStep(const std::string& id) : id_(id) {}

    void* exec(pqtr::Flow& flow) override {
        std::cout << "MockStep[" << id_ << "]: exec\n";
        flow.flow().next(id_).leaf("done").dial(1);
        return flow.data();
    }
};

// MockTail - outputs result
class MockTail : public pqtr::Tail {
public:
    void* save(pqtr::Flow& flow) override {
        std::cout << "MockTail: save\n";
        std::cout << "Flow JSON:\n" << flow.json() << "\n";
        return nullptr;
    }
};

}  // namespace

int main() {
    std::cout << "=== unit test: pipe ===\n\n";

    auto pipe = pqtr::pipe();
    pipe->head(std::make_unique<MockHead>())
        .body("mock-step-1", std::make_unique<MockStep>("1"))
        .body("mock-step-2", std::make_unique<MockStep>("2"))
        .tail(std::make_unique<MockTail>());

    const char* data = "test-data";
    pipe->pump((void*)data, strlen(data));

    std::cout << "\n=== pass ===\n";
    return 0;
}
