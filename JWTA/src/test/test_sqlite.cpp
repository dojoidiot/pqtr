// test_sqlite.cpp
// Test JWTA with SQLite store

#include "jwta.hpp"
#include "sqlite_store.hpp"
#include <iostream>
#include <cstdio>

// Console mailer for testing
class ConsoleMailer : public jwta::Mailer {
public:
    bool sendOtp(const std::string& email, const std::string& otp) override {
        std::cout << "[MAIL] OTP " << otp << " -> " << email << std::endl;
        last_otp = otp;
        return true;
    }
    std::string last_otp;
};

int main() {
    std::cout << "=== JWTA SQLite Test ===" << std::endl;

    // Use temp database
    const char* db_path = "/tmp/jwta_test.db";
    std::remove(db_path);  // Clean up any previous test

    // Initialize store
    jwta::SqliteStore store;
    if (!store.open(db_path)) {
        std::cerr << "Failed to open database" << std::endl;
        return 1;
    }
    std::cout << "[OK] Database opened: " << db_path << std::endl;

    // Initialize service
    ConsoleMailer mailer;
    jwta::Service service(store, mailer);

    if (!service.init()) {
        std::cerr << "Failed to initialize service" << std::endl;
        return 1;
    }
    std::cout << "[OK] Service initialized" << std::endl;
    std::cout << "     Master key: " << (service.hasMasterKey() ? "YES" : "NO") << std::endl;

    // Test registration
    std::cout << "\n--- Registration ---" << std::endl;

    jwta::rpc::RegisterRequest reg_req;
    reg_req.email = "alice@example.com";
    auto reg_resp = service.handleRegister(reg_req);

    if (!reg_resp.ok) {
        std::cerr << "Registration failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] Registration initiated" << std::endl;

    // Verify OTP
    jwta::rpc::VerifyRequest verify_req;
    verify_req.email = "alice@example.com";
    verify_req.otp = mailer.last_otp;
    auto verify_resp = service.handleVerify(verify_req);

    if (verify_resp.jwt.empty()) {
        std::cerr << "Verification failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] User created:" << std::endl;
    std::cout << "     ID: " << verify_resp.user_id << std::endl;
    std::cout << "     Pubkey: " << verify_resp.pubkey_hex.substr(0, 16) << "..." << std::endl;

    // Verify user is in database
    auto user = store.getUserByEmail("alice@example.com");
    if (!user) {
        std::cerr << "User not found in database" << std::endl;
        return 1;
    }
    std::cout << "[OK] User persisted in SQLite" << std::endl;
    std::cout << "     privkey_encrypted: " << user->privkey_encrypted.size() << " bytes" << std::endl;
    std::cout << "     privkey_salt: " << user->privkey_salt.size() << " bytes" << std::endl;

    // Test login
    std::cout << "\n--- Login ---" << std::endl;

    jwta::rpc::LoginRequest login_req;
    login_req.email = "alice@example.com";
    auto login_resp = service.handleLogin(login_req);

    if (!login_resp.ok) {
        std::cerr << "Login request failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] Login OTP sent" << std::endl;

    jwta::rpc::VerifyRequest login_verify;
    login_verify.email = "alice@example.com";
    login_verify.otp = mailer.last_otp;
    auto login_verify_resp = service.handleVerify(login_verify);

    if (login_verify_resp.jwt.empty()) {
        std::cerr << "Login verification failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] Login successful" << std::endl;

    // Verify JWT
    auto claims = jwta::jwt::decode(login_verify_resp.jwt, service.getSigningPubkey());
    if (!claims) {
        std::cerr << "JWT decode failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] JWT valid: " << claims->email << " (" << claims->tier << ")" << std::endl;

    // Test pubkey lookup
    std::cout << "\n--- Pubkey Lookup ---" << std::endl;

    jwta::rpc::PubkeyRequest pk_req;
    pk_req.user_id = verify_resp.user_id;
    auto pk_resp = service.handlePubkey(pk_req);

    if (pk_resp.pubkey_hex.empty()) {
        std::cerr << "Pubkey lookup failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] Pubkey: " << pk_resp.pubkey_hex.substr(0, 16) << "..." << std::endl;

    // Test persistence - close and reopen
    std::cout << "\n--- Persistence Test ---" << std::endl;
    store.close();

    jwta::SqliteStore store2;
    if (!store2.open(db_path)) {
        std::cerr << "Failed to reopen database" << std::endl;
        return 1;
    }

    auto user2 = store2.getUserByEmail("alice@example.com");
    if (!user2) {
        std::cerr << "User not found after reopen" << std::endl;
        return 1;
    }
    std::cout << "[OK] User persisted across sessions" << std::endl;
    std::cout << "     ID: " << user2->id << std::endl;
    std::cout << "     Email: " << user2->email << std::endl;

    store2.close();

    // Clean up
    std::remove(db_path);

    std::cout << "\n=== All SQLite tests passed ===" << std::endl;
    return 0;
}
