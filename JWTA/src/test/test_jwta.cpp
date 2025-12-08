// test_jwta.cpp
// Basic test for JWTA service

#include "jwta.hpp"
#include <iostream>
#include <map>

// In-memory store implementation for testing
class MemoryStore : public jwta::Store {
public:
    bool createUser(const jwta::User& user) override {
        if (users.count(user.id)) return false;
        users[user.id] = user;
        email_to_id[user.email] = user.id;
        return true;
    }

    std::optional<jwta::User> getUser(const std::string& id) override {
        auto it = users.find(id);
        if (it == users.end()) return std::nullopt;
        return it->second;
    }

    std::optional<jwta::User> getUserByEmail(const std::string& email) override {
        auto it = email_to_id.find(email);
        if (it == email_to_id.end()) return std::nullopt;
        return getUser(it->second);
    }

    bool createOtp(const jwta::Otp& otp) override {
        otps[otp.email] = otp;
        return true;
    }

    std::optional<jwta::Otp> getOtp(const std::string& email) override {
        auto it = otps.find(email);
        if (it == otps.end()) return std::nullopt;
        return it->second;
    }

    bool deleteOtp(const std::string& email) override {
        otps.erase(email);
        return true;
    }

    bool storeRefreshToken(const std::string& user_id, const std::string& token_hash) override {
        refresh_tokens[user_id] = token_hash;
        return true;
    }

    bool validateRefreshToken(const std::string& user_id, const std::string& token_hash) override {
        auto it = refresh_tokens.find(user_id);
        if (it == refresh_tokens.end()) return false;
        return it->second == token_hash;
    }

    bool revokeRefreshToken(const std::string& user_id) override {
        refresh_tokens.erase(user_id);
        return true;
    }

    // Get stored OTP code for testing
    std::string getOtpCode(const std::string& email) {
        auto it = otps.find(email);
        if (it == otps.end()) return "";
        return it->second.code;
    }

private:
    std::map<std::string, jwta::User> users;
    std::map<std::string, std::string> email_to_id;
    std::map<std::string, jwta::Otp> otps;
    std::map<std::string, std::string> refresh_tokens;
};

// Console mailer for testing
class ConsoleMailer : public jwta::Mailer {
public:
    bool sendOtp(const std::string& email, const std::string& otp) override {
        std::cout << "[MAIL] Sending OTP " << otp << " to " << email << std::endl;
        last_otp = otp;
        return true;
    }

    std::string last_otp;
};

int main() {
    std::cout << "=== JWTA Test ===" << std::endl;

    // Initialize
    MemoryStore store;
    ConsoleMailer mailer;
    jwta::Service service(store, mailer);

    if (!service.init()) {
        std::cerr << "Failed to initialize service" << std::endl;
        return 1;
    }
    std::cout << "[OK] Service initialized" << std::endl;

    // Test crypto functions
    std::cout << "\n--- Crypto Tests ---" << std::endl;

    std::string otp = jwta::crypto::generateOtp();
    std::cout << "[OK] Generated OTP: " << otp << std::endl;

    std::string uuid = jwta::crypto::generateUuid();
    std::cout << "[OK] Generated UUID: " << uuid << std::endl;

    std::vector<uint8_t> pubkey, privkey;
    if (!jwta::crypto::generateKeypair(pubkey, privkey)) {
        std::cerr << "Failed to generate keypair" << std::endl;
        return 1;
    }
    std::cout << "[OK] Generated keypair (pubkey: " << pubkey.size() << " bytes, privkey: " << privkey.size() << " bytes)" << std::endl;

    // Test sign/verify
    std::vector<uint8_t> message = {'h', 'e', 'l', 'l', 'o'};
    auto signature = jwta::crypto::sign(message, privkey);
    std::cout << "[OK] Signed message (signature: " << signature.size() << " bytes)" << std::endl;

    if (!jwta::crypto::verify(message, signature, pubkey)) {
        std::cerr << "Signature verification failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] Signature verified" << std::endl;

    // Test registration flow
    std::cout << "\n--- Registration Flow ---" << std::endl;

    jwta::rpc::RegisterRequest reg_req;
    reg_req.email = "test@example.com";
    auto reg_resp = service.handleRegister(reg_req);

    if (!reg_resp.ok) {
        std::cerr << "Registration request failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] Registration initiated (expires in " << reg_resp.expires << "s)" << std::endl;

    // Verify with correct OTP
    std::string otp_code = store.getOtpCode("test@example.com");
    std::cout << "[OK] OTP stored: " << otp_code << std::endl;

    jwta::rpc::VerifyRequest verify_req;
    verify_req.email = "test@example.com";
    verify_req.otp = otp_code;
    auto verify_resp = service.handleVerify(verify_req);

    if (verify_resp.jwt.empty()) {
        std::cerr << "Verification failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] User verified!" << std::endl;
    std::cout << "     User ID: " << verify_resp.user_id << std::endl;
    std::cout << "     Pubkey: " << verify_resp.pubkey_hex.substr(0, 16) << "..." << std::endl;
    std::cout << "     JWT: " << verify_resp.jwt.substr(0, 50) << "..." << std::endl;

    // Verify JWT
    std::cout << "\n--- JWT Verification ---" << std::endl;
    auto claims = jwta::jwt::decode(verify_resp.jwt, service.getSigningPubkey());
    if (!claims) {
        std::cerr << "JWT decode failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] JWT decoded:" << std::endl;
    std::cout << "     iss: " << claims->iss << std::endl;
    std::cout << "     sub: " << claims->sub << std::endl;
    std::cout << "     email: " << claims->email << std::endl;
    std::cout << "     tier: " << claims->tier << std::endl;

    // Test login flow
    std::cout << "\n--- Login Flow ---" << std::endl;

    jwta::rpc::LoginRequest login_req;
    login_req.email = "test@example.com";
    auto login_resp = service.handleLogin(login_req);

    if (!login_resp.ok) {
        std::cerr << "Login request failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] Login OTP sent" << std::endl;

    // Verify login OTP
    std::string login_otp = store.getOtpCode("test@example.com");
    jwta::rpc::VerifyRequest login_verify;
    login_verify.email = "test@example.com";
    login_verify.otp = login_otp;
    auto login_verify_resp = service.handleVerify(login_verify);

    if (login_verify_resp.jwt.empty()) {
        std::cerr << "Login verification failed" << std::endl;
        return 1;
    }
    std::cout << "[OK] Login successful, new JWT issued" << std::endl;

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

    std::cout << "\n=== All tests passed ===" << std::endl;
    return 0;
}
