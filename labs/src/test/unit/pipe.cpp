// Unit test - Mock pipe test (no pipe/ linkage)

#include "pqtr.hpp"
#include <iostream>
#include <cstring>

using namespace pqtr::Labs;

namespace {

// MockHead - creates flow with test data
class MockHead : public Head {
public:
    std::unique_ptr<Flow> load(Flow& flow, const void* bytes, size_t size) override {
        std::string name = "mock-image";
        flow.head().leaf(NAME).text(name);
        flow.head().leaf(WIDTH).dial(100);
        flow.head().leaf(HEIGHT).dial(100);

        std::cout << "MockHead: loaded " << size << " bytes\n";
        (void)bytes;
        return nullptr;
    }
};

// MockStep - logs execution
class MockStep : public Step {
    std::string id_;
public:
    explicit MockStep(const std::string& id) : id_(id) {}

    void* exec(Flow& flow) override {
        std::cout << "MockStep[" << id_ << "]: exec\n";
        flow.flow().next(id_).leaf("done").dial(1);
        return flow.data();
    }
};

// MockTail - outputs result
class MockTail : public Tail {
public:
    void* save(Flow& flow) override {
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
