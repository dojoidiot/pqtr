// data.hpp
// JWTA data store interface

#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <memory>
#include <vector>

namespace base {

struct User {
    std::string id;
    std::string itag;  // Base-36 user code (8 chars)
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

    // Rate limiting - returns true if action is allowed
    // OTP: max 3 requests per 10 minutes per email
    // Verify: max 5 attempts per OTP (to prevent brute force)
    virtual bool checkOtpRateLimit(const std::string& email) = 0;
    virtual void recordOtpRequest(const std::string& email) = 0;
    virtual bool checkVerifyRateLimit(const std::string& email) = 0;
    virtual void recordVerifyAttempt(const std::string& email) = 0;
    virtual void clearVerifyAttempts(const std::string& email) = 0;

    // Signing key persistence (prevents key drift on restart)
    virtual bool getSigningKeys(std::vector<uint8_t>& pubkey, std::vector<uint8_t>& privkey) = 0;
    virtual bool setSigningKeys(const std::vector<uint8_t>& pubkey, const std::vector<uint8_t>& privkey) = 0;
};

std::unique_ptr<Store> createStore(const std::string& db_path);

} // namespace base
