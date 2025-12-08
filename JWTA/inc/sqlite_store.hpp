// sqlite_store.hpp
// SQLite implementation of JWTA Store interface

#pragma once

#include "jwta.hpp"

struct sqlite3;

namespace jwta {

class SqliteStore : public Store {
public:
    SqliteStore();
    ~SqliteStore();

    // Open database (creates file if doesn't exist)
    bool open(const std::string& path);
    void close();

    // Store interface
    bool createUser(const User& user) override;
    std::optional<User> getUser(const std::string& id) override;
    std::optional<User> getUserByEmail(const std::string& email) override;

    bool createOtp(const Otp& otp) override;
    std::optional<Otp> getOtp(const std::string& email) override;
    bool deleteOtp(const std::string& email) override;

    bool storeRefreshToken(const std::string& user_id, const std::string& token_hash) override;
    bool validateRefreshToken(const std::string& user_id, const std::string& token_hash) override;
    bool revokeRefreshToken(const std::string& user_id) override;

private:
    sqlite3* m_db;
};

} // namespace jwta
