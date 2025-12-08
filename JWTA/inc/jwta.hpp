// jwta.hpp
// JWTA - JWT Web Auth service
// Custodial ed25519 keys with email verification

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace jwta {

// ============================================================
// User record
// ============================================================

struct User {
    std::string id;           // UUID
    std::string email;
    std::string tier;         // "anonymous", "registered", "pro"
    int64_t created_at;       // Unix timestamp

    // ed25519 keypair
    std::vector<uint8_t> pubkey;              // 32 bytes, plaintext
    std::vector<uint8_t> privkey_encrypted;   // 64 + 24 + 16 bytes (ciphertext + nonce + tag)
    std::vector<uint8_t> privkey_salt;        // 16 bytes, random per user
};

// ============================================================
// OTP record (temporary)
// ============================================================

struct Otp {
    std::string email;
    std::string code;         // 6 digits
    int64_t expires_at;       // Unix timestamp
    std::string purpose;      // "register" or "login"
};

// ============================================================
// JWT claims
// ============================================================

struct Claims {
    std::string iss;          // "jwta.pqtr.io"
    std::string sub;          // user_id
    std::string email;
    std::string tier;
    int64_t iat;              // Issued at
    int64_t exp;              // Expires at
};

// ============================================================
// Database interface
// ============================================================

class Store {
public:
    virtual ~Store() = default;

    // Users
    virtual bool createUser(const User& user) = 0;
    virtual std::optional<User> getUser(const std::string& id) = 0;
    virtual std::optional<User> getUserByEmail(const std::string& email) = 0;

    // OTPs
    virtual bool createOtp(const Otp& otp) = 0;
    virtual std::optional<Otp> getOtp(const std::string& email) = 0;
    virtual bool deleteOtp(const std::string& email) = 0;

    // Refresh tokens
    virtual bool storeRefreshToken(const std::string& user_id, const std::string& token_hash) = 0;
    virtual bool validateRefreshToken(const std::string& user_id, const std::string& token_hash) = 0;
    virtual bool revokeRefreshToken(const std::string& user_id) = 0;
};

// ============================================================
// Email sender interface
// ============================================================

class Mailer {
public:
    virtual ~Mailer() = default;
    virtual bool sendOtp(const std::string& email, const std::string& otp) = 0;
};

// ============================================================
// Crypto operations (libsodium wrapper)
// ============================================================

namespace crypto {
    // Initialize libsodium (call once at startup)
    bool init();

    // Load master key from JWTA_MASTER_KEY env var (64 hex chars = 32 bytes)
    // Returns false if not set or invalid
    bool loadMasterKey(std::vector<uint8_t>& master_key);

    // Generate ed25519 keypair
    // pubkey: 32 bytes, privkey: 64 bytes
    bool generateKeypair(std::vector<uint8_t>& pubkey, std::vector<uint8_t>& privkey);

    // Encrypt private key with master key + per-user salt
    // Returns ciphertext (privkey + nonce + auth tag)
    std::vector<uint8_t> encryptPrivkey(
        const std::vector<uint8_t>& privkey,
        const std::vector<uint8_t>& master_key,
        const std::vector<uint8_t>& salt);

    // Decrypt private key
    // Returns empty vector on failure (wrong key or tampered)
    std::vector<uint8_t> decryptPrivkey(
        const std::vector<uint8_t>& encrypted,
        const std::vector<uint8_t>& master_key,
        const std::vector<uint8_t>& salt);

    // Generate random salt (16 bytes)
    std::vector<uint8_t> generateSalt();

    // Sign message with private key
    std::vector<uint8_t> sign(const std::vector<uint8_t>& message, const std::vector<uint8_t>& privkey);

    // Verify signature with public key
    bool verify(const std::vector<uint8_t>& message, const std::vector<uint8_t>& signature, const std::vector<uint8_t>& pubkey);

    // Generate random OTP (6 digits)
    std::string generateOtp();

    // Generate UUID
    std::string generateUuid();

    // Hash token for storage
    std::string hashToken(const std::string& token);

    // Generate random refresh token
    std::string generateRefreshToken();
}

// ============================================================
// JWT operations
// ============================================================

namespace jwt {
    // Create signed JWT from claims
    // Uses JWTA's signing key (ed25519)
    std::string encode(const Claims& claims, const std::vector<uint8_t>& signing_key);

    // Decode and verify JWT
    // Returns nullopt if invalid/expired
    std::optional<Claims> decode(const std::string& token, const std::vector<uint8_t>& pubkey);
}

// ============================================================
// JRPC request/response types
// ============================================================

namespace rpc {

    // Register: start email verification
    struct RegisterRequest {
        std::string email;
    };
    struct RegisterResponse {
        bool ok;
        int expires;  // seconds until OTP expires
    };

    // Verify: complete registration/login
    struct VerifyRequest {
        std::string email;
        std::string otp;
    };
    struct VerifyResponse {
        std::string jwt;
        std::string refresh_token;
        std::string user_id;
        std::string pubkey_hex;
    };

    // Login: request OTP for existing user
    struct LoginRequest {
        std::string email;
    };
    struct LoginResponse {
        bool ok;
        int expires;
    };

    // Refresh: get new JWT
    struct RefreshRequest {
        std::string refresh_token;
    };
    struct RefreshResponse {
        std::string jwt;
    };

    // Pubkey: get user's public key
    struct PubkeyRequest {
        std::string user_id;
    };
    struct PubkeyResponse {
        std::string pubkey_hex;
    };
}

// ============================================================
// Service
// ============================================================

class Service {
public:
    Service(Store& store, Mailer& mailer);
    ~Service();

    // Initialize service
    // Loads master key from JWTA_MASTER_KEY env var
    // Generates JWTA signing keypair if needed
    bool init();

    // JRPC handlers
    rpc::RegisterResponse handleRegister(const rpc::RegisterRequest& req);
    rpc::VerifyResponse handleVerify(const rpc::VerifyRequest& req);
    rpc::LoginResponse handleLogin(const rpc::LoginRequest& req);
    rpc::RefreshResponse handleRefresh(const rpc::RefreshRequest& req);
    rpc::PubkeyResponse handlePubkey(const rpc::PubkeyRequest& req);

    // Get JWTA's public key (for JWT verification)
    std::vector<uint8_t> getSigningPubkey() const;

    // Check if master key is loaded (for testing without encryption)
    bool hasMasterKey() const { return !m_master_key.empty(); }

private:
    Store& m_store;
    Mailer& m_mailer;

    // Master key for encrypting user private keys (from env var)
    std::vector<uint8_t> m_master_key;

    // JWTA's own signing keypair
    std::vector<uint8_t> m_signing_pubkey;
    std::vector<uint8_t> m_signing_privkey;
};

} // namespace jwta
