// wasm.cpp - Static WASM file serving

#include "base.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

namespace base {
namespace wasm {

bool setup(httplib::Server& svr, const std::string& wasm_root) {
    if (wasm_root.empty()) {
        return false;
    }

    // Serve .wasm files with correct MIME type and caching
    svr.Get("/.*\\.wasm", [wasm_root](const httplib::Request& req, httplib::Response& res) {
        std::string path = wasm_root + req.path;
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            res.status = 404;
            return;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        res.set_content(ss.str(), "application/wasm");
        res.set_header("Cache-Control", "public, max-age=31536000");  // Cache for 1 year
    });

    // Serve .js files with caching
    svr.Get("/.*\\.js", [wasm_root](const httplib::Request& req, httplib::Response& res) {
        std::string path = wasm_root + req.path;
        std::ifstream file(path);
        if (!file) {
            res.status = 404;
            return;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        res.set_content(ss.str(), "application/javascript");
        res.set_header("Cache-Control", "no-cache");  // Check for updates
    });

    // Mount the static directory
    if (!svr.set_mount_point("/", wasm_root)) {
        std::cerr << "[BASE] Warning: Failed to mount www: " << wasm_root << std::endl;
        return false;
    }

    std::cout << "[BASE] WWW: " << wasm_root << std::endl;
    return true;
}

} // namespace wasm
} // namespace base
