// itag.hpp
// Base-36 user identifier (8 lowercase alphanumeric characters)

#pragma once

#include <string>
#include <cstdint>

namespace itag {

constexpr size_t SIZE = 8;  // Number of base-36 digits
constexpr uint64_t MOD = 2821109907456ULL;  // 36^8

// Generate a random itag from random bytes
// Returns 8 lowercase alphanumeric characters (0-9, a-z)
inline std::string generate(const uint8_t* random_bytes, size_t len) {
    // Interpret bytes as big-endian integer, reduce modulo 36^8
    uint64_t val = 0;
    for (size_t i = 0; i < len && i < 8; i++) {
        val = (val << 8) | random_bytes[i];
        val %= MOD;
    }

    // Convert to base-36 string
    std::string result(SIZE, '0');
    for (size_t i = 0; i < SIZE; i++) {
        uint64_t digit = val % 36;
        result[i] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        val /= 36;
    }
    return result;
}

// Validate an itag - must be exactly 8 lowercase alphanumeric characters
inline bool valid(const std::string& tag) {
    if (tag.size() != SIZE) return false;
    for (char c : tag) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'))) {
            return false;
        }
    }
    return true;
}

}  // namespace itag
