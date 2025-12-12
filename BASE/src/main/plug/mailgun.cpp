// mailgun.cpp - Mailgun implementation using httplib (no shell commands)

#include "mail.hpp"
#include "httplib.h"
#include <iostream>
#include <sstream>

namespace base {

namespace {

// Strict email validation - only allow safe characters
bool validEmail(const std::string& s) {
    if (s.empty() || s.size() > 254) return false;
    size_t at_pos = s.find('@');
    if (at_pos == std::string::npos || at_pos == 0 || at_pos == s.size() - 1) return false;
    if (s.find('@', at_pos + 1) != std::string::npos) return false;  // Multiple @

    for (char c : s) {
        if (c >= 'a' && c <= 'z') continue;
        if (c >= 'A' && c <= 'Z') continue;
        if (c >= '0' && c <= '9') continue;
        if (c == '@' || c == '.' || c == '-' || c == '_' || c == '+') continue;
        return false;
    }
    return true;
}

// URL-encode a string for form data
std::string urlEncode(const std::string& s) {
    std::ostringstream out;
    out << std::hex;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out << c;
        } else {
            out << '%' << std::uppercase;
            if (c < 16) out << '0';
            out << static_cast<int>(c);
        }
    }
    return out.str();
}

}  // namespace

class MailgunMailer : public Mailer {
    std::string m_secret;
    std::string m_domain;
    std::string m_from;
    std::string m_otp_text;
    std::string m_host;

public:
    MailgunMailer(const std::string& secret, const std::string& domain,
                  const std::string& from, const std::string& otp_text, const std::string& region)
        : m_secret(secret), m_domain(domain), m_from(from), m_otp_text(otp_text) {
        m_host = (region == "eu") ? "api.eu.mailgun.net" : "api.mailgun.net";
    }

    bool sendOtp(const std::string& email, const std::string& otp) override {
        if (!validEmail(email)) {
            std::cerr << "[MAIL] Invalid email format" << std::endl;
            return false;
        }

        // Format the OTP text (safely - no shell involved)
        std::string text = m_otp_text;
        size_t pos = text.find("%s");
        if (pos != std::string::npos) {
            text.replace(pos, 2, otp);
        }

        // Build form data
        std::string body;
        body += "from=" + urlEncode(m_from);
        body += "&to=" + urlEncode(email);
        body += "&subject=" + urlEncode("Your PQTR verification code");
        body += "&text=" + urlEncode(text);

        // Create HTTPS client
        httplib::Client cli("https://" + m_host);
        cli.set_connection_timeout(10);
        cli.set_read_timeout(10);
        cli.set_basic_auth("api", m_secret);

        // POST to Mailgun API
        std::string path = "/v3/" + m_domain + "/messages";
        auto res = cli.Post(path, body, "application/x-www-form-urlencoded");

        if (!res) {
            std::cerr << "[MAIL] Connection failed to " << m_host << std::endl;
            return false;
        }

        if (res->status == 200) {
            std::cout << "[MAIL] OTP sent to " << email << std::endl;
            return true;
        } else {
            std::cerr << "[MAIL] Failed (" << res->status << "): " << res->body << std::endl;
            return false;
        }
    }
};

std::unique_ptr<Mailer> createMailer(const std::string& secret, const std::string& domain,
                                      const std::string& from, const std::string& otp_text,
                                      const std::string& region) {
    return std::make_unique<MailgunMailer>(secret, domain, from, otp_text, region);
}

}  // namespace base
