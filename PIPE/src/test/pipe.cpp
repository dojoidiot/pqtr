// test_pipe.cpp - Tests for simplified pipe API
//
// Tests:
//   1. Node JSON persistence (save/load)
//   2. Simple Link contribution
//   3. Pipe flow with Page context

#include <pipe.hpp>
#include <iostream>
#include <cmath>

using namespace pipe;

// ============================================================
// Simple test Link - records scale dial in metadata
// ============================================================

class ScaleLink : public Link {
    Name m_name;
    float m_scale;
public:
    ScaleLink(const Name& name, float scale)
        : m_name(name), m_scale(scale) {}

    Name name() const override { return m_name; }
    Name type() const override { return "scale"; }

    Data flow(Data in) override {
        // Page available: in.page (GPU context)
        in.info.dial("scale", m_scale);
        return in;
    }
};

// ============================================================
// Test Node JSON persistence
// ============================================================

bool test_node_json() {
    std::cout << "=== Test: Node JSON ===" << std::endl;

    // Build a tree
    Node root("root");
    root.dial("exposure", 1.5f);
    root.dial("contrast", 0.8f);
    root.text("camera", "Sony A7R");
    root.text("lens", "24-70mm");

    // Add array data
    float curve[] = {0.0f, 0.1f, 0.3f, 0.6f, 1.0f};
    root.data("curve", curve, 5);

    // Add child node
    Node& wb = root.make("white_balance");
    wb.dial("red", 1.2f);
    wb.dial("green", 1.0f);
    wb.dial("blue", 0.9f);

    // Add nested child
    Node& tint = wb.make("tint");
    tint.dial("amount", 0.05f);

    // Serialize
    Name json = root.save();
    std::cout << "  Saved: " << json.substr(0, 80) << "..." << std::endl;

    // Deserialize into new node
    Node loaded;
    if (!loaded.load(json)) {
        std::cout << "  FAIL: load() returned false" << std::endl;
        return false;
    }

    // Verify dials
    if (std::abs(loaded.dial("exposure") - 1.5f) > 0.001f) {
        std::cout << "  FAIL: exposure mismatch" << std::endl;
        return false;
    }
    if (std::abs(loaded.dial("contrast") - 0.8f) > 0.001f) {
        std::cout << "  FAIL: contrast mismatch" << std::endl;
        return false;
    }

    // Verify texts
    if (loaded.text("camera") != "Sony A7R") {
        std::cout << "  FAIL: camera text mismatch" << std::endl;
        return false;
    }

    // Verify array
    const float* loadedCurve = loaded.data("curve");
    size_t curveSize = loaded.size("curve");
    if (curveSize != 5 || !loadedCurve) {
        std::cout << "  FAIL: curve array missing" << std::endl;
        return false;
    }
    if (std::abs(loadedCurve[2] - 0.3f) > 0.001f) {
        std::cout << "  FAIL: curve[2] mismatch" << std::endl;
        return false;
    }

    // Verify child
    Node* wbLoaded = loaded.find("white_balance");
    if (!wbLoaded) {
        std::cout << "  FAIL: white_balance child missing" << std::endl;
        return false;
    }
    if (std::abs(wbLoaded->dial("red") - 1.2f) > 0.001f) {
        std::cout << "  FAIL: wb.red mismatch" << std::endl;
        return false;
    }

    // Verify nested child
    Node* tintLoaded = wbLoaded->find("tint");
    if (!tintLoaded) {
        std::cout << "  FAIL: tint child missing" << std::endl;
        return false;
    }
    if (std::abs(tintLoaded->dial("amount") - 0.05f) > 0.001f) {
        std::cout << "  FAIL: tint.amount mismatch" << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

// ============================================================
// Test Node special characters in JSON
// ============================================================

bool test_node_json_escape() {
    std::cout << "=== Test: Node JSON Escape ===" << std::endl;

    Node root;
    root.text("path", "/home/user/photos/test.ARW");
    root.text("note", "Line1\nLine2\tTabbed");
    root.text("quote", "He said \"hello\"");

    Name json = root.save();
    std::cout << "  JSON: " << json << std::endl;

    Node loaded;
    if (!loaded.load(json)) {
        std::cout << "  FAIL: load() returned false" << std::endl;
        return false;
    }

    if (loaded.text("path") != "/home/user/photos/test.ARW") {
        std::cout << "  FAIL: path mismatch" << std::endl;
        return false;
    }
    if (loaded.text("note") != "Line1\nLine2\tTabbed") {
        std::cout << "  FAIL: note mismatch" << std::endl;
        return false;
    }
    if (loaded.text("quote") != "He said \"hello\"") {
        std::cout << "  FAIL: quote mismatch: " << loaded.text("quote") << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

// ============================================================
// Test Pipe with contributed Links
// ============================================================

bool test_pipe_flow() {
    std::cout << "=== Test: Pipe Flow ===" << std::endl;

    // Create pipe
    Hold<Pipe> p = make();

    // Add links
    p->link(Hold<Link>(new ScaleLink("scale1", 2.0f)));
    p->link(Hold<Link>(new ScaleLink("scale2", 0.5f)));

    if (p->size() != 2) {
        std::cout << "  FAIL: expected 2 links, got " << p->size() << std::endl;
        return false;
    }

    // Check link access
    if (p->link(0).name() != "scale1") {
        std::cout << "  FAIL: link(0) name mismatch" << std::endl;
        return false;
    }

    // Check find
    Link* found = p->find("scale2");
    if (!found || found->name() != "scale2") {
        std::cout << "  FAIL: find(scale2) failed" << std::endl;
        return false;
    }

    // Check type filter
    List<Link*> scales = p->type("scale");
    if (scales.size() != 2) {
        std::cout << "  FAIL: type(scale) should return 2" << std::endl;
        return false;
    }

    // Flow data through with Page context
    Data in;
    in.page = nullptr;  // No GPU for this test
    in.info.dial("input", 1.0f);

    Data out = p->flow(std::move(in));

    // Both links should have added their scale dial
    // (last one wins since they use same key)
    if (std::abs(out.info.dial("scale") - 0.5f) > 0.001f) {
        std::cout << "  FAIL: scale dial should be 0.5 (from scale2)" << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

// ============================================================
// Test Data move semantics with Page
// ============================================================

bool test_data_move() {
    std::cout << "=== Test: Data Move ===" << std::endl;

    Data d1;
    d1.page = (Page)0x1234;  // Fake GPU context
    d1.info.dial("test", 42.0f);
    d1.info.text("name", "original");

    // Move construct
    Data d2 = std::move(d1);

    if (d2.page != (Page)0x1234) {
        std::cout << "  FAIL: move didn't preserve page" << std::endl;
        return false;
    }
    if (std::abs(d2.info.dial("test") - 42.0f) > 0.001f) {
        std::cout << "  FAIL: move didn't preserve dial" << std::endl;
        return false;
    }
    if (d2.info.text("name") != "original") {
        std::cout << "  FAIL: move didn't preserve text" << std::endl;
        return false;
    }

    // Move assign
    Data d3;
    d3 = std::move(d2);

    if (d3.page != (Page)0x1234) {
        std::cout << "  FAIL: move assign didn't preserve page" << std::endl;
        return false;
    }
    if (d3.info.text("name") != "original") {
        std::cout << "  FAIL: move assign didn't preserve text" << std::endl;
        return false;
    }

    std::cout << "  PASS" << std::endl;
    return true;
}

// ============================================================
// Main
// ============================================================

int main() {
    std::cout << "\n=== PIPE Tests ===\n" << std::endl;

    int passed = 0;
    int failed = 0;

    if (test_node_json()) passed++; else failed++;
    if (test_node_json_escape()) passed++; else failed++;
    if (test_pipe_flow()) passed++; else failed++;
    if (test_data_move()) passed++; else failed++;

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    return failed > 0 ? 1 : 0;
}
