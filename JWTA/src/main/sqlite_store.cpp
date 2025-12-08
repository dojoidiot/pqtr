// sqlite_store.cpp
// SQLite implementation of JWTA Store interface

#include "sqlite_store.hpp"
#include <sqlite3.h>
#include <cstring>

namespace jwta {

SqliteStore::SqliteStore() : m_db(nullptr) {}

SqliteStore::~SqliteStore() {
    close();
}

bool SqliteStore::open(const std::string& path) {
    if (m_db) {
        close();
    }

    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        m_db = nullptr;
        return false;
    }

    // Enable foreign keys
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    // Create tables
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id TEXT PRIMARY KEY,
            email TEXT UNIQUE NOT NULL,
            tier TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            pubkey BLOB NOT NULL,
            privkey_encrypted BLOB NOT NULL,
            privkey_salt BLOB
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

        CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
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

void SqliteStore::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool SqliteStore::createUser(const User& user) {
    const char* sql = R"(
        INSERT INTO users (id, email, tier, created_at, pubkey, privkey_encrypted, privkey_salt)
        VALUES (?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, user.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user.email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user.tier.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, user.created_at);
    sqlite3_bind_blob(stmt, 5, user.pubkey.data(), user.pubkey.size(), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 6, user.privkey_encrypted.data(), user.privkey_encrypted.size(), SQLITE_TRANSIENT);

    if (!user.privkey_salt.empty()) {
        sqlite3_bind_blob(stmt, 7, user.privkey_salt.data(), user.privkey_salt.size(), SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 7);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::optional<User> SqliteStore::getUser(const std::string& id) {
    const char* sql = "SELECT id, email, tier, created_at, pubkey, privkey_encrypted, privkey_salt FROM users WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    User user;
    user.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    user.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    user.tier = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    user.created_at = sqlite3_column_int64(stmt, 3);

    const void* pubkey_data = sqlite3_column_blob(stmt, 4);
    int pubkey_size = sqlite3_column_bytes(stmt, 4);
    user.pubkey.assign(static_cast<const uint8_t*>(pubkey_data),
                       static_cast<const uint8_t*>(pubkey_data) + pubkey_size);

    const void* privkey_data = sqlite3_column_blob(stmt, 5);
    int privkey_size = sqlite3_column_bytes(stmt, 5);
    user.privkey_encrypted.assign(static_cast<const uint8_t*>(privkey_data),
                                   static_cast<const uint8_t*>(privkey_data) + privkey_size);

    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
        const void* salt_data = sqlite3_column_blob(stmt, 6);
        int salt_size = sqlite3_column_bytes(stmt, 6);
        user.privkey_salt.assign(static_cast<const uint8_t*>(salt_data),
                                  static_cast<const uint8_t*>(salt_data) + salt_size);
    }

    sqlite3_finalize(stmt);
    return user;
}

std::optional<User> SqliteStore::getUserByEmail(const std::string& email) {
    const char* sql = "SELECT id FROM users WHERE email = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);

    return getUser(id);
}

bool SqliteStore::createOtp(const Otp& otp) {
    const char* sql = R"(
        INSERT OR REPLACE INTO otps (email, code, expires_at, purpose)
        VALUES (?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, otp.email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, otp.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, otp.expires_at);
    sqlite3_bind_text(stmt, 4, otp.purpose.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

std::optional<Otp> SqliteStore::getOtp(const std::string& email) {
    const char* sql = "SELECT email, code, expires_at, purpose FROM otps WHERE email = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
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

bool SqliteStore::deleteOtp(const std::string& email) {
    const char* sql = "DELETE FROM otps WHERE email = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool SqliteStore::storeRefreshToken(const std::string& user_id, const std::string& token_hash) {
    const char* sql = R"(
        INSERT OR REPLACE INTO refresh_tokens (user_id, token_hash)
        VALUES (?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, token_hash.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool SqliteStore::validateRefreshToken(const std::string& user_id, const std::string& token_hash) {
    const char* sql = "SELECT 1 FROM refresh_tokens WHERE user_id = ? AND token_hash = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, token_hash.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_ROW;
}

bool SqliteStore::revokeRefreshToken(const std::string& user_id) {
    const char* sql = "DELETE FROM refresh_tokens WHERE user_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

} // namespace jwta
