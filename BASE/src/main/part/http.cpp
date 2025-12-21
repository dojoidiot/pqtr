// http.cpp - HTTP service handlers
//
// NOTE: Use printf+fflush for logging, NOT iostream. httplib runs handlers
// in worker threads where cout/cerr buffer unpredictably.

#include "base.hpp"
#include <sodium.h>
#include <ctime>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>

namespace base {

// Validate a path component (name or filename) - prevent directory traversal
// Allows: alphanumeric, dash, underscore, dot (but not leading dot or ..)
static bool validPathComponent(const std::string& s) {
    if (s.empty() || s.size() > 255) return false;
    if (s[0] == '.') return false;  // No hidden files or ..
    if (s.find("..") != std::string::npos) return false;
    for (char c : s) {
        if (c >= 'a' && c <= 'z') continue;
        if (c >= 'A' && c <= 'Z') continue;
        if (c >= '0' && c <= '9') continue;
        if (c == '-' || c == '_' || c == '.') continue;
        return false;  // Invalid character (including / and null)
    }
    return true;
}

Service::Service(Store& store, Mailer& mailer)
    : m_store(store), m_mailer(mailer) {
}

Service::~Service() = default;

bool Service::init() {
    if (!crypto::init()) {
        return false;
    }

    // Try to load existing signing keys from database
    if (m_store.getSigningKeys(m_signing_pubkey, m_signing_privkey)) {
        printf("[init] Signing keys loaded from database\n"); fflush(stdout);
        return true;
    }

    // Generate new signing keypair
    printf("[init] Generating new signing keys...\n"); fflush(stdout);
    if (!crypto::generateKeypair(m_signing_pubkey, m_signing_privkey)) {
        printf("[init] FAIL: Could not generate signing keys\n"); fflush(stdout);
        return false;
    }

    // Persist keys to database
    if (m_store.setSigningKeys(m_signing_pubkey, m_signing_privkey)) {
        printf("[init] Signing keys saved to database\n"); fflush(stdout);
    } else {
        printf("[init] WARNING: Could not save signing keys!\n"); fflush(stdout);
    }

    return true;
}

rpc::RegisterResponse Service::handleRegister(const rpc::RegisterRequest& req) {
    rpc::RegisterResponse resp{};

    printf("[register] email=%s\n", req.email.c_str()); fflush(stdout);

    // Rate limit check
    if (!m_store.checkOtpRateLimit(req.email)) {
        printf("[register] FAIL: rate limit exceeded\n"); fflush(stdout);
        resp.ok = false;
        resp.error = "Rate limit exceeded. Please wait a moment.";
        return resp;
    }

    // Check if user already exists - if so, send login OTP instead
    auto existing = m_store.getUserByEmail(req.email);
    if (existing) {
        if (existing->locked) {
            printf("[register] FAIL: user locked\n"); fflush(stdout);
            resp.ok = false;
            resp.error = "This account is locked.";
            return resp;
        }
        // Existing user - send login OTP
        printf("[register] existing user, forwarding to login\n"); fflush(stdout);
        auto login_resp = handleLogin({req.email});
        resp.ok = login_resp.ok;
        resp.expires = login_resp.expires;
        resp.error = login_resp.error;
        return resp;
    }

    // Record this OTP request for rate limiting
    m_store.recordOtpRequest(req.email);

    // Generate OTP
    std::string otp_code = crypto::generateOtp();
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    int64_t expires = now + 600;  // 10 minutes

    Otp otp;
    otp.email = req.email;
    otp.code = otp_code;
    otp.expires_at = expires;
    otp.purpose = "register";

    // Delete any existing OTP for this email
    m_store.deleteOtp(req.email);

    // Store new OTP
    if (!m_store.createOtp(otp)) {
        printf("[register] FAIL: createOtp failed\n"); fflush(stdout);
        resp.ok = false;
        resp.error = "Database error. Could not create login request.";
        return resp;
    }

    // Send OTP email
    printf("[register] sending OTP...\n"); fflush(stdout);
    if (!m_mailer.sendOtp(req.email, otp_code)) {
        printf("[register] FAIL: sendOtp failed\n"); fflush(stdout);
        m_store.deleteOtp(req.email);
        resp.ok = false;
        resp.error = "Failed to send login code.";
        return resp;
    }

    printf("[register] OK\n"); fflush(stdout);
    resp.ok = true;
    resp.expires = 600;
    return resp;
}

rpc::VerifyResponse Service::handleVerify(const rpc::VerifyRequest& req) {
    rpc::VerifyResponse resp{};

    printf("[verify] email=%s otp=%s (len=%zu)\n", req.email.c_str(), req.otp.c_str(), req.otp.size()); fflush(stdout);

    // Rate limit check - prevent brute force OTP guessing
    if (!m_store.checkVerifyRateLimit(req.email)) {
        printf("[verify] FAIL: rate limit exceeded\n"); fflush(stdout);
        resp.error = "Too many attempts. Please wait a moment.";
        return resp;
    }

    // Get OTP
    auto otp = m_store.getOtp(req.email);
    if (!otp) {
        printf("[verify] FAIL: no OTP found for email=%s\n", req.email.c_str()); fflush(stdout);
        resp.error = "No pending login found for that email. Please try again.";
        return resp;
    }
    printf("[verify] DB has code=%s (len=%zu)\n", otp->code.c_str(), otp->code.size()); fflush(stdout);

    // Check expiration
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    if (otp->expires_at < now) {
        printf("[verify] FAIL: OTP expired\n"); fflush(stdout);
        m_store.deleteOtp(req.email);
        resp.error = "Your one-time code has expired. Please try again.";
        return resp;
    }

    // Record this verify attempt before checking (prevents timing attacks)
    m_store.recordVerifyAttempt(req.email);

    // Verify OTP code (constant-time comparison)
    if (otp->code.size() != req.otp.size() ||
        sodium_memcmp(otp->code.c_str(), req.otp.c_str(), otp->code.size()) != 0) {
        printf("[verify] FAIL: wrong OTP code\n"); fflush(stdout);
        resp.error = "The code you entered is incorrect.";
        return resp;
    }

    printf("[verify] OTP valid\n"); fflush(stdout);

    // Clear rate limit on successful verification
    m_store.clearVerifyAttempts(req.email);

    // OTP verified - delete it
    m_store.deleteOtp(req.email);

    User user;

    if (otp->purpose == "register") {
        // Create new user
        user.id = crypto::generateUuid();
        user.itag = crypto::generateItag();
        user.email = req.email;
        user.tier = "registered";
        // Bootstrap admin gets PQTR role
        user.role = (!m_admin_email.empty() && req.email == m_admin_email) ? "PQTR" : "NONE";
        user.created_at = now;

        if (!m_store.createUser(user)) {
            resp.error = "Failed to create new user account.";
            return resp;
        }

        // Create user folders in var/LABS/<itag>
        if (!m_data_area.empty()) {
            std::string labs_path = m_data_area + "LABS";
            mkdir(labs_path.c_str(), 0755);
            std::string user_path = labs_path + "/" + user.itag;
            mkdir(user_path.c_str(), 0755);
            mkdir((user_path + "/gear").c_str(), 0755);
            mkdir((user_path + "/pipe").c_str(), 0755);
        }
    } else {
        // Login - get existing user
        auto existing = m_store.getUserByEmail(req.email);
        if (!existing) {
            resp.error = "Could not find user account.";
            return resp;
        }
        user = *existing;
    }

    // Generate refresh token
    std::string refresh_token = crypto::generateRefreshToken();
    std::string token_hash = crypto::hashToken(refresh_token);

    if (!m_store.storeRefreshToken(user.id, token_hash)) {
        resp.error = "Failed to store session.";
        return resp;
    }

    // Generate JWT
    Claims claims;
    claims.iss = "base.pqtr.io";
    claims.sub = user.id;
    claims.itag = user.itag;
    claims.email = user.email;
    claims.tier = user.tier;
    claims.role = user.role.empty() ? "NONE" : user.role;
    claims.iat = now;
    claims.exp = now + 3600;  // 1 hour

    resp.jwt = jwt::encode(claims, m_signing_privkey);
    resp.refresh_token = refresh_token;
    resp.user_id = user.id;
    resp.itag = user.itag;
    resp.role = claims.role;

    return resp;
}

rpc::LoginResponse Service::handleLogin(const rpc::LoginRequest& req) {
    rpc::LoginResponse resp{};

    printf("[login] email=%s\n", req.email.c_str()); fflush(stdout);

    // Rate limit check
    if (!m_store.checkOtpRateLimit(req.email)) {
        printf("[login] FAIL: rate limit exceeded\n"); fflush(stdout);
        resp.ok = false;
        resp.error = "Rate limit exceeded. Please wait a moment.";
        return resp;
    }

    // Check if user exists
    auto existing = m_store.getUserByEmail(req.email);
    if (!existing) {
        printf("[login] FAIL: user not found\n"); fflush(stdout);
        resp.ok = false;
        resp.error = "User account not found.";
        return resp;
    }

    // Check if account is locked
    if (existing->locked) {
        printf("[login] FAIL: user locked\n"); fflush(stdout);
        resp.ok = false;
        resp.error = "This account is locked.";
        return resp;
    }

    // Record this OTP request for rate limiting
    m_store.recordOtpRequest(req.email);

    // Generate OTP
    std::string otp_code = crypto::generateOtp();
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    int64_t expires = now + 600;  // 10 minutes

    Otp otp;
    otp.email = req.email;
    otp.code = otp_code;
    otp.expires_at = expires;
    otp.purpose = "login";

    // Delete any existing OTP
    m_store.deleteOtp(req.email);

    // Store new OTP
    if (!m_store.createOtp(otp)) {
        printf("[login] FAIL: createOtp failed\n"); fflush(stdout);
        resp.ok = false;
        resp.error = "Database error. Could not create login request.";
        return resp;
    }

    // Send OTP email
    printf("[login] sending OTP...\n"); fflush(stdout);
    if (!m_mailer.sendOtp(req.email, otp_code)) {
        printf("[login] FAIL: sendOtp failed\n"); fflush(stdout);
        m_store.deleteOtp(req.email);
        resp.ok = false;
        resp.error = "Failed to send login code.";
        return resp;
    }

    printf("[login] OK\n"); fflush(stdout);
    resp.ok = true;
    resp.expires = 600;
    return resp;
}

rpc::RefreshResponse Service::handleRefresh(const rpc::RefreshRequest& req) {
    rpc::RefreshResponse resp{};

    // Hash the token
    std::string token_hash = crypto::hashToken(req.refresh_token);

    // TODO: Implement proper refresh token validation
    // For now, return empty response

    return resp;
}

std::vector<uint8_t> Service::getSigningPubkey() const {
    return m_signing_pubkey;
}

// Helper: verify JWT and check for PQTR role
static bool verifyAdminJwt(const std::string& token, const std::vector<uint8_t>& pubkey) {
    auto claims = jwt::decode(token, pubkey);
    if (!claims) return false;
    return claims->role == "PQTR";
}

rpc::FindResponse Service::handleFind(const rpc::FindRequest& req) {
    rpc::FindResponse resp{};

    if (!verifyAdminJwt(req.jwt, m_signing_pubkey)) {
        return resp;  // Unauthorized
    }

    auto user = m_store.getUserByEmail(req.email);
    if (!user) {
        return resp;
    }

    resp.user_id = user->id;
    resp.email = user->email;
    resp.tier = user->tier;
    resp.role = user->role;
    resp.locked = user->locked;
    resp.created_at = user->created_at;

    return resp;
}

rpc::GiveResponse Service::handleGive(const rpc::GiveRequest& req) {
    rpc::GiveResponse resp{};

    if (!verifyAdminJwt(req.jwt, m_signing_pubkey)) {
        return resp;
    }

    // Validate role
    if (req.role != "NONE" && req.role != "PLAY" && req.role != "HERO" && req.role != "PQTR") {
        return resp;
    }

    resp.ok = m_store.updateUserRole(req.user_id, req.role);
    return resp;
}

rpc::TakeResponse Service::handleTake(const rpc::TakeRequest& req) {
    rpc::TakeResponse resp{};

    if (!verifyAdminJwt(req.jwt, m_signing_pubkey)) {
        return resp;
    }

    resp.ok = m_store.updateUserRole(req.user_id, "NONE");
    return resp;
}

rpc::LockResponse Service::handleLock(const rpc::LockRequest& req) {
    rpc::LockResponse resp{};

    if (!verifyAdminJwt(req.jwt, m_signing_pubkey)) {
        return resp;
    }

    resp.ok = m_store.updateUserLocked(req.user_id, true);
    return resp;
}

rpc::FreeResponse Service::handleFree(const rpc::FreeRequest& req) {
    rpc::FreeResponse resp{};

    if (!verifyAdminJwt(req.jwt, m_signing_pubkey)) {
        return resp;
    }

    resp.ok = m_store.updateUserLocked(req.user_id, false);
    return resp;
}

rpc::DropResponse Service::handleDrop(const rpc::DropRequest& req) {
    rpc::DropResponse resp{};

    if (!verifyAdminJwt(req.jwt, m_signing_pubkey)) {
        return resp;
    }

    // Also revoke refresh tokens
    m_store.revokeRefreshToken(req.user_id);
    resp.ok = m_store.deleteUser(req.user_id);
    return resp;
}

rpc::InfoResponse Service::handleInfo(const rpc::InfoRequest& req) {
    rpc::InfoResponse resp{};

    if (!verifyAdminJwt(req.jwt, m_signing_pubkey)) {
        return resp;
    }

    resp.total_users = m_store.countUsers();
    resp.users_none = m_store.countUsersByRole("NONE");
    resp.users_play = m_store.countUsersByRole("PLAY");
    resp.users_hero = m_store.countUsersByRole("HERO");
    resp.users_pqtr = m_store.countUsersByRole("PQTR");

    return resp;
}

bool Service::sendBootstrapEmail(const std::string& boot_email) {
    // Generate one-time bootstrap token
    m_bootstrap_token = crypto::generateBootstrapToken();

    // Send email with bootstrap link
    std::string message = "BASE Bootstrap Token: " + m_bootstrap_token +
                          "\n\nReply to this email to activate admin account.";

    return m_mailer.sendOtp(boot_email, m_bootstrap_token);
}

bool Service::verifyBootstrapToken(const std::string& token) {
    if (m_bootstrap_token.empty()) {
        return false;  // No bootstrap pending
    }

    // Constant-time comparison
    if (token.length() != m_bootstrap_token.length()) {
        return false;
    }

    bool match = sodium_memcmp(token.c_str(), m_bootstrap_token.c_str(), token.length()) == 0;

    if (match) {
        // Clear token after successful verification (one-time use)
        m_bootstrap_token.clear();
    }

    return match;
}

rpc::ListResponse Service::handleList(const rpc::ListRequest& req) {
    rpc::ListResponse resp{};

    printf("[list] jwt_len=%zu name=%s\n", req.jwt.size(), req.name.c_str()); fflush(stdout);

    // Verify JWT and extract itag
    auto claims = jwt::decode(req.jwt, m_signing_pubkey);
    if (!claims) {
        printf("[list] FAIL: JWT decode failed\n"); fflush(stdout);
        return resp;
    }
    if (!itag::valid(claims->itag)) {
        printf("[list] FAIL: invalid itag=%s\n", claims->itag.c_str()); fflush(stdout);
        return resp;
    }
    printf("[list] itag=%s\n", claims->itag.c_str()); fflush(stdout);

    if (m_data_area.empty()) {
        resp.ok = true;
        return resp;  // No data area configured
    }

    // If name provided, list files in that pipe folder
    // Otherwise, list pipe folders
    std::string path;
    bool list_files = !req.name.empty();

    if (list_files) {
        if (!validPathComponent(req.name)) {
            return resp;  // Invalid name
        }
        path = m_data_area + "LABS/" + claims->itag + "/pipe/" + req.name;
    } else {
        path = m_data_area + "LABS/" + claims->itag + "/pipe";
    }

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        resp.ok = true;
        return resp;  // Empty or doesn't exist
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        if (list_files) {
            // List regular files
            if (entry->d_type == DT_REG) {
                resp.items.push_back(name);
            }
        } else {
            // List directories (pipes)
            if (entry->d_type == DT_DIR) {
                resp.items.push_back(name);
            }
        }
    }
    closedir(dir);

