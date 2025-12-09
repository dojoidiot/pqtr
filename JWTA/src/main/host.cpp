// host.cpp
// JWTA HTTP server with JRPC endpoints

#include "jwta.hpp"
#include "httplib.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <memory>
#include <keyutils.h>

namespace {

struct Config {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string db_path = "var/jwta.db";
    std::string admin_email;
    std::string boot_email;
    std::string mailgun_api_key;
    std::string mailgun_domain;
    std::string mailgun_from;
    std::string mailgun_region = "us";
};

std::string loadFromKeyring(const std::string& name) {
    std::string desc = "jwta:" + name;
    key_serial_t key = request_key("user", desc.c_str(), nullptr, KEY_SPEC_USER_KEYRING);
    if (key < 0) return "";

    long size = keyctl_read(key, nullptr, 0);
    if (size <= 0) return "";

    std::string value(size, '\0');
    if (keyctl_read(key, value.data(), size) != size) return "";
    return value;
}

bool loadConfig(const std::string& path, Config& cfg) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::stringstream buf;
    buf << f.rdbuf();
    std::string json = buf.str();

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
            if (json[pos] == '\\' && pos + 1 < json.size()) pos++;
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

    size_t mg_pos = json.find("\"mailgun\"");
    if (mg_pos != std::string::npos) {
        std::string mg = json.substr(mg_pos);
        auto getMg = [&mg](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\"";
            size_t pos = mg.find(search);
            if (pos == std::string::npos) return "";
            pos = mg.find(':', pos);
            if (pos == std::string::npos) return "";
            pos = mg.find('"', pos);
            if (pos == std::string::npos) return "";
            pos++;
            std::string result;
            while (pos < mg.size() && mg[pos] != '"') result += mg[pos++];
            return result;
        };
        if (!(s = getMg("api_key")).empty()) cfg.mailgun_api_key = s;
        if (!(s = getMg("domain")).empty()) cfg.mailgun_domain = s;
        if (!(s = getMg("from")).empty()) cfg.mailgun_from = s;
        if (!(s = getMg("region")).empty()) cfg.mailgun_region = s;
    }
    return true;
}

std::string jsonString(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out + "\"";
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
    std::string search = "\"id\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "null";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "null";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "null";
    if (json[pos] == '"') return jsonString(extractString(json, "id"));
    if (json[pos] == 'n') return "null";
    std::string num;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-')) num += json[pos++];
    return num.empty() ? "null" : num;
}

std::string jrpcResult(const std::string& id, const std::string& result) {
    return "{\"jsonrpc\":\"2.0\",\"result\":" + result + ",\"id\":" + id + "}";
}

