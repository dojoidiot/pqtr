// server.cpp
// JWTA HTTP server with JRPC (JSON-RPC 2.0) endpoints

#include "jwta.hpp"
#include "sqlite_store.hpp"
#include "httplib.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <memory>
#include <keyutils.h>

namespace {

// Config structure
struct Config {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string db_path = "var/jwta.db";
    std::string admin_email;  // Bootstrap admin (gets PQTR role on register)
    std::string boot_email;   // Email to send bootstrap token to (e.g., boot@pqtr.ai)

    // Mailgun API (loaded from keyring in production)
    std::string mailgun_api_key;
    std::string mailgun_domain;
    std::string mailgun_from;
    std::string mailgun_region = "us";
};

// Load secret from Linux keyring
// Keys are stored as: keyctl add user "jwta:<name>" "<value>" @u
std::string loadFromKeyring(const std::string& name) {
    std::string desc = "jwta:" + name;
    key_serial_t key = request_key("user", desc.c_str(), nullptr, KEY_SPEC_USER_KEYRING);
    if (key < 0) {
        return "";
    }

    // Get key size
    long size = keyctl_read(key, nullptr, 0);
    if (size <= 0) {
        return "";
    }

    // Read key value
    std::string value(size, '\0');
    long read = keyctl_read(key, value.data(), size);
    if (read != size) {
        return "";
    }

    return value;
}

// Load config from JSON file
bool loadConfig(const std::string& path, Config& cfg) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::stringstream buf;
    buf << f.rdbuf();
    std::string json = buf.str();

    // Simple JSON extraction (reusing extractString pattern)
    auto getString = [&json](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        pos = json.find('"', pos);
        if (pos == std::string::npos) return "";
        pos++;
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
            }
            result += json[pos++];
        }
        return result;
    };

    auto getInt = [&json](const std::string& key) -> int {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0;
        pos = json.find(':', pos);
        if (pos == std::string::npos) return 0;
        pos++;
        while (pos < json.size() && !isdigit(json[pos]) && json[pos] != '-') pos++;
        return std::atoi(json.c_str() + pos);
    };

    std::string s;
    int i;

    if (!(s = getString("host")).empty()) cfg.host = s;
    if ((i = getInt("port")) > 0) cfg.port = i;
    if (!(s = getString("db_path")).empty()) cfg.db_path = s;
    if (!(s = getString("admin_email")).empty()) cfg.admin_email = s;
    if (!(s = getString("boot_email")).empty()) cfg.boot_email = s;

    // Mailgun config (nested under "mailgun")
    size_t mg_pos = json.find("\"mailgun\"");
    if (mg_pos != std::string::npos) {
        std::string mg_section = json.substr(mg_pos);
        auto getMgString = [&mg_section](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\"";
            size_t pos = mg_section.find(search);
            if (pos == std::string::npos) return "";
            pos = mg_section.find(':', pos);
            if (pos == std::string::npos) return "";
            pos = mg_section.find('"', pos);
            if (pos == std::string::npos) return "";
            pos++;
            std::string result;
            while (pos < mg_section.size() && mg_section[pos] != '"') {
                result += mg_section[pos++];
            }
            return result;
        };

        if (!(s = getMgString("api_key")).empty()) cfg.mailgun_api_key = s;
        if (!(s = getMgString("domain")).empty()) cfg.mailgun_domain = s;
        if (!(s = getMgString("from")).empty()) cfg.mailgun_from = s;
        if (!(s = getMgString("region")).empty()) cfg.mailgun_region = s;
    }

    return true;
}

// Simple JSON helpers (no external dependency)
std::string jsonString(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    out += "\"";
    return out;
}

std::string extractString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";

    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    pos++;

    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
            if (json[pos] == 'n') result += '\n';
            else result += json[pos];
        } else {
            result += json[pos];
        }
        pos++;
    }
    return result;
}

