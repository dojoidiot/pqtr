// conf.cpp - Configuration loading

#include "base.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace base {

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
    if (!(s = getString("boot_path")).empty()) cfg.boot_path = s;
    if (!(s = getString("admin_email")).empty()) cfg.admin_email = s;
    if (!(s = getString("boot_email")).empty()) cfg.boot_email = s;
    if (!(s = getString("otp_from")).empty()) cfg.otp_from = s;
    if (!(s = getString("otp_text")).empty()) cfg.otp_text = s;

    // sqlite.file
    if (!(s = getNestedString("sqlite", "file")).empty()) cfg.sqlite_file = s;

    // mailgun.domain, mailgun.region, mailgun.otp_skey
    if (!(s = getNestedString("mailgun", "domain")).empty()) cfg.mailgun_domain = s;
    if (!(s = getNestedString("mailgun", "region")).empty()) cfg.mailgun_region = s;
    if (!(s = getNestedString("mailgun", "otp_skey")).empty()) cfg.mailgun_otp_skey = s;

    return true;
}

} // namespace base
