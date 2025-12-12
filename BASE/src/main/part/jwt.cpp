// jwt.cpp - JWT encoding/decoding with base64url

#include "base.hpp"
#include <sstream>
#include <ctime>
#include <cstdio>

namespace base {

// Base64url encoding (internal)
namespace {

static const char base64url_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string base64url_encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve((len + 2) / 3 * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

        result += base64url_chars[(n >> 18) & 0x3f];
        result += base64url_chars[(n >> 12) & 0x3f];
        if (i + 1 < len) result += base64url_chars[(n >> 6) & 0x3f];
        if (i + 2 < len) result += base64url_chars[n & 0x3f];
    }
    return result;
}

std::string base64url_encode(const std::string& str) {
    return base64url_encode(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

int base64url_char_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

std::vector<uint8_t> base64url_decode(const std::string& str) {
    std::vector<uint8_t> result;
    result.reserve(str.size() * 3 / 4);

    uint32_t n = 0;
    int bits = 0;

    for (char c : str) {
        int val = base64url_char_value(c);
        if (val < 0) continue;

        n = (n << 6) | val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<uint8_t>((n >> bits) & 0xff));
        }
    }
    return result;
}

// Escape a string for JSON (handles quotes, backslashes, control chars)
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    // Control character - use \u00XX
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

} // anonymous namespace

namespace jwt {

std::string encode(const Claims& claims, const std::vector<uint8_t>& signing_key) {
    // Header: {"alg":"EdDSA","typ":"JWT"}
    std::string header = R"({"alg":"EdDSA","typ":"JWT"})";

    // Payload (with proper JSON escaping)
    std::ostringstream payload;
    payload << "{";
    payload << R"("iss":")" << jsonEscape(claims.iss) << R"(",)";
    payload << R"("sub":")" << jsonEscape(claims.sub) << R"(",)";
    payload << R"("itag":")" << jsonEscape(claims.itag) << R"(",)";
    payload << R"("email":")" << jsonEscape(claims.email) << R"(",)";
    payload << R"("tier":")" << jsonEscape(claims.tier) << R"(",)";
    payload << R"("role":")" << jsonEscape(claims.role) << R"(",)";
    payload << R"("iat":)" << claims.iat << ",";
    payload << R"("exp":)" << claims.exp;
    payload << "}";

    std::string header_b64 = base64url_encode(header);
    std::string payload_b64 = base64url_encode(payload.str());
    std::string message = header_b64 + "." + payload_b64;

    // Sign
    std::vector<uint8_t> msg_bytes(message.begin(), message.end());
    std::vector<uint8_t> signature = crypto::sign(msg_bytes, signing_key);
    std::string sig_b64 = base64url_encode(signature.data(), signature.size());

    return message + "." + sig_b64;
}

std::optional<Claims> decode(const std::string& token, const std::vector<uint8_t>& pubkey) {
    // Split token
    size_t dot1 = token.find('.');
    if (dot1 == std::string::npos) return std::nullopt;

    size_t dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return std::nullopt;

    std::string header_b64 = token.substr(0, dot1);
    std::string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string sig_b64 = token.substr(dot2 + 1);

    // Verify signature
    std::string message = header_b64 + "." + payload_b64;
    std::vector<uint8_t> msg_bytes(message.begin(), message.end());
    std::vector<uint8_t> signature = base64url_decode(sig_b64);

    if (!crypto::verify(msg_bytes, signature, pubkey)) {
        return std::nullopt;
    }

    // Decode payload (simple JSON parsing)
    std::vector<uint8_t> payload_bytes = base64url_decode(payload_b64);
    std::string payload(payload_bytes.begin(), payload_bytes.end());

    Claims claims;

    // Extract fields (minimal JSON parsing)
    auto extractString = [&payload](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t pos = payload.find(search);
        if (pos == std::string::npos) return "";
        pos += search.length();
        size_t end = payload.find('"', pos);
        if (end == std::string::npos) return "";
        return payload.substr(pos, end - pos);
    };

    auto extractInt = [&payload](const std::string& key) -> int64_t {
        std::string search = "\"" + key + "\":";
        size_t pos = payload.find(search);
        if (pos == std::string::npos) return 0;
        pos += search.length();
        return std::stoll(payload.substr(pos));
    };

    claims.iss = extractString("iss");
    claims.sub = extractString("sub");
    claims.itag = extractString("itag");
    claims.email = extractString("email");
    claims.tier = extractString("tier");
    claims.role = extractString("role");
    claims.iat = extractInt("iat");
    claims.exp = extractInt("exp");

    // Check expiration
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    if (claims.exp < now) {
        return std::nullopt;
    }

    return claims;
}

} // namespace jwt
} // namespace base