std::string extractId(const std::string& json) {
    // ID can be string, number, or null
    std::string search = "\"id\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "null";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "null";
    pos++;

    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    if (pos >= json.size()) return "null";

    if (json[pos] == '"') {
        // String ID
        return jsonString(extractString(json, "id"));
    } else if (json[pos] == 'n') {
        return "null";
    } else {
        // Number ID
        std::string num;
        while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-')) {
            num += json[pos++];
        }
        return num.empty() ? "null" : num;
    }
}

// JRPC response helpers
std::string jrpcResult(const std::string& id, const std::string& result) {
    return "{\"jsonrpc\":\"2.0\",\"result\":" + result + ",\"id\":" + id + "}";
}

std::string jrpcError(const std::string& id, int code, const std::string& message) {
    return "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":" + std::to_string(code) +
           ",\"message\":" + jsonString(message) + "},\"id\":" + id + "}";
}

// Console mailer (fallback when no mailgun config)
class ConsoleMailer : public jwta::Mailer {
public:
    bool sendOtp(const std::string& email, const std::string& otp) override {
        std::cout << "[MAIL] OTP " << otp << " -> " << email << std::endl;
        return true;
    }
};

// Mailgun HTTP API mailer (using curl for reliability)
class MailgunMailer : public jwta::Mailer {
    std::string m_api_key;
    std::string m_domain;
    std::string m_from;
    std::string m_base_url;

    // Shell-escape a string for use in single quotes
    static std::string shellEscape(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '\'') out += "'\\''";
            else out += c;
        }
        return out;
    }

public:
    MailgunMailer(const std::string& api_key, const std::string& domain,
                  const std::string& from, const std::string& region = "us")
        : m_api_key(api_key), m_domain(domain), m_from(from) {
        if (region == "eu") {
            m_base_url = "https://api.eu.mailgun.net";
        } else {
            m_base_url = "https://api.mailgun.net";
        }
    }

    bool sendOtp(const std::string& email, const std::string& otp) override {
        std::string text = "Your verification code is: " + otp + "\n\nThis code expires in 10 minutes.";

        // Build curl command - use single quotes for shell safety
        std::ostringstream cmd;
        cmd << "curl -s --max-time 10 "
            << "--user 'api:" << m_api_key << "' "
            << m_base_url << "/v3/" << m_domain << "/messages "
            << "-F 'from=" << m_from << "' "
            << "-F 'to=" << email << "' "
            << "-F 'subject=Your PQTR verification code' "
            << "-F 'text=" << text << "' 2>&1";

        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
            std::cerr << "[MAIL] Failed to execute curl" << std::endl;
            return false;
        }

        char buffer[256];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        int status = pclose(pipe);

        if (status == 0 && result.find("\"message\"") != std::string::npos) {
            std::cout << "[MAIL] OTP sent to " << email << std::endl;
            return true;
        } else {
            std::cerr << "[MAIL] Failed to send OTP to " << email << ": " << result << std::endl;
            return false;
        }
    }
};

} // anonymous namespace

