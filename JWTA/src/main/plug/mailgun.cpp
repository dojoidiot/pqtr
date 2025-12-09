// mailgun.cpp
// Mailgun implementation of JWTA Mailer interface

#include "mail.hpp"
#include <iostream>
#include <sstream>
#include <cstdio>

namespace jwta {

class MailgunMailer : public Mailer {
    std::string m_api_key;
    std::string m_domain;
    std::string m_from;
    std::string m_base_url;

public:
    MailgunMailer(const std::string& api_key, const std::string& domain,
                  const std::string& from, const std::string& region)
        : m_api_key(api_key), m_domain(domain), m_from(from) {
        m_base_url = (region == "eu") ? "https://api.eu.mailgun.net" : "https://api.mailgun.net";
    }

    bool sendOtp(const std::string& email, const std::string& otp) override {
        std::string text = "Your verification code is: " + otp + "\n\nThis code expires in 10 minutes.";

        std::ostringstream cmd;
        cmd << "curl -s --max-time 10 "
            << "--user 'api:" << m_api_key << "' "
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

std::unique_ptr<Mailer> createMailer(const std::string& api_key, const std::string& domain,
                                      const std::string& from, const std::string& region) {
    return std::make_unique<MailgunMailer>(api_key, domain, from, region);
}

} // namespace jwta
