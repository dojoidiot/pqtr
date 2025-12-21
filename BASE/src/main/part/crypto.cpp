// crypto.cpp - Cryptographic operations (libsodium wrapper)

#include "base.hpp"
#include <sodium.h>
#include <sstream>
#include <iomanip>

namespace base {
namespace crypto {

bool init() {
    return sodium_init() >= 0;
}

bool generateKeypair(std::vector<uint8_t>& pubkey, std::vector<uint8_t>& privkey) {
    pubkey.resize(crypto_sign_PUBLICKEYBYTES);   // 32 bytes
    privkey.resize(crypto_sign_SECRETKEYBYTES);  // 64 bytes

    if (crypto_sign_keypair(pubkey.data(), privkey.data()) != 0) {
        return false;
    }
    return true;
}

std::vector<uint8_t> sign(const std::vector<uint8_t>& message, const std::vector<uint8_t>& privkey) {
    std::vector<uint8_t> signature(crypto_sign_BYTES);
    unsigned long long sig_len;

    if (crypto_sign_detached(signature.data(), &sig_len,
                             message.data(), message.size(),
                             privkey.data()) != 0) {
        return {};
    }

    signature.resize(sig_len);
    return signature;
}

bool verify(const std::vector<uint8_t>& message,
            const std::vector<uint8_t>& signature,
            const std::vector<uint8_t>& pubkey) {
    if (signature.size() != crypto_sign_BYTES ||
        pubkey.size() != crypto_sign_PUBLICKEYBYTES) {
        return false;
    }

    return crypto_sign_verify_detached(signature.data(),
                                       message.data(), message.size(),
                                       pubkey.data()) == 0;
}

std::string generateOtp() {
    uint32_t value;
    randombytes_buf(&value, sizeof(value));
    value = value % 1000000;  // 6 digits

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(6) << value;
    return oss.str();
}

std::string generateUuid() {
    uint8_t bytes[16];
    randombytes_buf(bytes, sizeof(bytes));

    // Set version 4 (random) and variant bits
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) oss << '-';
        oss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

std::string hashToken(const std::string& token) {
    std::vector<uint8_t> hash(crypto_generichash_BYTES);
    crypto_generichash(hash.data(), hash.size(),
                       reinterpret_cast<const uint8_t*>(token.data()), token.size(),
                       nullptr, 0);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t b : hash) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::string generateRefreshToken() {
    uint8_t bytes[32];
    randombytes_buf(bytes, sizeof(bytes));

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t b : bytes) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::string generateBootstrapToken() {
    return generateRefreshToken();  // Same format - 64 hex chars
}

std::string generateItag() {
    uint8_t bytes[8];
    randombytes_buf(bytes, sizeof(bytes));
    return itag::generate(bytes, sizeof(bytes));
}

} // namespace crypto
} // namespace base
