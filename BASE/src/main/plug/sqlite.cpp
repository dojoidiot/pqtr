// sqlite.cpp - SQLite implementation of Store interface

#include "base.hpp"
#include <sqlite3.h>
#include <cstring>
#include <ctime>
#include <iostream>

namespace base {

class SqliteStore : public Store {
public:
    SqliteStore() : m_db(nullptr) {}

    ~SqliteStore() {
        close();
    }

    bool open(const std::string& path) {
        if (m_db) close();

        int rc = sqlite3_open(path.c_str(), &m_db);
        if (rc != SQLITE_OK) {
            m_db = nullptr;
            return false;
        }

        sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS users (
                id TEXT PRIMARY KEY,
                itag TEXT UNIQUE NOT NULL,
                email TEXT UNIQUE NOT NULL,
                tier TEXT NOT NULL,
                role TEXT NOT NULL DEFAULT 'NONE',
                locked INTEGER NOT NULL DEFAULT 0,
                created_at INTEGER NOT NULL
            );
            CREATE TABLE IF NOT EXISTS otps (
                email TEXT PRIMARY KEY,
                code TEXT NOT NULL,
                expires_at INTEGER NOT NULL,
                purpose TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS refresh_tokens (
                user_id TEXT PRIMARY KEY,
                token_hash TEXT NOT NULL,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
            );
            CREATE TABLE IF NOT EXISTS rate_limits (
                email TEXT NOT NULL,
                action TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                PRIMARY KEY (email, action, timestamp)
            );
            CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
            CREATE INDEX IF NOT EXISTS idx_rate_limits ON rate_limits(email, action, timestamp);
            CREATE TABLE IF NOT EXISTS signing_keys (
                id INTEGER PRIMARY KEY CHECK (id = 1),
                pubkey BLOB NOT NULL,
                privkey BLOB NOT NULL
            );
        )";

        char* err = nullptr;
        rc = sqlite3_exec(m_db, schema, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            if (err) sqlite3_free(err);
            close();
            return false;
        }

        return true;
    }

    void close() {
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
    }

