// console.cpp
// Console mailer - prints OTP to stdout for local testing

#include "mail.hpp"
#include <iostream>

namespace base {

class ConsoleMailer : public Mailer {
public:
    bool sendOtp(const std::string& email, const std::string& otp) override {
        std::cout << "\n"
                  << "╔════════════════════════════════════════╗\n"
                  << "║           LOCAL TEST OTP               ║\n"
                  << "╠════════════════════════════════════════╣\n"
                  << "║  Email: " << email << std::string(30 - email.size(), ' ') << "║\n"
                  << "║  OTP:   " << otp << std::string(30 - otp.size(), ' ') << "║\n"
                  << "╚════════════════════════════════════════╝\n"
                  << std::endl;
        return true;
    }
};

std::unique_ptr<Mailer> createConsoleMailer() {
    return std::make_unique<ConsoleMailer>();
}

} // namespace base
