// jwta.cpp
// JWTA - JWT Web Auth service implementation

#include "jwta.hpp"
#include <sodium.h>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>

namespace jwta {

// ============================================================
// Crypto implementation (libsodium wrapper)
// ============================================================

namespace crypto {

bool init() {
    return sodium_init() >= 0;
}

bool loadMasterKey(std::vector<uint8_t>& master_key) {
    const char* env = std::getenv("JWTA_MASTER_KEY");
    if (!env) {
        return false;
    }

    std::string hex(env);
    if (hex.length() != 64) {
        return false;  // Must be 64 hex chars = 32 bytes
    }

    master_key.resize(32);
    for (size_t i = 0; i < 32; i++) {
        unsigned int byte;
        if (sscanf(hex.c_str() + i * 2, "%02x", &byte) != 1) {
            master_key.clear();
            return false;
        }
        master_key[i] = static_cast<uint8_t>(byte);
    }

    return true;
}

std::vector<uint8_t> generateSalt() {
    std::vector<uint8_t> salt(16);
    randombytes_buf(salt.data(), salt.size());
    return salt;
}

std::vector<uint8_t> encryptPrivkey(
    const std::vector<uint8_t>& privkey,
    const std::vector<uint8_t>& master_key,
    const std::vector<uint8_t>& salt)
{
    if (master_key.size() != 32 || salt.size() != 16) {
        return {};
    }

    // Derive per-user encryption key using BLAKE2b
    std::vector<uint8_t> derived_key(crypto_secretbox_KEYBYTES);  // 32 bytes
    crypto_generichash_state state;
    crypto_generichash_init(&state, master_key.data(), master_key.size(), derived_key.size());
    crypto_generichash_update(&state, salt.data(), salt.size());
    crypto_generichash_final(&state, derived_key.data(), derived_key.size());

    // Generate random nonce
    std::vector<uint8_t> nonce(crypto_secretbox_NONCEBYTES);  // 24 bytes
    randombytes_buf(nonce.data(), nonce.size());

    // Encrypt: ciphertext = privkey + auth tag
    std::vector<uint8_t> ciphertext(privkey.size() + crypto_secretbox_MACBYTES);  // 64 + 16 = 80 bytes
    if (crypto_secretbox_easy(ciphertext.data(), privkey.data(), privkey.size(),
                              nonce.data(), derived_key.data()) != 0) {
        return {};
    }

    // Return: ciphertext || nonce (80 + 24 = 104 bytes)
    std::vector<uint8_t> result;
    result.reserve(ciphertext.size() + nonce.size());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    result.insert(result.end(), nonce.begin(), nonce.end());

    // Zero out derived key
    sodium_memzero(derived_key.data(), derived_key.size());

    return result;
}

std::vector<uint8_t> decryptPrivkey(
    const std::vector<uint8_t>& encrypted,
    const std::vector<uint8_t>& master_key,
    const std::vector<uint8_t>& salt)
{
    if (master_key.size() != 32 || salt.size() != 16) {
        return {};
    }

    // Expected size: 64 (privkey) + 16 (tag) + 24 (nonce) = 104 bytes
    const size_t expected_size = crypto_sign_SECRETKEYBYTES + crypto_secretbox_MACBYTES + crypto_secretbox_NONCEBYTES;
    if (encrypted.size() != expected_size) {
        return {};
    }

    // Split encrypted data
    size_t ciphertext_size = encrypted.size() - crypto_secretbox_NONCEBYTES;
    const uint8_t* ciphertext = encrypted.data();
    const uint8_t* nonce = encrypted.data() + ciphertext_size;

    // Derive per-user encryption key
    std::vector<uint8_t> derived_key(crypto_secretbox_KEYBYTES);
    crypto_generichash_state state;
    crypto_generichash_init(&state, master_key.data(), master_key.size(), derived_key.size());
    crypto_generichash_update(&state, salt.data(), salt.size());
    crypto_generichash_final(&state, derived_key.data(), derived_key.size());

    // Decrypt
    std::vector<uint8_t> privkey(ciphertext_size - crypto_secretbox_MACBYTES);
    if (crypto_secretbox_open_easy(privkey.data(), ciphertext, ciphertext_size,
                                    nonce, derived_key.data()) != 0) {
        sodium_memzero(derived_key.data(), derived_key.size());
        return {};  // Decryption failed (wrong key or tampered)
    }

    sodium_memzero(derived_key.data(), derived_key.size());
    return privkey;
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

} // namespace crypto

// ============================================================
// Base64url encoding (for JWT)
// ============================================================

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

std::string toHex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t b : data) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

} // anonymous namespace

// ============================================================
// JWT implementation
// ============================================================

