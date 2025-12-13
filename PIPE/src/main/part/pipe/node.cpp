// node.cpp - Node, Data implementations

#include <pipe.hpp>
#include <sstream>
#include <iomanip>
#include <cstdlib>

namespace pipe {

// ============================================================
// JSON helpers
// ============================================================

static Name jsonEscape(const Name& s) {
    Name out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

static Name jsonUnescape(const Name& s) {
    Name out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[++i]) {
                case '"':  out += '"'; break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// ============================================================
// Node::Impl - Private implementation
// ============================================================

struct Node::Impl {
    Name tag;
    Dict<Name, float> dials;
    Dict<Name, Name> texts;
    Dict<Name, List<float>> arrays;
    Dict<Name, Hold<Node>> children;

    Impl() = default;
    explicit Impl(const Name& t) : tag(t) {}
};

// ============================================================
// Node - Constructors/Destructor
// ============================================================

Node::Node() : m_impl(Hold<Impl>(new Impl())) {}

Node::Node(const Name& tag) : m_impl(Hold<Impl>(new Impl(tag))) {}

Node::~Node() = default;

Node::Node(Node&&) = default;

Node& Node::operator=(Node&&) = default;

// ============================================================
// Node - Identity
// ============================================================

Name Node::tag() const {
    return m_impl->tag;
}

// ============================================================
// Node - Children
// ============================================================

List<Node*> Node::list() {
    List<Node*> result;
    result.reserve(m_impl->children.size());
    for (auto& [tag, node] : m_impl->children) {
        result.push_back(node.get());
    }
    return result;
}

List<const Node*> Node::list() const {
    List<const Node*> result;
    result.reserve(m_impl->children.size());
    for (const auto& [tag, node] : m_impl->children) {
        result.push_back(node.get());
    }
    return result;
}

Node& Node::make(const Name& tag) {
    auto it = m_impl->children.find(tag);
    if (it != m_impl->children.end()) {
        return *it->second;
    }
    Hold<Node> node(new Node(tag));
    Node& ref = *node;
    m_impl->children[tag] = std::move(node);
    return ref;
}

Node* Node::find(const Name& tag) {
    auto it = m_impl->children.find(tag);
    return it != m_impl->children.end() ? it->second.get() : nullptr;
}

const Node* Node::find(const Name& tag) const {
    auto it = m_impl->children.find(tag);
    return it != m_impl->children.end() ? it->second.get() : nullptr;
}

// ============================================================
// Node - Float values
// ============================================================

void Node::dial(const Name& key, float value) {
    m_impl->dials[key] = value;
}

float Node::dial(const Name& key) const {
    auto it = m_impl->dials.find(key);
    return it != m_impl->dials.end() ? it->second : 0.0f;
}

// ============================================================
// Node - String values
// ============================================================

void Node::text(const Name& key, const Name& value) {
    m_impl->texts[key] = value;
}

Name Node::text(const Name& key) const {
    auto it = m_impl->texts.find(key);
    return it != m_impl->texts.end() ? it->second : "";
}

// ============================================================
// Node - Array values
// ============================================================

void Node::data(const Name& key, const float* values, size_t sz) {
    if (values && sz > 0) {
        m_impl->arrays[key] = List<float>(values, values + sz);
    } else {
        m_impl->arrays.erase(key);
    }
}

const float* Node::data(const Name& key) const {
    auto it = m_impl->arrays.find(key);
    return it != m_impl->arrays.end() && !it->second.empty() ? it->second.data() : nullptr;
}

size_t Node::size(const Name& key) const {
    auto it = m_impl->arrays.find(key);
    return it != m_impl->arrays.end() ? it->second.size() : 0;
}

// ============================================================
// Node - Utilities
// ============================================================

bool Node::test(const Name& key) const {
    return m_impl->dials.count(key) ||
           m_impl->texts.count(key) ||
           m_impl->arrays.count(key) ||
           m_impl->children.count(key);
}

void Node::tidy() {
    m_impl->dials.clear();
    m_impl->texts.clear();
    m_impl->arrays.clear();
    m_impl->children.clear();
}

// ============================================================
// Data
// ============================================================

Data::Data() : page(nullptr) {}

Data::Data(Page p, Info i) : page(p), info(std::move(i)) {}

Data::~Data() = default;

Data::Data(Data&&) = default;

Data& Data::operator=(Data&&) = default;

// ============================================================
// Node - JSON persistence
// ============================================================

Name Node::save() const {
    std::ostringstream ss;
    ss << "{";

    bool first = true;

    // Tag
    if (!m_impl->tag.empty()) {
        ss << "\"_tag\":\"" << jsonEscape(m_impl->tag) << "\"";
        first = false;
    }

    // Dials (floats)
    for (const auto& [k, v] : m_impl->dials) {
        if (!first) ss << ",";
        ss << "\"" << jsonEscape(k) << "\":" << std::setprecision(9) << v;
        first = false;
    }

    // Texts (strings)
    for (const auto& [k, v] : m_impl->texts) {
        if (!first) ss << ",";
        ss << "\"" << jsonEscape(k) << "\":\"" << jsonEscape(v) << "\"";
        first = false;
    }

    // Arrays (float arrays)
    for (const auto& [k, arr] : m_impl->arrays) {
        if (!first) ss << ",";
        ss << "\"" << jsonEscape(k) << "\":[";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) ss << ",";
            ss << std::setprecision(9) << arr[i];
        }
        ss << "]";
        first = false;
    }

    // Children (nested nodes)
    for (const auto& [k, child] : m_impl->children) {
        if (!first) ss << ",";
        ss << "\"" << jsonEscape(k) << "\":" << child->save();
        first = false;
    }

    ss << "}";
    return ss.str();
}

// Simple JSON parser state
struct JsonParser {
    const char* p;
    const char* end;

