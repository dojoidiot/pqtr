// jrpc.cpp - JSON-RPC request handling

#include "base.hpp"
#include <sstream>
#include <cstdio>

namespace {

std::string jsonEscape(const std::string& s) {
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
    if (json[pos] == '"') return jsonEscape(extractString(json, "id"));
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
           ",\"message\":" + jsonEscape(message) + "},\"id\":" + id + "}";
}

} // anonymous namespace

namespace base {
namespace jrpc {

std::string handle(Service& service, const std::string& body, const std::string& auth_header) {
    std::string id = extractId(body);
    std::string method = extractString(body, "method");

    // Get JWT from Authorization header
    std::string jwt;
    if (auth_header.rfind("Bearer ", 0) == 0) {
        jwt = auth_header.substr(7);
    }

    printf("[JRPC] method=%s\n", method.c_str());
    fflush(stdout);

    // Validate JRPC envelope
    if (body.find("\"jsonrpc\"") == std::string::npos || body.find("\"2.0\"") == std::string::npos) {
        return jrpcError(id, -32600, "Invalid Request");
    }
    if (method.empty()) {
        return jrpcError(id, -32600, "Missing method");
    }

    // Extract common parameters
    std::string p_email = extractString(body, "email");
    std::string p_otp = extractString(body, "otp");
    std::string p_refresh = extractString(body, "refresh_token");
    std::string p_user_id = extractString(body, "user_id");
    std::string p_role = extractString(body, "role");

    // Dispatch methods
    if (method == "register") {
        if (p_email.empty()) return jrpcError(id, -32602, "Missing email");
        auto resp = service.handleRegister({p_email});
        if (!resp.ok) {
            const std::string& err_msg = resp.error.empty() ? "Registration failed" : resp.error;
            return jrpcError(id, -32001, err_msg);
        }
        std::ostringstream r;
        r << "{\"ok\":" << (resp.ok ? "true" : "false");
        if (resp.ok) r << ",\"expires\":" << resp.expires;
        r << "}";
        return jrpcResult(id, r.str());

    } else if (method == "verify") {
        if (p_email.empty() || p_otp.empty()) return jrpcError(id, -32602, "Missing email or otp");
        auto resp = service.handleVerify({p_email, p_otp});
        if (resp.jwt.empty()) {
            const std::string& err_msg = resp.error.empty() ? "Verification failed" : resp.error;
            printf("[JRPC] verify FAIL: %s\n", err_msg.c_str()); fflush(stdout);
            return jrpcError(id, -32001, err_msg);
        }
        std::ostringstream r;
        r << "{\"jwt\":" << jsonEscape(resp.jwt) << ",\"refresh_token\":" << jsonEscape(resp.refresh_token)
          << ",\"user_id\":" << jsonEscape(resp.user_id) << ",\"itag\":" << jsonEscape(resp.itag)
          << ",\"role\":" << jsonEscape(resp.role) << "}";
        return jrpcResult(id, r.str());

    } else if (method == "login") {
        if (p_email.empty()) return jrpcError(id, -32602, "Missing email");
        auto resp = service.handleLogin({p_email});
        if (!resp.ok) {
            const std::string& err_msg = resp.error.empty() ? "Login failed" : resp.error;
            return jrpcError(id, -32001, err_msg);
        }
        std::ostringstream r;
        r << "{\"ok\":" << (resp.ok ? "true" : "false");
        if (resp.ok) r << ",\"expires\":" << resp.expires;
        r << "}";
        return jrpcResult(id, r.str());

    } else if (method == "refresh") {
        if (p_refresh.empty()) return jrpcError(id, -32602, "Missing refresh_token");
        auto resp = service.handleRefresh({p_refresh});
        if (resp.jwt.empty()) return jrpcError(id, -32001, "Refresh failed");
        return jrpcResult(id, "{\"jwt\":" + jsonEscape(resp.jwt) + "}");

    } else if (method == "find") {
        if (jwt.empty() || p_email.empty()) return jrpcError(id, -32602, "Missing jwt or email");
        auto resp = service.handleFind({jwt, p_email});
        if (resp.user_id.empty()) return jrpcError(id, -32001, "Not found");
        std::ostringstream r;
        r << "{\"user_id\":" << jsonEscape(resp.user_id) << ",\"email\":" << jsonEscape(resp.email)
          << ",\"tier\":" << jsonEscape(resp.tier) << ",\"role\":" << jsonEscape(resp.role)
          << ",\"locked\":" << (resp.locked ? "true" : "false") << ",\"created_at\":" << resp.created_at << "}";
        return jrpcResult(id, r.str());

    } else if (method == "give") {
        if (jwt.empty() || p_user_id.empty() || p_role.empty()) return jrpcError(id, -32602, "Missing params");
        auto resp = service.handleGive({jwt, p_user_id, p_role});
        return jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}");

    } else if (method == "take") {
        if (jwt.empty() || p_user_id.empty()) return jrpcError(id, -32602, "Missing params");
        auto resp = service.handleTake({jwt, p_user_id});
        return jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}");

    } else if (method == "lock") {
        if (jwt.empty() || p_user_id.empty()) return jrpcError(id, -32602, "Missing params");
        auto resp = service.handleLock({jwt, p_user_id});
        return jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}");

    } else if (method == "free") {
        if (jwt.empty() || p_user_id.empty()) return jrpcError(id, -32602, "Missing params");
        auto resp = service.handleFree({jwt, p_user_id});
        return jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}");

    } else if (method == "drop") {
        if (jwt.empty() || p_user_id.empty()) return jrpcError(id, -32602, "Missing params");
        auto resp = service.handleDrop({jwt, p_user_id});
        return jrpcResult(id, std::string("{\"ok\":") + (resp.ok ? "true" : "false") + "}");

    } else if (method == "info") {
        if (jwt.empty()) return jrpcError(id, -32602, "Missing jwt");
        auto resp = service.handleInfo({jwt});
        if (resp.total_users == 0 && resp.users_none == 0) return jrpcError(id, -32001, "Unauthorized");
        std::ostringstream r;
        r << "{\"total_users\":" << resp.total_users << ",\"users_none\":" << resp.users_none
          << ",\"users_play\":" << resp.users_play << ",\"users_hero\":" << resp.users_hero
          << ",\"users_pqtr\":" << resp.users_pqtr << "}";
        return jrpcResult(id, r.str());

    } else if (method == "list") {
        std::string p_name = extractString(body, "name");
        if (jwt.empty()) return jrpcError(id, -32602, "Missing jwt");
        auto resp = service.handleList({jwt, p_name});
        if (!resp.ok) return jrpcError(id, -32001, "Unauthorized");
        std::ostringstream r;
        r << "{\"items\":[";
        for (size_t i = 0; i < resp.items.size(); i++) {
            if (i > 0) r << ",";
            r << "\"" << resp.items[i] << "\"";
        }
        r << "]}";
        return jrpcResult(id, r.str());

    } else if (method == "test") {
        std::string p_name = extractString(body, "name");
        if (jwt.empty() || p_name.empty()) return jrpcError(id, -32602, "Missing jwt or name");
        auto resp = service.handleTest({jwt, p_name});
        if (!resp.ok) return jrpcError(id, -32001, "Unauthorized");
        return jrpcResult(id, std::string("{\"exists\":") + (resp.exists ? "true" : "false") + "}");

    } else {
        return jrpcError(id, -32601, "Method not found");
    }
}

} // namespace jrpc
} // namespace base
