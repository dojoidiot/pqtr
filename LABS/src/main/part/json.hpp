// json.hpp - Lightweight JSON parser for tree view display
// No dependencies, header-only, ~200 lines

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include <cstring>

namespace json {

// JSON value types
enum class Type { Null, Bool, Number, String, Array, Object };

// Forward declaration
struct Value;
using ValuePtr = std::unique_ptr<Value>;

// Key-value pair for objects
struct Pair {
    std::string key;
    ValuePtr value;
};

// JSON value node
struct Value {
    Type type = Type::Null;

    // Value storage (union-like, only one valid based on type)
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<ValuePtr> arr_val;     // For arrays
    std::vector<Pair> obj_val;         // For objects

    // Convenience constructors
    Value() = default;
    explicit Value(Type t) : type(t) {}
    explicit Value(bool b) : type(Type::Bool), bool_val(b) {}
    explicit Value(double n) : type(Type::Number), num_val(n) {}
    explicit Value(const std::string& s) : type(Type::String), str_val(s) {}
};

// Parser state
class Parser {
    const char* m_cur;
    const char* m_end;

    void skipWhitespace() {
        while (m_cur < m_end && (*m_cur == ' ' || *m_cur == '\t' || *m_cur == '\n' || *m_cur == '\r'))
            m_cur++;
    }

    bool match(char c) {
        skipWhitespace();
        if (m_cur < m_end && *m_cur == c) {
            m_cur++;
            return true;
        }
        return false;
    }

    bool matchWord(const char* word) {
        skipWhitespace();
        size_t len = strlen(word);
        if (m_cur + len <= m_end && strncmp(m_cur, word, len) == 0) {
            m_cur += len;
            return true;
        }
        return false;
    }

    ValuePtr parseString() {
        if (!match('"')) return nullptr;

        std::string s;
        while (m_cur < m_end && *m_cur != '"') {
            if (*m_cur == '\\' && m_cur + 1 < m_end) {
                m_cur++;
                switch (*m_cur) {
                    case '"':  s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/':  s += '/'; break;
                    case 'b':  s += '\b'; break;
                    case 'f':  s += '\f'; break;
                    case 'n':  s += '\n'; break;
                    case 'r':  s += '\r'; break;
                    case 't':  s += '\t'; break;
                    case 'u':  s += "\\u"; m_cur++; continue; // Skip unicode for now
                    default:   s += *m_cur; break;
                }
            } else {
                s += *m_cur;
            }
            m_cur++;
        }

        if (!match('"')) return nullptr;

        auto v = std::make_unique<Value>(s);
        return v;
    }

    ValuePtr parseNumber() {
        skipWhitespace();
        const char* start = m_cur;

        // Optional minus
        if (m_cur < m_end && *m_cur == '-') m_cur++;

        // Integer part
        while (m_cur < m_end && *m_cur >= '0' && *m_cur <= '9') m_cur++;

        // Decimal part
        if (m_cur < m_end && *m_cur == '.') {
            m_cur++;
            while (m_cur < m_end && *m_cur >= '0' && *m_cur <= '9') m_cur++;
        }

        // Exponent
        if (m_cur < m_end && (*m_cur == 'e' || *m_cur == 'E')) {
            m_cur++;
            if (m_cur < m_end && (*m_cur == '+' || *m_cur == '-')) m_cur++;
            while (m_cur < m_end && *m_cur >= '0' && *m_cur <= '9') m_cur++;
        }

        if (m_cur == start) return nullptr;

        double num = strtod(start, nullptr);
        auto v = std::make_unique<Value>(num);
        return v;
    }

    ValuePtr parseArray() {
        if (!match('[')) return nullptr;

        auto v = std::make_unique<Value>(Type::Array);

        skipWhitespace();
        if (m_cur < m_end && *m_cur == ']') {
            m_cur++;
            return v;
        }

        while (true) {
            auto elem = parseValue();
            if (!elem) return nullptr;
            v->arr_val.push_back(std::move(elem));

            if (match(']')) break;
            if (!match(',')) return nullptr;
        }

        return v;
    }

    ValuePtr parseObject() {
        if (!match('{')) return nullptr;

        auto v = std::make_unique<Value>(Type::Object);

        skipWhitespace();
        if (m_cur < m_end && *m_cur == '}') {
            m_cur++;
            return v;
        }

        while (true) {
            auto key = parseString();
            if (!key || key->type != Type::String) return nullptr;

            if (!match(':')) return nullptr;

            auto val = parseValue();
            if (!val) return nullptr;

            Pair p;
            p.key = std::move(key->str_val);
            p.value = std::move(val);
            v->obj_val.push_back(std::move(p));

            if (match('}')) break;
            if (!match(',')) return nullptr;
        }

        return v;
    }

    ValuePtr parseValue() {
        skipWhitespace();
        if (m_cur >= m_end) return nullptr;

        if (*m_cur == '{') return parseObject();
        if (*m_cur == '[') return parseArray();
        if (*m_cur == '"') return parseString();
        if (*m_cur == '-' || (*m_cur >= '0' && *m_cur <= '9')) return parseNumber();
        if (matchWord("true")) return std::make_unique<Value>(true);
        if (matchWord("false")) return std::make_unique<Value>(false);
        if (matchWord("null")) return std::make_unique<Value>(Type::Null);

        return nullptr;
    }

public:
    ValuePtr parse(const char* json, size_t len) {
        m_cur = json;
        m_end = json + len;
        return parseValue();
    }

    ValuePtr parse(const char* json) {
        return parse(json, strlen(json));
    }
};

// Convenience function
inline ValuePtr parse(const char* json) {
    Parser p;
    return p.parse(json);
}

inline ValuePtr parse(const char* json, size_t len) {
    Parser p;
    return p.parse(json, len);
}

} // namespace json
