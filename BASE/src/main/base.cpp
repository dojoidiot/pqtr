// base.cpp - BASE server entry point

#include "base.hpp"
#include <iostream>
#include <cstdio>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

int main(int argc, char* argv[]) {
    std::string config_path;
    std::string data_area;
    std::string wasm_root;
    bool test_mode = false;

    // Parse arguments
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

    // Load configuration
    base::Config cfg;
    if (!base::loadConfig(config_path, cfg)) {
        std::cerr << "[BASE] Error: Failed to load config: " << config_path << std::endl;
        return 1;
    }
    std::cout << "[BASE] Config: " << config_path << std::endl;

    // Load mailgun secret from file
    if (!cfg.mailgun_otp_skey.empty()) {
        std::string secret_path = cfg.mailgun_otp_skey;
        if (!secret_path.empty() && secret_path[0] != '/') {
            secret_path = data_area + secret_path;
        }
        std::string s = base::loadSecret(secret_path);
        if (!s.empty()) {
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
    if (cfg.sqlite_file.empty()) {
        std::cerr << "[BASE] Error: sqlite.file required in config" << std::endl;
        return 1;
    }
    if (!test_mode) {
        if (cfg.mailgun_otp_skey.empty()) {
            std::cerr << "[BASE] Error: mailgun.otp_skey required in config" << std::endl;
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

    // Initialize database
    std::string db_path = data_area + cfg.sqlite_file;
    auto store = base::createStore(db_path);
    if (!store) {
        std::cerr << "[BASE] Error: Failed to open database: " << db_path << std::endl;
        return 1;
    }
    std::cout << "[BASE] Database: " << db_path << std::endl;

    if (test_mode) {
        store->clearAllRateLimits();
        std::cout << "[BASE] Rate limits: cleared (test mode)" << std::endl;
    }

    // Initialize mailer
    std::unique_ptr<base::Mailer> mailer;
    if (test_mode) {
        mailer = base::createConsoleMailer();
        std::cout << "[BASE] Mailer: CONSOLE (test mode)" << std::endl;
    } else {
        mailer = base::createMailer(cfg.mailgun_secret, cfg.mailgun_domain, cfg.otp_from, cfg.otp_text, cfg.mailgun_region);
        std::cout << "[BASE] Mailer: " << cfg.mailgun_domain << " (" << cfg.mailgun_region << ")" << std::endl;
    }

    // Initialize service
    base::Service service(*store, *mailer);
    if (!service.init()) {
        std::cerr << "[BASE] Error: Failed to initialize service" << std::endl;
        return 1;
    }
    service.setDataArea(data_area);

    // Create data directories
    if (!data_area.empty()) {
        std::string labs_dir = data_area + "LABS";
        if (mkdir(labs_dir.c_str(), 0755) == 0) {
            std::cout << "[BASE] Created: " << labs_dir << std::endl;
        } else if (errno == EEXIST) {
            std::cout << "[BASE] Data: " << labs_dir << std::endl;
        } else {
            std::cerr << "[BASE] Warning: Could not create " << labs_dir << ": " << strerror(errno) << std::endl;
        }
    }

    if (!cfg.admin_email.empty()) {
        service.setAdminEmail(cfg.admin_email);
        std::cout << "[BASE] Admin: " << cfg.admin_email << std::endl;
    }

    // Bootstrap admin if needed
    if (!cfg.boot_email.empty() && store->countUsersByRole("PQTR") == 0) {
        std::cout << "[BASE] No admin found, sending bootstrap to " << cfg.boot_email << std::endl;
        if (service.sendBootstrapEmail(cfg.boot_email)) {
            std::cout << "[BASE] Bootstrap token sent" << std::endl;
        } else {
            std::cerr << "[BASE] Failed to send bootstrap email" << std::endl;
        }
    }

    // Setup HTTP server
    httplib::Server svr;

    // JRPC endpoint
    svr.Post("/jrpc", [&service](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "application/json");
        std::string auth = req.get_header_value("Authorization");
        res.set_content(base::jrpc::handle(service, req.body, auth), "application/json");
    });
    std::cout << "[BASE] JRPC: /jrpc" << std::endl;

    // Bootstrap endpoint
    if (!cfg.boot_path.empty()) {
        svr.Post(cfg.boot_path, [&service, &cfg, &store](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Content-Type", "application/json");
            std::string body_plain = req.has_param("body-plain") ? req.get_param_value("body-plain") : req.body;

            // Extract 64-char hex token
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
                    for (char& c : token) if (c >= 'A' && c <= 'F') c = c - 'A' + 'a';
                    break;
                }
            }

            if (token.empty()) {
                res.set_content("{\"ok\":false,\"error\":\"no_token\"}", "application/json");
                return;
            }
            if (!service.verifyBootstrapToken(token)) {
                res.set_content("{\"ok\":false,\"error\":\"invalid_token\"}", "application/json");
                return;
            }

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

    // REST endpoints
    svr.Post("/push", [&service](const httplib::Request& req, httplib::Response& res) {
        base::rest::handlePush(service, req, res);
    });
    std::cout << "[BASE] Push: /push" << std::endl;

    svr.Get("/pull", [&service, &data_area](const httplib::Request& req, httplib::Response& res) {
        base::rest::handlePull(service, data_area, req, res);
    });
    std::cout << "[BASE] Pull: /pull" << std::endl;

    svr.Delete("/drop", [&service, &data_area](const httplib::Request& req, httplib::Response& res) {
        base::rest::handleDropPipe(service, data_area, req, res);
    });
    std::cout << "[BASE] Drop: /drop" << std::endl;

    // Root redirect
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/labs.html");
    });

    // Static WASM serving
    base::wasm::setup(svr, wasm_root);

    // Start server
    std::cout << "[BASE] Listening on " << cfg.host << ":" << cfg.port << " (v2-debug)" << std::endl;
    if (!svr.listen(cfg.host, cfg.port)) {
        std::cerr << "[BASE] Failed to start server" << std::endl;
        return 1;
    }
    return 0;
}
