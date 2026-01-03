// JsonTail.cpp - Saves flow JSON to file

#include "labs.hpp"
#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace pqtr {

void* JsonTail::save(Flow& flow) {
    // Ensure parent directory exists
    size_t last_slash = path_.rfind('/');
    if (last_slash != std::string::npos) {
        std::string dir = path_.substr(0, last_slash);
        mkdir(dir.c_str(), 0755);
    }

    std::ofstream file(path_);
    if (!file) {
        std::cerr << "JsonTail: failed to open " << path_ << "\n";
        return nullptr;
    }

    file << flow.json();
    file.close();

    std::cout << "JsonTail: saved " << path_ << "\n";
    return nullptr;
}

}  // namespace pqtr
