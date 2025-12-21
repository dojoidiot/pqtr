// base.hpp - BASE server declarations
#pragma once

#include "httplib.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <memory>

// ---------------------------------------------------------------------------
// itag - Base-36 user identifier (8 lowercase alphanumeric characters)
// ---------------------------------------------------------------------------
namespace itag {

constexpr size_t SIZE = 8;
constexpr uint64_t MOD = 2821109907456ULL;  // 36^8

inline std::string generate(const uint8_t* random_bytes, size_t len) {
    uint64_t val = 0;
    for (size_t i = 0; i < len && i < 8; i++) {
        val = (val << 8) | random_bytes[i];
        val %= MOD;
    }
    std::string result(SIZE, '0');
    for (size_t i = 0; i < SIZE; i++) {
        uint64_t digit = val % 36;
        result[i] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        val /= 36;
    }
    return result;
}

inline bool valid(const std::string& tag) {
    if (tag.size() != SIZE) return false;
    for (char c : tag) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'))) return false;
    }
    return true;
}

}  // namespace itag

// ---------------------------------------------------------------------------
// base namespace
// ---------------------------------------------------------------------------
namespace base {

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------
struct User {
    std::string id;
    std::string itag;
    std::string email;
    std::string tier;
    std::string role;
    bool locked = false;
    int64_t created_at;
};

struct Otp {
    std::string email;
    std::string code;
    int64_t expires_at;
    std::string purpose;
};

struct Claims {
    std::string iss;
    std::string sub;
    std::string itag;
    std::string email;
    std::string tier;
    std::string role;
    int64_t iat;
    int64_t exp;
};

struct Config {
    std::string host;
    int port = 0;
    std::string boot_path;
    std::string admin_email;
    std::string boot_email;
    std::string otp_from;
    std::string otp_text;
    std::string sqlite_file;
    std::string mailgun_otp_skey;
    std::string mailgun_secret;
    std::string mailgun_domain;
    std::string mailgun_region;
};

// ---------------------------------------------------------------------------
// Store interface
// ---------------------------------------------------------------------------
class Store {
public:
    virtual ~Store() = default;

    virtual bool createUser(const User& user) = 0;
    virtual std::optional<User> getUser(const std::string& id) = 0;
    virtual std::optional<User> getUserByEmail(const std::string& email) = 0;
    virtual bool updateUserRole(const std::string& id, const std::string& role) = 0;
    virtual bool updateUserLocked(const std::string& id, bool locked) = 0;
    virtual bool deleteUser(const std::string& id) = 0;
    virtual int countUsers() = 0;
    virtual int countUsersByRole(const std::string& role) = 0;

    virtual bool createOtp(const Otp& otp) = 0;
    virtual std::optional<Otp> getOtp(const std::string& email) = 0;
    virtual bool deleteOtp(const std::string& email) = 0;

    virtual bool storeRefreshToken(const std::string& user_id, const std::string& token_hash) = 0;
    virtual bool validateRefreshToken(const std::string& user_id, const std::string& token_hash) = 0;
    virtual bool revokeRefreshToken(const std::string& user_id) = 0;

    virtual bool checkOtpRateLimit(const std::string& email) = 0;
    virtual void recordOtpRequest(const std::string& email) = 0;
    virtual bool checkVerifyRateLimit(const std::string& email) = 0;
    virtual void recordVerifyAttempt(const std::string& email) = 0;
    virtual void clearVerifyAttempts(const std::string& email) = 0;
    virtual void clearAllRateLimits() = 0;

    virtual bool getSigningKeys(std::vector<uint8_t>& pubkey, std::vector<uint8_t>& privkey) = 0;
    virtual bool setSigningKeys(const std::vector<uint8_t>& pubkey, const std::vector<uint8_t>& privkey) = 0;
};

std::unique_ptr<Store> createStore(const std::string& db_path);

// ---------------------------------------------------------------------------
// Mailer interface
// ---------------------------------------------------------------------------
class Mailer {
public:
    virtual ~Mailer() = default;
    virtual bool sendOtp(const std::string& email, const std::string& otp) = 0;
};

std::unique_ptr<Mailer> createMailer(const std::string& api_key, const std::string& domain,
    const std::string& from, const std::string& otp_text, const std::string& region);
std::unique_ptr<Mailer> createConsoleMailer();

// ---------------------------------------------------------------------------
// RPC request/response types
// ---------------------------------------------------------------------------
namespace rpc {
    struct RegisterRequest { std::string email; };
    struct RegisterResponse { bool ok; int expires; std::string error; };

    struct VerifyRequest { std::string email; std::string otp; };
    struct VerifyResponse { std::string jwt; std::string refresh_token; std::string user_id; std::string itag; std::string role; std::string error; };

    struct LoginRequest { std::string email; };
    struct LoginResponse { bool ok; int expires; std::string error; };

    struct RefreshRequest { std::string refresh_token; };
    struct RefreshResponse { std::string jwt; };

    struct FindRequest { std::string jwt; std::string email; };
    struct FindResponse { std::string user_id; std::string email; std::string tier; std::string role; bool locked; int64_t created_at; };