std::string jrpcError(const std::string& id, int code, const std::string& message) {
    return "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":" + std::to_string(code) +
           ",\"message\":" + jsonString(message) + "},\"id\":" + id + "}";
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    std::string config_path = "etc/jwta.json";
    std::string data_area;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "JWTA Server - JWT Web Auth\n\n"
                      << "Usage: jwta [options]\n\n"
                      << "Options:\n"
                      << "  --info-file <path>   Config file (default: etc/jwta.json)\n"
                      << "  --data-area <path>   Data directory\n"
                      << "  -h, --help           Show this help\n";
            return 0;
        } else if (arg == "--info-file" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--data-area" && i + 1 < argc) {
            data_area = argv[++i];
            if (!data_area.empty() && data_area.back() != '/') data_area += '/';
        } else if (arg[0] != '-') {
            config_path = arg;
        }
    }

    Config cfg;
    if (loadConfig(config_path, cfg)) {
        std::cout << "[JWTA] Config: " << config_path << std::endl;
    } else {
        std::cout << "[JWTA] Config: " << config_path << " (not found, using defaults)" << std::endl;
    }

    if (const char* env = std::getenv("JWTA_HOST")) cfg.host = env;
    if (const char* env = std::getenv("JWTA_PORT")) cfg.port = std::atoi(env);
    if (const char* env = std::getenv("JWTA_DB_PATH")) cfg.db_path = env;

    if (!data_area.empty()) {
        std::string db_filename = cfg.db_path;
        size_t slash = db_filename.rfind('/');
        if (slash != std::string::npos) db_filename = db_filename.substr(slash + 1);
        cfg.db_path = data_area + db_filename;
        std::cout << "[JWTA] Data area: " << data_area << std::endl;
    }

    std::string s;
    if (!(s = loadFromKeyring("mailgun_api_key")).empty()) {
        cfg.mailgun_api_key = s;
        std::cout << "[JWTA] Keyring: mailgun_api_key" << std::endl;
    }
    if (!(s = loadFromKeyring("mailgun_domain")).empty()) {
        cfg.mailgun_domain = s;
        std::cout << "[JWTA] Keyring: mailgun_domain" << std::endl;
    }
    if (!(s = loadFromKeyring("mailgun_from")).empty()) {
        cfg.mailgun_from = s;
        std::cout << "[JWTA] Keyring: mailgun_from" << std::endl;
    }
    if (!(s = loadFromKeyring("mailgun_region")).empty()) {
        cfg.mailgun_region = s;
        std::cout << "[JWTA] Keyring: mailgun_region" << std::endl;
    }

    // Require mailgun config
    if (cfg.mailgun_api_key.empty()) {
        std::cerr << "[JWTA] Error: mailgun_api_key not configured" << std::endl;
        std::cerr << "[JWTA] Set via keyring: keyctl add user \"jwta:mailgun_api_key\" \"KEY\" @u" << std::endl;
        return 1;
    }

    auto store = jwta::createStore(cfg.db_path);
    if (!store) {
        std::cerr << "[JWTA] Error: Failed to open database: " << cfg.db_path << std::endl;
        return 1;
    }
    std::cout << "[JWTA] Database: " << cfg.db_path << std::endl;

    auto mailer = jwta::createMailer(cfg.mailgun_api_key, cfg.mailgun_domain, cfg.mailgun_from, cfg.mailgun_region);
    std::cout << "[JWTA] Mailer: " << cfg.mailgun_domain << " (" << cfg.mailgun_region << ")" << std::endl;

    jwta::Service service(*store, *mailer);
    if (!service.init()) {
        std::cerr << "[JWTA] Error: Failed to initialize service" << std::endl;
        return 1;
    }

    if (!cfg.admin_email.empty()) {
        service.setAdminEmail(cfg.admin_email);
        std::cout << "[JWTA] Admin: " << cfg.admin_email << std::endl;
    }

    if (!cfg.boot_email.empty() && store->countUsersByRole("PQTR") == 0) {
        std::cout << "[JWTA] No admin found, sending bootstrap to " << cfg.boot_email << std::endl;
        if (service.sendBootstrapEmail(cfg.boot_email)) {
            std::cout << "[JWTA] Bootstrap token sent" << std::endl;
        } else {
            std::cerr << "[JWTA] Failed to send bootstrap email" << std::endl;
        }
    }

    httplib::Server svr;

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    svr.Post("/rpc", [&service](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");
        const std::string& body = req.body;
        std::string id = extractId(body);
        std::string method = extractString(body, "method");

        if (body.find("\"jsonrpc\"") == std::string::npos || body.find("\"2.0\"") == std::string::npos) {
            res.set_content(jrpcError(id, -32600, "Invalid Request"), "application/json");
            return;
        }
        if (method.empty()) {
            res.set_content(jrpcError(id, -32600, "Missing method"), "application/json");
            return;
        }

        std::string p_email = extractString(body, "email");
        std::string p_otp = extractString(body, "otp");
        std::string p_refresh = extractString(body, "refresh_token");
        std::string p_user_id = extractString(body, "user_id");
        std::string p_jwt = extractString(body, "jwt");
        std::string p_role = extractString(body, "role");

        if (method == "register") {
            if (p_email.empty()) { res.set_content(jrpcError(id, -32602, "Missing email"), "application/json"); return; }
            auto resp = service.handleRegister({p_email});
            std::ostringstream r; r << "{\"ok\":" << (resp.ok ? "true" : "false");
            if (resp.ok) r << ",\"expires\":" << resp.expires;
            r << "}";
            res.set_content(jrpcResult(id, r.str()), "application/json");

        } else if (method == "verify") {
            if (p_email.empty() || p_otp.empty()) { res.set_content(jrpcError(id, -32602, "Missing email or otp"), "application/json"); return; }
            auto resp = service.handleVerify({p_email, p_otp});
            if (resp.jwt.empty()) { res.set_content(jrpcError(id, -32001, "Verification failed"), "application/json"); return; }
            std::ostringstream r;
            r << "{\"jwt\":" << jsonString(resp.jwt) << ",\"refresh_token\":" << jsonString(resp.refresh_token)
              << ",\"user_id\":" << jsonString(resp.user_id) << ",\"role\":" << jsonString(resp.role) << "}";
            res.set_content(jrpcResult(id, r.str()), "application/json");

        } else if (method == "login") {
            if (p_email.empty()) { res.set_content(jrpcError(id, -32602, "Missing email"), "application/json"); return; }
            auto resp = service.handleLogin({p_email});
            std::ostringstream r; r << "{\"ok\":" << (resp.ok ? "true" : "false");
            if (resp.ok) r << ",\"expires\":" << resp.expires;
            r << "}";
            res.set_content(jrpcResult(id, r.str()), "application/json");

        } else if (method == "refresh") {
            if (p_refresh.empty()) { res.set_content(jrpcError(id, -32602, "Missing refresh_token"), "application/json"); return; }
            auto resp = service.handleRefresh({p_refresh});
            if (resp.jwt.empty()) { res.set_content(jrpcError(id, -32001, "Refresh failed"), "application/json"); return; }
            res.set_content(jrpcResult(id, "{\"jwt\":" + jsonString(resp.jwt) + "}"), "application/json");

        } else if (method == "find") {
            if (p_jwt.empty() || p_email.empty()) { res.set_content(jrpcError(id, -32602, "Missing jwt or email"), "application/json"); return; }
            auto resp = service.handleFind({p_jwt, p_email});
            if (resp.user_id.empty()) { res.set_content(jrpcError(id, -32001, "Not found"), "application/json"); return; }
            std::ostringstream r;
            r << "{\"user_id\":" << jsonString(resp.user_id) << ",\"email\":" << jsonString(resp.email)
              << ",\"tier\":" << jsonString(resp.tier) << ",\"role\":" << jsonString(resp.role)
              << ",\"locked\":" << (resp.locked ? "true" : "false") << ",\"created_at\":" << resp.created_at << "}";
            res.set_content(jrpcResult(id, r.str()), "application/json");

        } else if (method == "give") {
            if (p_jwt.empty() || p_user_id.empty() || p_role.empty()) { res.set_content(jrpcError(id, -32602, "Missing params"), "application/json"); return; }
            auto resp = service.handleGive({p_jwt, p_user_id, p_role});
            res.set_content(jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}"), "application/json");

        } else if (method == "take") {
            if (p_jwt.empty() || p_user_id.empty()) { res.set_content(jrpcError(id, -32602, "Missing params"), "application/json"); return; }
            auto resp = service.handleTake({p_jwt, p_user_id});
            res.set_content(jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}"), "application/json");

        } else if (method == "lock") {
            if (p_jwt.empty() || p_user_id.empty()) { res.set_content(jrpcError(id, -32602, "Missing params"), "application/json"); return; }
            auto resp = service.handleLock({p_jwt, p_user_id});
            res.set_content(jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}"), "application/json");

        } else if (method == "free") {
            if (p_jwt.empty() || p_user_id.empty()) { res.set_content(jrpcError(id, -32602, "Missing params"), "application/json"); return; }
            auto resp = service.handleFree({p_jwt, p_user_id});
            res.set_content(jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}"), "application/json");

        } else if (method == "drop") {
            if (p_jwt.empty() || p_user_id.empty()) { res.set_content(jrpcError(id, -32602, "Missing params"), "application/json"); return; }
            auto resp = service.handleDrop({p_jwt, p_user_id});
            res.set_content(jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}"), "application/json");

        } else if (method == "info") {
            if (p_jwt.empty()) { res.set_content(jrpcError(id, -32602, "Missing jwt"), "application/json"); return; }
            auto resp = service.handleInfo({p_jwt});
            if (resp.total_users == 0 && resp.users_none == 0) { res.set_content(jrpcError(id, -32001, "Unauthorized"), "application/json"); return; }
            std::ostringstream r;
            r << "{\"total_users\":" << resp.total_users << ",\"users_none\":" << resp.users_none
              << ",\"users_play\":" << resp.users_play << ",\"users_hero\":" << resp.users_hero
              << ",\"users_pqtr\":" << resp.users_pqtr << "}";
            res.set_content(jrpcResult(id, r.str()), "application/json");

        } else {
            res.set_content(jrpcError(id, -32601, "Method not found"), "application/json");
        }
    });

    svr.Post("/boot", [&service, &cfg, &store](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");
        std::string body_plain = req.has_param("body-plain") ? req.get_param_value("body-plain") : req.body;

        std::string token;
        for (size_t i = 0; i + 64 <= body_plain.size(); ++i) {
            bool valid = true;
            for (size_t j = 0; j < 64; ++j) {
                char c = body_plain[i + j];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) { valid = false; break; }
            }
            if (valid) {
                token = body_plain.substr(i, 64);
                for (char& c : token) if (c >= 'A' && c <= 'F') c = c - 'A' + 'a';
                break;
            }
        }

        if (token.empty()) { res.set_content("{\"ok\":false,\"error\":\"no_token\"}", "application/json"); return; }
        if (!service.verifyBootstrapToken(token)) { res.set_content("{\"ok\":false,\"error\":\"invalid_token\"}", "application/json"); return; }

        if (!cfg.admin_email.empty()) {
            auto user = store->getUserByEmail(cfg.admin_email);
            if (user) {
                store->updateUserRole(user->id, "PQTR");
                std::cout << "[BOOT] Admin: " << cfg.admin_email << std::endl;
            } else {
                std::cout << "[BOOT] Token verified, admin gets PQTR on register" << std::endl;
            }
        }
        res.set_content("{\"ok\":true}", "application/json");
    });

    std::cout << "[JWTA] Listening on " << cfg.host << ":" << cfg.port << std::endl;
    if (!svr.listen(cfg.host, cfg.port)) {
        std::cerr << "[JWTA] Failed to start server" << std::endl;
        return 1;
    }
    return 0;
}
