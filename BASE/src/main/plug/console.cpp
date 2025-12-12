// console.cpp
// Console mailer - prints OTP to stdout for local testing

#include "mail.hpp"
#include <iostream>

namespace base {

class ConsoleMailer : public Mailer {
public:
    bool sendOtp(const std::string& email, const std::string& otp) override {
        // Truncate if too long to prevent overflow
        std::string e = email.size() <= 30 ? email : email.substr(0, 27) + "...";
        std::string o = otp.size() <= 30 ? otp : otp.substr(0, 27) + "...";
        std::string e_pad(30 - e.size(), ' ');
        std::string o_pad(30 - o.size(), ' ');

        std::cout << "\n"
                  << "╔════════════════════════════════════════╗\n"
                  << "║           LOCAL TEST OTP               ║\n"
                  << "╠════════════════════════════════════════╣\n"
                  << "║  Email: " << e << e_pad << "║\n"
                  << "║  OTP:   " << o << o_pad << "║\n"
                  << "╚════════════════════════════════════════╝\n"
                  << std::endl;
        return true;
    }
};

std::unique_ptr<Mailer> createConsoleMailer() {
    return std::make_unique<ConsoleMailer>();
}

} // namespace base
