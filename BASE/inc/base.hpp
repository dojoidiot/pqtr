// base.hpp
// BASE - JWT operations, crypto, and service

#pragma once

#include "data.hpp"
#include "mail.hpp"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace base {

struct Claims {
    std::string iss;
    std::string sub;
    std::string itag;  // Base-36 user code
    std::string email;
    std::string tier;
    std::string role;
    int64_t iat;
    int64_t exp;
};

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

namespace jwt {
    std::string encode(const Claims& claims, const std::vector<uint8_t>& signing_key);
    std::optional<Claims> decode(const std::string& token, const std::vector<uint8_t>& pubkey);
}

namespace rpc {
    struct RegisterRequest { std::string email; };
    struct RegisterResponse { bool ok; int expires; };

    struct VerifyRequest { std::string email; std::string otp; };
    struct VerifyResponse { std::string jwt; std::string refresh_token; std::string user_id; std::string itag; std::string role; };

    struct LoginRequest { std::string email; };
    struct LoginResponse { bool ok; int expires; };

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

    struct ListRequest { std::string jwt; };
    struct ListResponse { bool ok; std::vector<std::string> pipes; };

    struct TestRequest { std::string jwt; std::string name; };
    struct TestResponse { bool ok; bool exists; };

    struct PushRequest { std::string jwt; std::string name; std::string file; std::string data; };
    struct PushResponse { bool ok; std::string error; };
}

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

    std::vector<uint8_t> getSigningPubkey() const;
    void setAdminEmail(const std::string& email) { m_admin_email = email; }
    void setDataArea(const std::string& path) { m_data_area = path; }

    bool sendBootstrapEmail(const std::string& boot_email);
    bool verifyBootstrapToken(const std::string& token);
    std::string getBootstrapToken() const { return m_bootstrap_token; }

private:
    Store& m_store;
    Mailer& m_mailer;
    std::vector<uint8_t> m_signing_pubkey;
    std::vector<uint8_t> m_signing_privkey;
    std::string m_admin_email;
    std::string m_bootstrap_token;
    std::string m_data_area;
};

} // namespace base