    bool createUser(const User& user) override {
        const char* sql = "INSERT INTO users (id, itag, email, tier, role, locked, created_at) VALUES (?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, user.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user.itag.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, user.email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, user.tier.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, user.role.empty() ? "NONE" : user.role.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 6, user.locked ? 1 : 0);
        sqlite3_bind_int64(stmt, 7, user.created_at);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    std::optional<User> getUser(const std::string& id) override {
        const char* sql = "SELECT id, itag, email, tier, role, locked, created_at FROM users WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        User user;
        user.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        user.itag = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.tier = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        user.role = role ? role : "NONE";
        user.locked = sqlite3_column_int(stmt, 5) != 0;
        user.created_at = sqlite3_column_int64(stmt, 6);

        sqlite3_finalize(stmt);
        return user;
    }

    std::optional<User> getUserByEmail(const std::string& email) override {
        const char* sql = "SELECT id FROM users WHERE email = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        return getUser(id);
    }

    bool updateUserRole(const std::string& id, const std::string& role) override {
        const char* sql = "UPDATE users SET role = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, role.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool updateUserLocked(const std::string& id, bool locked) override {
        const char* sql = "UPDATE users SET locked = ? WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, locked ? 1 : 0);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool deleteUser(const std::string& id) override {
        const char* sql = "DELETE FROM users WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    int countUsers() override {
        const char* sql = "SELECT COUNT(*) FROM users;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return count;
    }

    int countUsersByRole(const std::string& role) override {
        const char* sql = "SELECT COUNT(*) FROM users WHERE role = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

        sqlite3_bind_text(stmt, 1, role.c_str(), -1, SQLITE_TRANSIENT);

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return count;
    }

    bool createOtp(const Otp& otp) override {
        const char* sql = "INSERT OR REPLACE INTO otps (email, code, expires_at, purpose) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, otp.email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, otp.code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, otp.expires_at);
        sqlite3_bind_text(stmt, 4, otp.purpose.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    std::optional<Otp> getOtp(const std::string& email) override {
        const char* sql = "SELECT email, code, expires_at, purpose FROM otps WHERE email = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        Otp otp;
        otp.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        otp.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        otp.expires_at = sqlite3_column_int64(stmt, 2);
        otp.purpose = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        sqlite3_finalize(stmt);
        return otp;
    }

    bool deleteOtp(const std::string& email) override {
        const char* sql = "DELETE FROM otps WHERE email = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool storeRefreshToken(const std::string& user_id, const std::string& token_hash) override {
        const char* sql = "INSERT OR REPLACE INTO refresh_tokens (user_id, token_hash) VALUES (?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, token_hash.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool validateRefreshToken(const std::string& user_id, const std::string& token_hash) override {
        const char* sql = "SELECT 1 FROM refresh_tokens WHERE user_id = ? AND token_hash = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, token_hash.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_ROW;
    }

    bool revokeRefreshToken(const std::string& user_id) override {
        const char* sql = "DELETE FROM refresh_tokens WHERE user_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    // Rate limiting: max 3 OTP requests per 10 minutes
    bool checkOtpRateLimit(const std::string& email) override {
        int64_t cutoff = static_cast<int64_t>(std::time(nullptr)) - 600;  // 10 min window
        const char* sql = "SELECT COUNT(*) FROM rate_limits WHERE email = ? AND action = 'otp' AND timestamp > ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[sqlite] checkOtpRateLimit prepare failed" << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, cutoff);

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        // Clean up old records
        const char* cleanup = "DELETE FROM rate_limits WHERE timestamp < ?;";
        sqlite3_stmt* cstmt = nullptr;
        if (sqlite3_prepare_v2(m_db, cleanup, -1, &cstmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(cstmt, 1, cutoff - 3600);  // Keep 1 hour of history
            sqlite3_step(cstmt);
            sqlite3_finalize(cstmt);
        }

        return count < 3;  // Allow if less than 3 requests
    }

    void recordOtpRequest(const std::string& email) override {
        const char* sql = "INSERT INTO rate_limits (email, action, timestamp) VALUES (?, 'otp', ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(std::time(nullptr)));

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Rate limiting: max 5 verify attempts per OTP
    bool checkVerifyRateLimit(const std::string& email) override {
        int64_t cutoff = static_cast<int64_t>(std::time(nullptr)) - 600;  // 10 min window
        const char* sql = "SELECT COUNT(*) FROM rate_limits WHERE email = ? AND action = 'verify' AND timestamp > ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, cutoff);

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return count < 5;  // Allow if less than 5 attempts
    }

    void recordVerifyAttempt(const std::string& email) override {
        const char* sql = "INSERT INTO rate_limits (email, action, timestamp) VALUES (?, 'verify', ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(std::time(nullptr)));

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void clearVerifyAttempts(const std::string& email) override {
        const char* sql = "DELETE FROM rate_limits WHERE email = ? AND action = 'verify';";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void clearAllRateLimits() override {
        sqlite3_exec(m_db, "DELETE FROM rate_limits;", nullptr, nullptr, nullptr);
    }

    bool getSigningKeys(std::vector<uint8_t>& pubkey, std::vector<uint8_t>& privkey) override {
        const char* sql = "SELECT pubkey, privkey FROM signing_keys WHERE id = 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const void* pub = sqlite3_column_blob(stmt, 0);
            int pub_len = sqlite3_column_bytes(stmt, 0);
            const void* priv = sqlite3_column_blob(stmt, 1);
            int priv_len = sqlite3_column_bytes(stmt, 1);

            if (pub && priv && pub_len > 0 && priv_len > 0) {
                pubkey.resize(pub_len);
                privkey.resize(priv_len);
                std::memcpy(pubkey.data(), pub, pub_len);
                std::memcpy(privkey.data(), priv, priv_len);
                found = true;
            }
        }
        sqlite3_finalize(stmt);
        return found;
    }

    bool setSigningKeys(const std::vector<uint8_t>& pubkey, const std::vector<uint8_t>& privkey) override {
        const char* sql = "INSERT OR REPLACE INTO signing_keys (id, pubkey, privkey) VALUES (1, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_blob(stmt, 1, pubkey.data(), pubkey.size(), SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 2, privkey.data(), privkey.size(), SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

private:
    sqlite3* m_db;
};

std::unique_ptr<Store> createStore(const std::string& db_path) {
    auto store = std::make_unique<SqliteStore>();
    if (!store->open(db_path)) return nullptr;
    return store;
}

} // namespace base
