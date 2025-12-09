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
    std::string role;         // "NONE" (default), "PLAY", "HERO", "PQTR" (admin)
    bool locked = false;      // Account locked (cannot login)
    int64_t created_at;       // Unix timestamp
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
    std::string role;         // "NONE", "PLAY", "HERO", "PQTR"
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

    // Admin: user management
    virtual bool updateUserRole(const std::string& id, const std::string& role) = 0;
    virtual bool updateUserLocked(const std::string& id, bool locked) = 0;
    virtual bool deleteUser(const std::string& id) = 0;
    virtual int countUsers() = 0;
    virtual int countUsersByRole(const std::string& role) = 0;

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

    // Generate ed25519 keypair (for JWT signing)
    bool generateKeypair(std::vector<uint8_t>& pubkey, std::vector<uint8_t>& privkey);

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

    // Generate random bootstrap token (32 bytes hex)
    std::string generateBootstrapToken();
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
        std::string role;
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

    // Admin: find user by email
    struct FindRequest {
        std::string jwt;       // Must be PQTR role
        std::string email;
    };
    struct FindResponse {
        std::string user_id;
        std::string email;
        std::string tier;
        std::string role;
        bool locked;
        int64_t created_at;
    };

    // Admin: set user role
    struct GiveRequest {
        std::string jwt;       // Must be PQTR role
        std::string user_id;
        std::string role;      // NONE, PLAY, HERO, PQTR
    };
    struct GiveResponse {
        bool ok;
    };

    // Admin: reset user role to NONE
    struct TakeRequest {
        std::string jwt;       // Must be PQTR role
        std::string user_id;
    };
    struct TakeResponse {
        bool ok;
    };

    // Admin: lock user account
    struct LockRequest {
        std::string jwt;       // Must be PQTR role
        std::string user_id;
    };
    struct LockResponse {
        bool ok;
    };

    // Admin: unlock user account
    struct FreeRequest {
        std::string jwt;       // Must be PQTR role
        std::string user_id;
    };
    struct FreeResponse {
        bool ok;
    };

    // Admin: delete user
    struct DropRequest {
        std::string jwt;       // Must be PQTR role
        std::string user_id;
    };
    struct DropResponse {
        bool ok;
    };

    // Admin: system info
    struct InfoRequest {
        std::string jwt;       // Must be PQTR role
    };
    struct InfoResponse {
        int total_users;
        int users_none;
        int users_play;
        int users_hero;
        int users_pqtr;
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
    // Generates JWTA signing keypair for JWT signing
    bool init();

    // JRPC handlers
    rpc::RegisterResponse handleRegister(const rpc::RegisterRequest& req);
    rpc::VerifyResponse handleVerify(const rpc::VerifyRequest& req);
    rpc::LoginResponse handleLogin(const rpc::LoginRequest& req);
    rpc::RefreshResponse handleRefresh(const rpc::RefreshRequest& req);

    // Admin handlers (require PQTR role)
    rpc::FindResponse handleFind(const rpc::FindRequest& req);
    rpc::GiveResponse handleGive(const rpc::GiveRequest& req);
    rpc::TakeResponse handleTake(const rpc::TakeRequest& req);
    rpc::LockResponse handleLock(const rpc::LockRequest& req);
    rpc::FreeResponse handleFree(const rpc::FreeRequest& req);
    rpc::DropResponse handleDrop(const rpc::DropRequest& req);
    rpc::InfoResponse handleInfo(const rpc::InfoRequest& req);

    // Get JWTA's public key (for JWT verification)
    std::vector<uint8_t> getSigningPubkey() const;

    // Set bootstrap admin email (gets PQTR role on first register)
    void setAdminEmail(const std::string& email) { m_admin_email = email; }

    // Bootstrap: send token to boot email, verify via /boot webhook
    bool sendBootstrapEmail(const std::string& boot_email);
    bool verifyBootstrapToken(const std::string& token);
    std::string getBootstrapToken() const { return m_bootstrap_token; }

private:
    Store& m_store;
    Mailer& m_mailer;

    // JWTA's own signing keypair
    std::vector<uint8_t> m_signing_pubkey;
    std::vector<uint8_t> m_signing_privkey;

    // Bootstrap admin email (gets PQTR role on first register)
    std::string m_admin_email;

    // Bootstrap token (one-time, cleared after use)
    std::string m_bootstrap_token;
};

} // namespace jwta