namespace jwt {

std::string encode(const Claims& claims, const std::vector<uint8_t>& signing_key) {
    // Header: {"alg":"EdDSA","typ":"JWT"}
    std::string header = R"({"alg":"EdDSA","typ":"JWT"})";

    // Payload
    std::ostringstream payload;
    payload << "{";
    payload << R"("iss":")" << claims.iss << R"(",)";
    payload << R"("sub":")" << claims.sub << R"(",)";
    payload << R"("email":")" << claims.email << R"(",)";
    payload << R"("tier":")" << claims.tier << R"(",)";
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
    claims.email = extractString("email");
    claims.tier = extractString("tier");
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

// ============================================================
// Service implementation
// ============================================================

Service::Service(Store& store, Mailer& mailer)
    : m_store(store), m_mailer(mailer) {
}

Service::~Service() = default;

bool Service::init() {
    if (!crypto::init()) {
        return false;
    }

    // Load master key from environment (optional - allows testing without encryption)
    crypto::loadMasterKey(m_master_key);

    // Generate JWTA's signing keypair
    if (!crypto::generateKeypair(m_signing_pubkey, m_signing_privkey)) {
        return false;
    }

    return true;
}

rpc::RegisterResponse Service::handleRegister(const rpc::RegisterRequest& req) {
    rpc::RegisterResponse resp{};

    // Check if user already exists
    auto existing = m_store.getUserByEmail(req.email);
    if (existing) {
        // User exists - they should use login instead
        resp.ok = false;
        return resp;
    }

    // Generate OTP
    std::string otp_code = crypto::generateOtp();
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    int64_t expires = now + 600;  // 10 minutes

    Otp otp;
    otp.email = req.email;
    otp.code = otp_code;
    otp.expires_at = expires;
    otp.purpose = "register";

    // Delete any existing OTP for this email
    m_store.deleteOtp(req.email);

    // Store new OTP
    if (!m_store.createOtp(otp)) {
        resp.ok = false;
        return resp;
    }

    // Send OTP email
    if (!m_mailer.sendOtp(req.email, otp_code)) {
        m_store.deleteOtp(req.email);
        resp.ok = false;
        return resp;
    }

    resp.ok = true;
    resp.expires = 600;
    return resp;
}

rpc::VerifyResponse Service::handleVerify(const rpc::VerifyRequest& req) {
    rpc::VerifyResponse resp{};

    // Get OTP
    auto otp = m_store.getOtp(req.email);
    if (!otp) {
        return resp;  // No OTP found
    }

    // Check expiration
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    if (otp->expires_at < now) {
        m_store.deleteOtp(req.email);
        return resp;  // Expired
    }

    // Verify OTP code
    if (otp->code != req.otp) {
        return resp;  // Wrong code
    }

    // OTP verified - delete it
    m_store.deleteOtp(req.email);

    User user;

    if (otp->purpose == "register") {
        // Create new user
        user.id = crypto::generateUuid();
        user.email = req.email;
        user.tier = "registered";
        user.created_at = now;

        // Generate user's ed25519 keypair
        std::vector<uint8_t> privkey;
        if (!crypto::generateKeypair(user.pubkey, privkey)) {
            return resp;
        }

        // Encrypt private key at rest
        if (!m_master_key.empty()) {
            user.privkey_salt = crypto::generateSalt();
            user.privkey_encrypted = crypto::encryptPrivkey(privkey, m_master_key, user.privkey_salt);
            if (user.privkey_encrypted.empty()) {
                return resp;  // Encryption failed
            }
            // Zero out plaintext private key
            sodium_memzero(privkey.data(), privkey.size());
        } else {
            // No master key - store unencrypted (for testing only)
            user.privkey_encrypted = privkey;
        }

        if (!m_store.createUser(user)) {
            return resp;
        }
    } else {
        // Login - get existing user
        auto existing = m_store.getUserByEmail(req.email);
        if (!existing) {
            return resp;
        }
        user = *existing;
    }

    // Generate refresh token
    std::string refresh_token = crypto::generateRefreshToken();
    std::string token_hash = crypto::hashToken(refresh_token);

    if (!m_store.storeRefreshToken(user.id, token_hash)) {
        return resp;
    }

    // Generate JWT
    Claims claims;
    claims.iss = "jwta.pqtr.io";
    claims.sub = user.id;
    claims.email = user.email;
    claims.tier = user.tier;
    claims.iat = now;
    claims.exp = now + 3600;  // 1 hour

    resp.jwt = jwt::encode(claims, m_signing_privkey);
    resp.refresh_token = refresh_token;
    resp.user_id = user.id;
    resp.pubkey_hex = toHex(user.pubkey);

    return resp;
}

rpc::LoginResponse Service::handleLogin(const rpc::LoginRequest& req) {
    rpc::LoginResponse resp{};

    // Check if user exists
    auto existing = m_store.getUserByEmail(req.email);
    if (!existing) {
        resp.ok = false;
        return resp;
    }

    // Generate OTP
    std::string otp_code = crypto::generateOtp();
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    int64_t expires = now + 600;  // 10 minutes

    Otp otp;
    otp.email = req.email;
    otp.code = otp_code;
    otp.expires_at = expires;
    otp.purpose = "login";

    // Delete any existing OTP
    m_store.deleteOtp(req.email);

    // Store new OTP
    if (!m_store.createOtp(otp)) {
        resp.ok = false;
        return resp;
    }

    // Send OTP email
    if (!m_mailer.sendOtp(req.email, otp_code)) {
        m_store.deleteOtp(req.email);
        resp.ok = false;
        return resp;
    }

    resp.ok = true;
    resp.expires = 600;
    return resp;
}

rpc::RefreshResponse Service::handleRefresh(const rpc::RefreshRequest& req) {
    rpc::RefreshResponse resp{};

    // Hash the token
    std::string token_hash = crypto::hashToken(req.refresh_token);

    // Find user by validating token
    // Note: This requires iterating users or having token->user mapping
    // For simplicity, we'd need to extend the Store interface
    // This is a placeholder implementation

    // TODO: Implement proper refresh token validation
    // For now, return empty response

    return resp;
}

rpc::PubkeyResponse Service::handlePubkey(const rpc::PubkeyRequest& req) {
    rpc::PubkeyResponse resp{};

    auto user = m_store.getUser(req.user_id);
    if (!user) {
        return resp;
    }

    resp.pubkey_hex = toHex(user->pubkey);
    return resp;
}

std::vector<uint8_t> Service::getSigningPubkey() const {
    return m_signing_pubkey;
}

} // namespace jwta