    void skip() {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    }

    bool match(char c) {
        skip();
        if (p < end && *p == c) { ++p; return true; }
        return false;
    }

    Name parseString() {
        skip();
        if (p >= end || *p != '"') return "";
        ++p;
        Name result;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                ++p;
                switch (*p) {
                    case '"':  result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    default:   result += *p; break;
                }
            } else {
                result += *p;
            }
            ++p;
        }
        if (p < end) ++p; // skip closing quote
        return result;
    }

    float parseNumber() {
        skip();
        const char* start = p;
        if (p < end && (*p == '-' || *p == '+')) ++p;
        while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '-' || *p == '+')) ++p;
        return std::strtof(start, nullptr);
    }

    List<float> parseArray() {
        List<float> result;
        if (!match('[')) return result;
        skip();
        if (p < end && *p != ']') {
            result.push_back(parseNumber());
            while (match(',')) {
                result.push_back(parseNumber());
            }
        }
        match(']');
        return result;
    }

    bool parseNode(Node& node);
};

bool JsonParser::parseNode(Node& node) {
    if (!match('{')) return false;

    skip();
    if (p < end && *p != '}') {
        do {
            Name key = parseString();
            if (!match(':')) return false;

            skip();
            if (p < end) {
                if (*p == '"') {
                    Name val = parseString();
                    if (key == "_tag") {
                        // Internal tag - store in impl
                    } else {
                        node.text(key, val);
                    }
                } else if (*p == '[') {
                    List<float> arr = parseArray();
                    node.data(key, arr.data(), arr.size());
                } else if (*p == '{') {
                    Node& child = node.make(key);
                    if (!parseNode(child)) return false;
                } else {
                    // Number
                    float val = parseNumber();
                    node.dial(key, val);
                }
            }
        } while (match(','));
    }

    return match('}');
}

bool Node::load(const Name& json) {
    tidy();
    JsonParser parser{json.c_str(), json.c_str() + json.size()};
    return parser.parseNode(*this);
}

} // namespace pipe
