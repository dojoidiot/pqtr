// server.cpp
// JWTA HTTP server with JRPC (JSON-RPC 2.0) endpoints

#include "jwta.hpp"
#include "sqlite_store.hpp"
#include "httplib.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace {

// Config structure
struct Config {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string db_path = "var/jwta.db";

    // SMTP (for future use)
    std::string smtp_host;
    int smtp_port = 587;
    std::string smtp_user;
    std::string smtp_pass;
    std::string smtp_from;
};

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

    // SMTP config
    if (!(s = getString("smtp_host")).empty()) cfg.smtp_host = s;
    // For nested "smtp.host", search after "smtp"
    size_t smtp_pos = json.find("\"smtp\"");
    if (smtp_pos != std::string::npos) {
        std::string smtp_section = json.substr(smtp_pos);
        auto getSmtpString = [&smtp_section](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\"";
            size_t pos = smtp_section.find(search);
            if (pos == std::string::npos) return "";
            pos = smtp_section.find(':', pos);
            if (pos == std::string::npos) return "";
            pos = smtp_section.find('"', pos);
            if (pos == std::string::npos) return "";
            pos++;
            std::string result;
            while (pos < smtp_section.size() && smtp_section[pos] != '"') {
                result += smtp_section[pos++];
            }
            return result;
        };
        auto getSmtpInt = [&smtp_section](const std::string& key) -> int {
            std::string search = "\"" + key + "\"";
            size_t pos = smtp_section.find(search);
            if (pos == std::string::npos) return 0;
            pos = smtp_section.find(':', pos);
            if (pos == std::string::npos) return 0;
            pos++;
            while (pos < smtp_section.size() && !isdigit(smtp_section[pos])) pos++;
            return std::atoi(smtp_section.c_str() + pos);
        };

        if (!(s = getSmtpString("host")).empty()) cfg.smtp_host = s;
        if ((i = getSmtpInt("port")) > 0) cfg.smtp_port = i;
        if (!(s = getSmtpString("user")).empty()) cfg.smtp_user = s;
        if (!(s = getSmtpString("pass")).empty()) cfg.smtp_pass = s;
        if (!(s = getSmtpString("from")).empty()) cfg.smtp_from = s;
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

// Console mailer (placeholder - replace with real email service)
class ConsoleMailer : public jwta::Mailer {
public:
    bool sendOtp(const std::string& email, const std::string& otp) override {
        std::cout << "[MAIL] OTP " << otp << " -> " << email << std::endl;
        return true;
    }
};

} // anonymous namespace

int main(int argc, char* argv[]) {
    // Help
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        std::cout << "JWTA Server - JWT Web Auth\n\n";
        std::cout << "Usage: jwta [config_file]\n\n";
        std::cout << "Config file: etc/jwta.json (default)\n";
        std::cout << "Environment:\n";
        std::cout << "  JWTA_MASTER_KEY  Master encryption key (64 hex chars, required)\n";
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

    // Initialize store
    jwta::SqliteStore store;
    if (!store.open(cfg.db_path)) {
        std::cerr << "Failed to open database: " << cfg.db_path << std::endl;
        return 1;
    }
    std::cout << "[JWTA] Database: " << cfg.db_path << std::endl;

    // Initialize service
    ConsoleMailer mailer;
    jwta::Service service(store, mailer);

    if (!service.init()) {
        std::cerr << "Failed to initialize service" << std::endl;
        return 1;
    }
    std::cout << "[JWTA] Master key: " << (service.hasMasterKey() ? "loaded" : "NOT SET (keys unencrypted!)") << std::endl;

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
            result << "\"pubkey_hex\":" << jsonString(resp.pubkey_hex);
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

        } else if (method == "pubkey") {
            jwta::rpc::PubkeyRequest req;
            req.user_id = params_user_id;

            if (req.user_id.empty()) {
                res.set_content(jrpcError(id, -32602, "Missing user_id"), "application/json");
                return;
            }

            auto resp = service.handlePubkey(req);

            if (resp.pubkey_hex.empty()) {
                res.set_content(jrpcError(id, -32001, "User not found"), "application/json");
                return;
            }

            std::ostringstream result;
            result << "{\"pubkey_hex\":" << jsonString(resp.pubkey_hex) << "}";

            res.set_content(jrpcResult(id, result.str()), "application/json");

        } else {
            res.set_content(jrpcError(id, -32601, "Method not found"), "application/json");
        }
    });

    // Start server
    std::cout << "[JWTA] Listening on " << cfg.host << ":" << cfg.port << std::endl;
    std::cout << "[JWTA] Endpoints: GET /health, POST /rpc" << std::endl;

    if (!svr.listen(cfg.host, cfg.port)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    return 0;
}