int main(int argc, char* argv[]) {
    // Help
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        std::cout << "JWTA Server - JWT Web Auth\n\n";
        std::cout << "Usage: jwta [config_file]\n\n";
        std::cout << "Config file: etc/jwta.json (default)\n";
        return 0;
    }

    // Load config
    Config cfg;
    std::string config_path = (argc > 1) ? argv[1] : "etc/jwta.json";

    if (loadConfig(config_path, cfg)) {
        std::cout << "[JWTA] Config: " << config_path << std::endl;
    } else {
        std::cout << "[JWTA] Config: " << config_path << " (not found, using defaults)" << std::endl;
    }

    // Environment overrides config
    if (const char* env = std::getenv("JWTA_HOST")) cfg.host = env;
    if (const char* env = std::getenv("JWTA_PORT")) cfg.port = std::atoi(env);
    if (const char* env = std::getenv("JWTA_DB_PATH")) cfg.db_path = env;

    // Load secrets from keyring (takes priority over config file)
    // To set: keyctl add user "jwta:mailgun_api_key" "YOUR_KEY" @u
    std::string s;
    if (!(s = loadFromKeyring("mailgun_api_key")).empty()) {
        cfg.mailgun_api_key = s;
        std::cout << "[JWTA] Keyring: mailgun_api_key loaded" << std::endl;
    }
    if (!(s = loadFromKeyring("mailgun_domain")).empty()) {
        cfg.mailgun_domain = s;
        std::cout << "[JWTA] Keyring: mailgun_domain loaded" << std::endl;
    }
    if (!(s = loadFromKeyring("mailgun_from")).empty()) {
        cfg.mailgun_from = s;
        std::cout << "[JWTA] Keyring: mailgun_from loaded" << std::endl;
    }
    if (!(s = loadFromKeyring("mailgun_region")).empty()) {
        cfg.mailgun_region = s;
        std::cout << "[JWTA] Keyring: mailgun_region loaded" << std::endl;
    }

    // Initialize store
    jwta::SqliteStore store;
    if (!store.open(cfg.db_path)) {
        std::cerr << "Failed to open database: " << cfg.db_path << std::endl;
        return 1;
    }
    std::cout << "[JWTA] Database: " << cfg.db_path << std::endl;

    // Initialize mailer
    std::unique_ptr<jwta::Mailer> mailer;
    if (!cfg.mailgun_api_key.empty()) {
        mailer = std::make_unique<MailgunMailer>(
            cfg.mailgun_api_key,
            cfg.mailgun_domain,
            cfg.mailgun_from,
            cfg.mailgun_region
        );
        std::cout << "[JWTA] Mailer: Mailgun (" << cfg.mailgun_domain << ", " << cfg.mailgun_region << " region)" << std::endl;
    } else {
        mailer = std::make_unique<ConsoleMailer>();
        std::cout << "[JWTA] Mailer: Console (no mailgun config)" << std::endl;
    }

    // Initialize service
    jwta::Service service(store, *mailer);

    if (!service.init()) {
        std::cerr << "Failed to initialize service" << std::endl;
        return 1;
    }

    // Set bootstrap admin
    if (!cfg.admin_email.empty()) {
        service.setAdminEmail(cfg.admin_email);
        std::cout << "[JWTA] Admin: " << cfg.admin_email << std::endl;
    }

    // Bootstrap: send email if no PQTR user exists and boot_email is configured
    if (!cfg.boot_email.empty() && store.countUsersByRole("PQTR") == 0) {
        std::cout << "[JWTA] No admin found, sending bootstrap email to " << cfg.boot_email << std::endl;
        if (service.sendBootstrapEmail(cfg.boot_email)) {
            std::cout << "[JWTA] Bootstrap token sent (expires on restart)" << std::endl;
        } else {
            std::cerr << "[JWTA] Failed to send bootstrap email" << std::endl;
        }
    }

    // Create HTTP server
    httplib::Server svr;

    // Health check
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // JRPC endpoint
    svr.Post("/rpc", [&service](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");

        const std::string& body = req.body;
        std::string id = extractId(body);
        std::string method = extractString(body, "method");

        // Validate JSON-RPC
        if (body.find("\"jsonrpc\"") == std::string::npos ||
            body.find("\"2.0\"") == std::string::npos) {
            res.set_content(jrpcError(id, -32600, "Invalid Request"), "application/json");
            return;
        }

        if (method.empty()) {
            res.set_content(jrpcError(id, -32600, "Missing method"), "application/json");
            return;
        }

        // Extract params (nested object)
        std::string params_email = extractString(body, "email");
        std::string params_otp = extractString(body, "otp");
        std::string params_refresh_token = extractString(body, "refresh_token");
        std::string params_user_id = extractString(body, "user_id");
        std::string params_jwt = extractString(body, "jwt");
        std::string params_role = extractString(body, "role");

        // Route to handlers
        if (method == "register") {
            jwta::rpc::RegisterRequest req;
            req.email = params_email;

            if (req.email.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing email"), "application/json");
                return;
            }

            auto resp = service.handleRegister(req);

            std::ostringstream result;
            result << "{\"ok\":" << (resp.ok ? "true" : "false");
            if (resp.ok) {
                result << ",\"expires\":" << resp.expires;
            }
            result << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "verify") {
            jwta::rpc::VerifyRequest req;
            req.email = params_email;
            req.otp = params_otp;

            if (req.email.empty() || req.otp.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing email or otp"), "application/json");
                return;
            }

            auto resp = service.handleVerify(req);

            if (resp.jwt.empty()) {
                res.set_content(jrpcError(id, -32001, "Verification failed"), "application/json");
                return;
            }

            std::ostringstream result;
            result << "{";
            result << "\"jwt\":" << jsonString(resp.jwt) << ",";
            result << "\"refresh_token\":" << jsonString(resp.refresh_token) << ",";
            result << "\"user_id\":" << jsonString(resp.user_id) << ",";
            result << "\"role\":" << jsonString(resp.role);
            result << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "login") {
            jwta::rpc::LoginRequest req;
            req.email = params_email;

            if (req.email.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing email"), "application/json");
                return;
            }

            auto resp = service.handleLogin(req);

            std::ostringstream result;
            result << "{\"ok\":" << (resp.ok ? "true" : "false");
            if (resp.ok) {
                result << ",\"expires\":" << resp.expires;
            }
            result << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "refresh") {
            jwta::rpc::RefreshRequest req;
            req.refresh_token = params_refresh_token;

            if (req.refresh_token.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing refresh_token"), "application/json");
                return;
            }

            auto resp = service.handleRefresh(req);

            if (resp.jwt.empty()) {
                res.set_content(jrpcError(id, -32001, "Refresh failed"), "application/json");
                return;
            }

            std::ostringstream result;
            result << "{\"jwt\":" << jsonString(resp.jwt) << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "find") {
            jwta::rpc::FindRequest req;
            req.jwt = params_jwt;
            req.email = params_email;

            if (req.jwt.empty() || req.email.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing jwt or email"), "application/json");
                return;
            }

            auto resp = service.handleFind(req);

            if (resp.user_id.empty()) {
                res.set_content(jrpcError(id, -32001, "Not found or unauthorized"), "application/json");
                return;
            }

            std::ostringstream result;
            result << "{";
            result << "\"user_id\":" << jsonString(resp.user_id) << ",";
            result << "\"email\":" << jsonString(resp.email) << ",";
            result << "\"tier\":" << jsonString(resp.tier) << ",";
            result << "\"role\":" << jsonString(resp.role) << ",";
            result << "\"locked\":" << (resp.locked ? "true" : "false") << ",";
            result << "\"created_at\":" << resp.created_at;
            result << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "give") {
            jwta::rpc::GiveRequest req;
            req.jwt = params_jwt;
            req.user_id = params_user_id;
            req.role = params_role;

            if (req.jwt.empty() || req.user_id.empty() || req.role.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing jwt, user_id, or role"), "application/json");
                return;
            }

            auto resp = service.handleGive(req);

            std::ostringstream result;
            result << "{\"ok\":" << (resp.ok ? "true" : "false") << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "take") {
            jwta::rpc::TakeRequest req;
            req.jwt = params_jwt;
            req.user_id = params_user_id;

            if (req.jwt.empty() || req.user_id.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing jwt or user_id"), "application/json");
                return;
            }

            auto resp = service.handleTake(req);

            std::ostringstream result;
            result << "{\"ok\":" << (resp.ok ? "true" : "false") << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "lock") {
            jwta::rpc::LockRequest req;
            req.jwt = params_jwt;
            req.user_id = params_user_id;

            if (req.jwt.empty() || req.user_id.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing jwt or user_id"), "application/json");
                return;
            }

            auto resp = service.handleLock(req);

            std::ostringstream result;
            result << "{\"ok\":" << (resp.ok ? "true" : "false") << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "free") {
            jwta::rpc::FreeRequest req;
            req.jwt = params_jwt;
            req.user_id = params_user_id;

            if (req.jwt.empty() || req.user_id.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing jwt or user_id"), "application/json");
                return;
            }

            auto resp = service.handleFree(req);

            std::ostringstream result;
            result << "{\"ok\":" << (resp.ok ? "true" : "false") << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "drop") {
            jwta::rpc::DropRequest req;
            req.jwt = params_jwt;
            req.user_id = params_user_id;

            if (req.jwt.empty() || req.user_id.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing jwt or user_id"), "application/json");
                return;
            }

            auto resp = service.handleDrop(req);

            std::ostringstream result;
            result << "{\"ok\":" << (resp.ok ? "true" : "false") << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else if (method == "info") {
            jwta::rpc::InfoRequest req;
            req.jwt = params_jwt;

            if (req.jwt.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing jwt"), "application/json");
                return;
            }

            auto resp = service.handleInfo(req);

            if (resp.total_users == 0 && resp.users_none == 0) {
                // Could be unauthorized - check if genuinely empty or just failed auth
                res.set_content(jrpcError(id, -32001, "Unauthorized or no data"), "application/json");
                return;
            }

            std::ostringstream result;
            result << "{";
            result << "\"total_users\":" << resp.total_users << ",";
            result << "\"users_none\":" << resp.users_none << ",";
            result << "\"users_play\":" << resp.users_play << ",";
            result << "\"users_hero\":" << resp.users_hero << ",";
            result << "\"users_pqtr\":" << resp.users_pqtr;
            result << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else {
            res.set_content(jrpcError(id, -32601, "Method not found"), "application/json");
        }
    });

    // Bootstrap webhook endpoint (called by Mailgun Route when boot@pqtr.ai receives email)
    // Mailgun forwards the email body which contains the bootstrap token
    svr.Post("/boot", [&service, &cfg, &store](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");

        // Mailgun forwards POST with form data containing the email body
        // The "body-plain" field contains the plaintext email body
        std::string body_plain;
        if (req.has_param("body-plain")) {
            body_plain = req.get_param_value("body-plain");
        } else {
            // Fallback: try to find token in raw body
            body_plain = req.body;
        }

        // Extract bootstrap token from email body
        // Token format: 64 hex characters
        std::string token;
        for (size_t i = 0; i + 64 <= body_plain.size(); ++i) {
            bool valid = true;
            for (size_t j = 0; j < 64; ++j) {
                char c = body_plain[i + j];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                token = body_plain.substr(i, 64);
                // Convert to lowercase
                for (char& c : token) {
                    if (c >= 'A' && c <= 'F') c = c - 'A' + 'a';
                }
                break;
            }
        }

        if (token.empty()) {
            std::cerr << "[BOOT] No token found in request" << std::endl;
            res.set_content("{\"ok\":false,\"error\":\"no_token\"}", "application/json");
            return;
        }

        // Verify token
        if (!service.verifyBootstrapToken(token)) {
            std::cerr << "[BOOT] Invalid token" << std::endl;
            res.set_content("{\"ok\":false,\"error\":\"invalid_token\"}", "application/json");
            return;
        }

        // Token verified - promote admin_email to PQTR if they exist
        if (!cfg.admin_email.empty()) {
            auto user = store.getUserByEmail(cfg.admin_email);
            if (user) {
                store.updateUserRole(user->id, "PQTR");
                std::cout << "[BOOT] Admin bootstrapped: " << cfg.admin_email << std::endl;
            } else {
                std::cout << "[BOOT] Token verified, admin will get PQTR on register" << std::endl;
            }
        }

        res.set_content("{\"ok\":true}", "application/json");
    });

    // Start server
    std::cout << "[JWTA] Listening on " << cfg.host << ":" << cfg.port << std::endl;
    std::cout << "[JWTA] Endpoints: GET /health, POST /rpc, POST /boot" << std::endl;

    if (!svr.listen(cfg.host, cfg.port)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    return 0;
}
