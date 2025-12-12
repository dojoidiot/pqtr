// mailgun.cpp
// Mailgun implementation of BASE Mailer interface

#include "mail.hpp"
#include <iostream>
#include <sstream>
#include <cstdio>

namespace base {

namespace {
bool validEmail(const std::string& s) {
    if (s.size() > 254) return false;
    bool at = false;
    for (char c : s) {
        if (c == '@') { at = true; continue; }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == '+') continue;
        return false;
    }
    return at;
}
}

class MailgunMailer : public Mailer {
    std::string m_secret;
    std::string m_domain;
    std::string m_from;
    std::string m_otp_text;
    std::string m_base_url;

public:
    MailgunMailer(const std::string& secret, const std::string& domain,
                  const std::string& from, const std::string& otp_text, const std::string& region)
        : m_secret(secret), m_domain(domain), m_from(from), m_otp_text(otp_text) {
        m_base_url = (region == "eu") ? "https://api.eu.mailgun.net" : "https://api.mailgun.net";
    }

    bool sendOtp(const std::string& email, const std::string& otp) override {
        if (!validEmail(email)) return false;
        char text[1024];
        snprintf(text, sizeof(text), m_otp_text.c_str(), otp.c_str());

        std::ostringstream cmd;
        cmd << "curl -s --max-time 10 "
            << "--user 'api:" << m_secret << "' "
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

std::unique_ptr<Mailer> createMailer(const std::string& secret, const std::string& domain,
                                      const std::string& from, const std::string& otp_text,
                                      const std::string& region) {
    return std::make_unique<MailgunMailer>(secret, domain, from, otp_text, region);
}

} // namespace base
