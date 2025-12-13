// base.cpp - BASE server entry point

#include "base.hpp"
#include "httplib.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <memory>

namespace {

struct Config {
    std::string host;
    int port = 0;
    std::string jrpc_path;
    std::string boot_path;
    std::string admin_email;
    std::string boot_email;
    std::string otp_from;
    std::string otp_text;
    // sqlite
    std::string sqlite_file;
    // mailgun
    std::string mailgun_otp_skey;      // sending key from config (keyring lookup name)
    std::string mailgun_secret;        // secret from keyring (looked up by otp_skey)
    std::string mailgun_domain;
    std::string mailgun_region;
};

std::string loadSecret(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string secret;
    std::getline(f, secret);
    // Trim trailing whitespace/newline
    while (!secret.empty() && (secret.back() == '\n' || secret.back() == '\r' || secret.back() == ' '))
        secret.pop_back();
    return secret;
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
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
                if (json[pos] == 'n') { result += '\n'; pos++; continue; }
                if (json[pos] == 'r') { result += '\r'; pos++; continue; }
                if (json[pos] == 't') { result += '\t'; pos++; continue; }
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

    auto getNestedString = [&json](const std::string& obj, const std::string& key) -> std::string {
        size_t obj_pos = json.find("\"" + obj + "\"");
        if (obj_pos == std::string::npos) return "";
        std::string sub = json.substr(obj_pos);
        std::string search = "\"" + key + "\"";
        size_t pos = sub.find(search);
        if (pos == std::string::npos) return "";
        pos = sub.find(':', pos);
        if (pos == std::string::npos) return "";
        pos = sub.find('"', pos);
        if (pos == std::string::npos) return "";
        pos++;
        std::string result;
        while (pos < sub.size() && sub[pos] != '"') result += sub[pos++];
        return result;
    };

    std::string s;
    int i;

    if (!(s = getString("host")).empty()) cfg.host = s;
    if ((i = getInt("port")) > 0) cfg.port = i;
    if (!(s = getString("jrpc_path")).empty()) cfg.jrpc_path = s;
    if (!(s = getString("boot_path")).empty()) cfg.boot_path = s;
    if (!(s = getString("admin_email")).empty()) cfg.admin_email = s;
    if (!(s = getString("boot_email")).empty()) cfg.boot_email = s;
    if (!(s = getString("otp_from")).empty()) cfg.otp_from = s;
    if (!(s = getString("otp_text")).empty()) cfg.otp_text = s;

    // sqlite.file
    if (!(s = getNestedString("sqlite", "file")).empty()) cfg.sqlite_file = s;

    // mailgun.domain, mailgun.region, mailgun.otp_skey (keyring lookup name)
    if (!(s = getNestedString("mailgun", "domain")).empty()) cfg.mailgun_domain = s;
    if (!(s = getNestedString("mailgun", "region")).empty()) cfg.mailgun_region = s;
    if (!(s = getNestedString("mailgun", "otp_skey")).empty()) cfg.mailgun_otp_skey = s;

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
    std::string config_path;
    std::string data_area;
    std::string wasm_root;
    bool test_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "BASE Server - Static site + JWT Auth\n\n"
                      << "Usage: base --info-file <config.json> --data-area <path> [--wasm-root <path>] [--test]\n\n"
                      << "Options:\n"
                      << "  --info-file <path>   Config file (required)\n"
                      << "  --data-area <path>   Data directory (required)\n"
                      << "  --wasm-root <path>   WASM app directory to serve (optional)\n"
                      << "  --test               Test mode: print OTP to console (no email)\n"
                      << "  -h, --help           Show this help\n";
            return 0;
        } else if (arg == "--info-file" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--data-area" && i + 1 < argc) {
            data_area = argv[++i];
            if (!data_area.empty() && data_area.back() != '/') data_area += '/';
        } else if (arg == "--wasm-root" && i + 1 < argc) {
            wasm_root = argv[++i];
        } else if (arg == "--test") {
            test_mode = true;
        }
    }

    if (config_path.empty()) {
        std::cerr << "[BASE] Error: --info-file required\n";
        return 1;
    }
    if (data_area.empty()) {
        std::cerr << "[BASE] Error: --data-area required\n";
        return 1;
    }

    Config cfg;
    if (!loadConfig(config_path, cfg)) {
        std::cerr << "[BASE] Error: Failed to load config: " << config_path << std::endl;
        return 1;
    }
    std::cout << "[BASE] Config: " << config_path << std::endl;

    // Load mailgun secret from file (otp_skey is the file path)
    std::string s;
    if (!cfg.mailgun_otp_skey.empty()) {
        std::string secret_path = cfg.mailgun_otp_skey;
        // If relative path, resolve relative to data_area
        if (!secret_path.empty() && secret_path[0] != '/') {
            secret_path = data_area + secret_path;
        }
        if (!(s = loadSecret(secret_path)).empty()) {
            cfg.mailgun_secret = s;
        }
    }

    // Validate required config
    if (cfg.host.empty()) {
        std::cerr << "[BASE] Error: host required in config" << std::endl;
        return 1;
    }
    if (cfg.port <= 0) {
        std::cerr << "[BASE] Error: port required in config" << std::endl;
        return 1;
    }
    if (cfg.jrpc_path.empty()) {
        std::cerr << "[BASE] Error: jrpc_path required in config" << std::endl;
        return 1;
    }
    if (cfg.sqlite_file.empty()) {
        std::cerr << "[BASE] Error: sqlite.file required in config" << std::endl;
        return 1;
    }
    // Mailgun config only required if not in test mode
    if (!test_mode) {
        if (cfg.mailgun_otp_skey.empty()) {
            std::cerr << "[BASE] Error: mailgun.otp_skey required in config (path to secret file)" << std::endl;
            return 1;
        }
        if (cfg.mailgun_secret.empty()) {
            std::cerr << "[BASE] Error: failed to load secret from: " << cfg.mailgun_otp_skey << std::endl;
            return 1;
        }
        if (cfg.mailgun_domain.empty()) {
            std::cerr << "[BASE] Error: mailgun.domain required in config" << std::endl;
            return 1;
        }
        if (cfg.mailgun_region.empty()) {
            std::cerr << "[BASE] Error: mailgun.region required in config" << std::endl;
            return 1;
        }
        if (cfg.otp_from.empty()) {
            std::cerr << "[BASE] Error: otp_from required in config" << std::endl;
            return 1;
        }
        if (cfg.otp_text.empty()) {
            std::cerr << "[BASE] Error: otp_text required in config" << std::endl;
            return 1;
        }
    }

    std::string db_path = data_area + cfg.sqlite_file;
    auto store = base::createStore(db_path);
    if (!store) {
        std::cerr << "[BASE] Error: Failed to open database: " << db_path << std::endl;
        return 1;
    }
    std::cout << "[BASE] Database: " << db_path << std::endl;

    std::unique_ptr<base::Mailer> mailer;
    if (test_mode) {
        mailer = base::createConsoleMailer();
        std::cout << "[BASE] Mailer: CONSOLE (test mode)" << std::endl;
    } else {
        mailer = base::createMailer(cfg.mailgun_secret, cfg.mailgun_domain, cfg.otp_from, cfg.otp_text, cfg.mailgun_region);
        std::cout << "[BASE] Mailer: " << cfg.mailgun_domain << " (" << cfg.mailgun_region << ")" << std::endl;
    }

    base::Service service(*store, *mailer);
    if (!service.init()) {
        std::cerr << "[BASE] Error: Failed to initialize service" << std::endl;
        return 1;
    }
    service.setDataArea(data_area);

    if (!cfg.admin_email.empty()) {
        service.setAdminEmail(cfg.admin_email);
        std::cout << "[BASE] Admin: " << cfg.admin_email << std::endl;
    }

    if (!cfg.boot_email.empty() && store->countUsersByRole("PQTR") == 0) {
        std::cout << "[BASE] No admin found, sending bootstrap to " << cfg.boot_email << std::endl;
        if (service.sendBootstrapEmail(cfg.boot_email)) {
            std::cout << "[BASE] Bootstrap token sent" << std::endl;
        } else {
            std::cerr << "[BASE] Failed to send bootstrap email" << std::endl;
        }
    }

    httplib::Server svr;

    svr.Post(cfg.jrpc_path, [&service](const httplib::Request& req, httplib::Response& res) {
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
              << ",\"user_id\":" << jsonString(resp.user_id) << ",\"itag\":" << jsonString(resp.itag)
              << ",\"role\":" << jsonString(resp.role) << "}";
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

        } else if (method == "list") {
            std::string p_name = extractString(body, "name");  // Optional
            if (p_jwt.empty()) { res.set_content(jrpcError(id, -32602, "Missing jwt"), "application/json"); return; }
            auto resp = service.handleList({p_jwt, p_name});
            if (!resp.ok) { res.set_content(jrpcError(id, -32001, "Unauthorized"), "application/json"); return; }
            std::ostringstream r;
            r << "{\"items\":[";
            for (size_t i = 0; i < resp.items.size(); i++) {
                if (i > 0) r << ",";
                r << "\"" << resp.items[i] << "\"";
            }
            r << "]}";
            res.set_content(jrpcResult(id, r.str()), "application/json");

        } else if (method == "test") {
            std::string p_name = extractString(body, "name");
            if (p_jwt.empty() || p_name.empty()) { res.set_content(jrpcError(id, -32602, "Missing jwt or name"), "application/json"); return; }
            auto resp = service.handleTest({p_jwt, p_name});
            if (!resp.ok) { res.set_content(jrpcError(id, -32001, "Unauthorized"), "application/json"); return; }
            res.set_content(jrpcResult(id, std::string("{\"exists\":") + (resp.exists ? "true" : "false") + "}"), "application/json");

        } else {
            res.set_content(jrpcError(id, -32601, "Method not found"), "application/json");
        }
    });

    if (!cfg.boot_path.empty()) {
        svr.Post(cfg.boot_path, [&service, &cfg, &store](const httplib::Request& req, httplib::Response& res) {
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
        std::cout << "[BASE] Boot: " << cfg.boot_path << std::endl;
    }

    std::cout << "[BASE] JRPC: " << cfg.jrpc_path << std::endl;

    // Binary push endpoint: POST /push?name=xxx&file=xxx with Authorization header
    svr.Post("/push", [&service](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");

        // Get JWT from Authorization header
        std::string auth = req.get_header_value("Authorization");
        std::string jwt;
        if (auth.substr(0, 7) == "Bearer ") {
            jwt = auth.substr(7);
        }
        if (jwt.empty()) {
            res.set_content("{\"ok\":false,\"error\":\"Missing authorization\"}", "application/json");
            return;
        }

        // Get params from query string
        std::string name = req.get_param_value("name");
        std::string file = req.get_param_value("file");
        if (name.empty() || file.empty()) {
            res.set_content("{\"ok\":false,\"error\":\"Missing name or file param\"}", "application/json");
            return;
        }

        // Binary body
        if (req.body.empty()) {
            res.set_content("{\"ok\":false,\"error\":\"Empty body\"}", "application/json");
            return;
        }

        auto resp = service.handlePush({jwt, name, file, req.body});
        if (!resp.ok) {
            res.set_content("{\"ok\":false,\"error\":\"" + resp.error + "\"}", "application/json");
            return;
        }
        res.set_content("{\"ok\":true}", "application/json");
    });
    std::cout << "[BASE] Push: /push" << std::endl;

    // Binary pull endpoint: GET /pull?name=xxx&file=xxx with Authorization header
    svr.Get("/pull", [&service, &data_area](const httplib::Request& req, httplib::Response& res) {
        // Get JWT from Authorization header
        std::string auth = req.get_header_value("Authorization");
        std::string jwt;
        if (auth.substr(0, 7) == "Bearer ") {
            jwt = auth.substr(7);
        }
        if (jwt.empty()) {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
            return;
        }

        // Get name and file from query params
        std::string name = req.get_param_value("name");
        std::string file = req.get_param_value("file");
        if (name.empty() || file.empty()) {
            res.status = 400;
            res.set_content("Missing name or file param", "text/plain");
            return;
        }

        // Verify JWT and get claims
        auto claims = base::jwt::decode(jwt, service.getSigningPubkey());
        if (!claims || claims->itag.empty()) {
            res.status = 401;
            res.set_content("Invalid JWT", "text/plain");
            return;
        }

        // Validate name and file to prevent path traversal
        auto validPath = [](const std::string& s) {
            if (s.empty() || s.size() > 255) return false;
            if (s.find("..") != std::string::npos) return false;
            if (s.find('/') != std::string::npos) return false;
            if (s.find('\\') != std::string::npos) return false;
            return true;
        };
        if (!validPath(name) || !validPath(file)) {
            res.status = 400;
            res.set_content("Invalid name or file", "text/plain");
            return;
        }

        // Build file path: var/LABS/<itag>/pipe/<name>/<file>
        std::string file_path = data_area + "LABS/" + claims->itag + "/pipe/" + name + "/" + file;

        // Read file
        FILE* f = fopen(file_path.c_str(), "rb");
        if (!f) {
            res.status = 404;
            res.set_content("File not found", "text/plain");
            return;
        }

        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);

        std::string content(size, '\0');
        size_t read = fread(&content[0], 1, size, f);
        fclose(f);

        if (read != size) {
            res.status = 500;
            res.set_content("Read error", "text/plain");
            return;
        }

        // Determine content type
        std::string content_type = "application/octet-stream";
        if (file.size() > 5 && file.substr(file.size() - 5) == ".json") {
            content_type = "application/json";
        }

        res.set_content(content, content_type);
    });
    std::cout << "[BASE] Pull: /pull" << std::endl;

    // Redirect root to labs.html
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/labs.html");
    });

    // Static file serving
    if (!wasm_root.empty()) {
        if (svr.set_mount_point("/", wasm_root)) {
            std::cout << "[BASE] WWW: " << wasm_root << std::endl;
        } else {
            std::cerr << "[BASE] Warning: Failed to mount www: " << wasm_root << std::endl;
        }
    }

    std::cout << "[BASE] Listening on " << cfg.host << ":" << cfg.port << std::endl;
    if (!svr.listen(cfg.host, cfg.port)) {
        std::cerr << "[BASE] Failed to start server" << std::endl;
        return 1;
    }
    return 0;
}