    struct GiveRequest { std::string jwt; std::string user_id; std::string role; };
    struct GiveResponse { bool ok; };

    struct TakeRequest { std::string jwt; std::string user_id; };
    struct TakeResponse { bool ok; };

    struct LockRequest { std::string jwt; std::string user_id; };
    struct LockResponse { bool ok; };

    struct FreeRequest { std::string jwt; std::string user_id; };
    struct FreeResponse { bool ok; };

    struct DropRequest { std::string jwt; std::string user_id; };
    struct DropResponse { bool ok; };

    struct InfoRequest { std::string jwt; };
    struct InfoResponse { int total_users; int users_none; int users_play; int users_hero; int users_pqtr; };

    struct ListRequest { std::string jwt; std::string name; };
    struct ListResponse { bool ok; std::vector<std::string> items; };

    struct TestRequest { std::string jwt; std::string name; };
    struct TestResponse { bool ok; bool exists; };

    struct PushRequest { std::string jwt; std::string name; std::string file; std::string data; };
    struct PushResponse { bool ok; std::string error; };

    struct DropPipeRequest { std::string jwt; std::string name; };
    struct DropPipeResponse { bool ok; std::string error; };
}

// ---------------------------------------------------------------------------
// Service class
// ---------------------------------------------------------------------------
class Service {
public:
    Service(Store& store, Mailer& mailer);
    ~Service();

    bool init();

    rpc::RegisterResponse handleRegister(const rpc::RegisterRequest& req);
    rpc::VerifyResponse handleVerify(const rpc::VerifyRequest& req);
    rpc::LoginResponse handleLogin(const rpc::LoginRequest& req);
    rpc::RefreshResponse handleRefresh(const rpc::RefreshRequest& req);
    rpc::FindResponse handleFind(const rpc::FindRequest& req);
    rpc::GiveResponse handleGive(const rpc::GiveRequest& req);
    rpc::TakeResponse handleTake(const rpc::TakeRequest& req);
    rpc::LockResponse handleLock(const rpc::LockRequest& req);
    rpc::FreeResponse handleFree(const rpc::FreeRequest& req);
    rpc::DropResponse handleDrop(const rpc::DropRequest& req);
    rpc::InfoResponse handleInfo(const rpc::InfoRequest& req);
    rpc::ListResponse handleList(const rpc::ListRequest& req);
    rpc::TestResponse handleTest(const rpc::TestRequest& req);
    rpc::PushResponse handlePush(const rpc::PushRequest& req);
    rpc::DropPipeResponse handleDropPipe(const rpc::DropPipeRequest& req);

    std::vector<uint8_t> getSigningPubkey() const;
    void setAdminEmail(const std::string& email) { m_admin_email = email; }
    void setDataArea(const std::string& path) { m_data_area = path; }

    bool sendBootstrapEmail(const std::string& boot_email);
    bool verifyBootstrapToken(const std::string& token);

private:
    Store& m_store;
    Mailer& m_mailer;
    std::vector<uint8_t> m_signing_pubkey;
    std::vector<uint8_t> m_signing_privkey;
    std::string m_admin_email;
    std::string m_bootstrap_token;
    std::string m_data_area;
};

// ---------------------------------------------------------------------------
// Crypto functions
// ---------------------------------------------------------------------------
namespace crypto {
    bool init();
    bool generateKeypair(std::vector<uint8_t>& pubkey, std::vector<uint8_t>& privkey);
    std::vector<uint8_t> sign(const std::vector<uint8_t>& message, const std::vector<uint8_t>& privkey);
    bool verify(const std::vector<uint8_t>& message, const std::vector<uint8_t>& signature, const std::vector<uint8_t>& pubkey);
    std::string generateOtp();
    std::string generateUuid();
    std::string generateItag();
    std::string hashToken(const std::string& token);
    std::string generateRefreshToken();
    std::string generateBootstrapToken();
}

// ---------------------------------------------------------------------------
// JWT functions
// ---------------------------------------------------------------------------
namespace jwt {
    std::string encode(const Claims& claims, const std::vector<uint8_t>& signing_key);
    std::optional<Claims> decode(const std::string& token, const std::vector<uint8_t>& pubkey);
}

// ---------------------------------------------------------------------------
// Config functions
// ---------------------------------------------------------------------------
bool loadConfig(const std::string& path, Config& cfg);
std::string loadSecret(const std::string& path);

// ---------------------------------------------------------------------------
// HTTP handler functions
// ---------------------------------------------------------------------------
namespace jrpc {
    std::string handle(Service& service, const std::string& body, const std::string& auth_header);
}

namespace rest {
    void handlePush(Service& service, const httplib::Request& req, httplib::Response& res);
    void handlePull(Service& service, const std::string& data_area, const httplib::Request& req, httplib::Response& res);
    void handleDropPipe(Service& service, const std::string& data_area, const httplib::Request& req, httplib::Response& res);
}

namespace wasm {
    bool setup(httplib::Server& svr, const std::string& wasm_root);
}

} // namespace base
