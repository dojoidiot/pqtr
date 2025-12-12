// mail.hpp
// JWTA mailer interface

#pragma once

#include <string>
#include <memory>

namespace jwta {

class Mailer {
public:
    virtual ~Mailer() = default;
    virtual bool sendOtp(const std::string& email, const std::string& otp) = 0;
};

std::unique_ptr<Mailer> createMailer(
    const std::string& api_key,
    const std::string& domain,
    const std::string& from,
    const std::string& otp_text,
    const std::string& region
);

// Console mailer for local testing (prints OTP to stdout)
std::unique_ptr<Mailer> createConsoleMailer();

} // namespace jwta
