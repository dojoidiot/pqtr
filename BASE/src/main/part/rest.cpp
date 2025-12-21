// rest.cpp - REST endpoint handlers (push/pull)

#include "base.hpp"
#include <cstdio>

namespace base {
namespace rest {

void handlePush(Service& service, const httplib::Request& req, httplib::Response& res) {
    res.set_header("Content-Type", "application/json");

    // Get JWT from Authorization header
    std::string auth = req.get_header_value("Authorization");
    std::string jwt;
    if (auth.substr(0, 7) == "Bearer ") {
        jwt = auth.substr(7);
    }
    if (jwt.empty()) {
        res.status = 401;
        res.set_content("{\"ok\":false,\"error\":\"Missing authorization\"}", "application/json");
        return;
    }

    // Get params from query string
    std::string name = req.get_param_value("name");
    std::string file = req.get_param_value("file");
    if (name.empty() || file.empty()) {
        res.status = 400;
        res.set_content("{\"ok\":false,\"error\":\"Missing name or file param\"}", "application/json");
        return;
    }

    // Binary body
    if (req.body.empty()) {
        res.status = 400;
        res.set_content("{\"ok\":false,\"error\":\"Empty body\"}", "application/json");
        return;
    }

    auto resp = service.handlePush({jwt, name, file, req.body});
    if (!resp.ok) {
        if (resp.error == "Unauthorized" || resp.error.find("Invalid") != std::string::npos) {
            res.status = 401;
        } else {
            res.status = 400;
        }
        res.set_content("{\"ok\":false,\"error\":\"" + resp.error + "\"}", "application/json");
        return;
    }
    res.set_content("{\"ok\":true}", "application/json");
}

void handlePull(Service& service, const std::string& data_area, const httplib::Request& req, httplib::Response& res) {
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
    auto claims = jwt::decode(jwt, service.getSigningPubkey());
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
}

void handleDropPipe(Service& service, const std::string& data_area, const httplib::Request& req, httplib::Response& res) {
    res.set_header("Content-Type", "application/json");

    // Get JWT from Authorization header
    std::string auth = req.get_header_value("Authorization");
    std::string jwt;
    if (auth.substr(0, 7) == "Bearer ") {
        jwt = auth.substr(7);
    }
    if (jwt.empty()) {
        res.status = 401;
        res.set_content("{\"ok\":false,\"error\":\"Missing authorization\"}", "application/json");
        return;
    }

    // Get name from query string
    std::string name = req.get_param_value("name");
    if (name.empty()) {
        res.status = 400;
        res.set_content("{\"ok\":false,\"error\":\"Missing name param\"}", "application/json");
        return;
    }

    auto resp = service.handleDropPipe({jwt, name});
    if (!resp.ok) {
        if (resp.error == "Unauthorized") {
            res.status = 401;
        } else if (resp.error == "Not found") {
            res.status = 404;
        } else {
            res.status = 400;
        }
        res.set_content("{\"ok\":false,\"error\":\"" + resp.error + "\"}", "application/json");
        return;
    }
    res.set_content("{\"ok\":true}", "application/json");
}

} // namespace rest
} // namespace base