    resp.ok = true;
    return resp;
}

rpc::TestResponse Service::handleTest(const rpc::TestRequest& req) {
    rpc::TestResponse resp{};

    // Verify JWT and extract itag
    auto claims = jwt::decode(req.jwt, m_signing_pubkey);
    if (!claims || !itag::valid(claims->itag)) {
        return resp;
    }

    // Validate name to prevent path traversal
    if (m_data_area.empty() || !validPathComponent(req.name)) {
        resp.ok = true;
        resp.exists = false;
        return resp;
    }

    // Check if pipe folder exists (var/LABS/<itag>/pipe/<name>)
    std::string pipe_path = m_data_area + "LABS/" + claims->itag + "/pipe/" + req.name;
    struct stat st;
    resp.exists = (stat(pipe_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
    resp.ok = true;
    return resp;
}

rpc::PushResponse Service::handlePush(const rpc::PushRequest& req) {
    rpc::PushResponse resp{};

    // Verify JWT and extract itag
    auto claims = jwt::decode(req.jwt, m_signing_pubkey);
    if (!claims || !itag::valid(claims->itag)) {
        resp.error = "Unauthorized";
        return resp;
    }

    if (m_data_area.empty()) {
        resp.error = "No data area";
        return resp;
    }

    // Validate path components to prevent directory traversal
    if (!validPathComponent(req.name)) {
        resp.error = "Invalid name";
        return resp;
    }
    if (!validPathComponent(req.file)) {
        resp.error = "Invalid filename";
        return resp;
    }
    if (req.data.empty()) {
        resp.error = "Empty data";
        return resp;
    }

    // Create pipe folder (var/LABS/<itag>/pipe/<name>)
    std::string user_path = m_data_area + "LABS/" + claims->itag;
    std::string pipe_base = user_path + "/pipe";
    std::string pipe_path = pipe_base + "/" + req.name;

    mkdir(user_path.c_str(), 0755);
    mkdir(pipe_base.c_str(), 0755);
    if (mkdir(pipe_path.c_str(), 0755) != 0 && errno != EEXIST) {
        resp.error = "Failed to create folder";
        return resp;
    }

    // Write file directly
    std::string file_path = pipe_path + "/" + req.file;
    FILE* f = fopen(file_path.c_str(), "wb");
    if (!f) {
        resp.error = "Failed to write file";
        return resp;
    }
    fwrite(req.data.data(), 1, req.data.size(), f);
    fclose(f);

    resp.ok = true;
    return resp;
}

rpc::DropPipeResponse Service::handleDropPipe(const rpc::DropPipeRequest& req) {
    rpc::DropPipeResponse resp{};

    // Verify JWT and extract itag
    auto claims = jwt::decode(req.jwt, m_signing_pubkey);
    if (!claims || !itag::valid(claims->itag)) {
        resp.error = "Unauthorized";
        return resp;
    }

    if (m_data_area.empty()) {
        resp.error = "No data area";
        return resp;
    }

    // Validate path component
    if (!validPathComponent(req.name)) {
        resp.error = "Invalid name";
        return resp;
    }

    // Build pipe path
    std::string pipe_path = m_data_area + "LABS/" + claims->itag + "/pipe/" + req.name;

    // Check if exists
    struct stat st;
    if (stat(pipe_path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        resp.error = "Not found";
        return resp;
    }

    // Delete all files in directory then remove directory
    DIR* dir = opendir(pipe_path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            std::string file_path = pipe_path + "/" + entry->d_name;
            unlink(file_path.c_str());
        }
        closedir(dir);
    }

    if (rmdir(pipe_path.c_str()) != 0) {
        resp.error = "Failed to remove directory";
        return resp;
    }

    resp.ok = true;
    return resp;
}

} // namespace base
