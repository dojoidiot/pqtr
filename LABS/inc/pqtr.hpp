// pqtr.hpp
// Shared paths and configuration for pqtr tools
//
// Default structure (~/.pqtr or override via --pqtr):
//   bin/           Binaries (labs, tune, desk)
//   etc/tune.json  Built-in priors (from code, static)
//   var/tune.json  Learned priors (accumulated)
//   var/tune/      Tune project outputs
//   var/desk.json  Desk state/settings

#pragma once

#include <string>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

namespace pqtr
{
    // Get pqtr root directory (default: ~/.pqtr)
    inline std::string root(const std::string& override = "")
    {
        if (!override.empty())
            return override;

        const char* home = std::getenv("HOME");
        if (!home)
            return ".pqtr";  // Fallback to current directory

        return std::string(home) + "/.pqtr";
    }

    // Standard paths relative to root
    inline std::string bin(const std::string& r) { return r + "/bin"; }
    inline std::string etc(const std::string& r) { return r + "/etc"; }
    inline std::string var(const std::string& r) { return r + "/var"; }

    // Specific files
    inline std::string etcTune(const std::string& r) { return r + "/etc/tune.json"; }   // Built-in priors
    inline std::string varTune(const std::string& r) { return r + "/var/tune.json"; }   // Learned priors
    inline std::string varTuneDir(const std::string& r) { return r + "/var/tune"; }     // Project outputs
    inline std::string varDesk(const std::string& r) { return r + "/var/desk.json"; }   // Desk state

    // Check if file exists
    inline bool exists(const std::string& path)
    {
        std::ifstream f(path);
        return f.good();
    }

    // Create directory (recursive)
    inline bool makedirs(const std::string& path)
    {
        std::string cmd = "mkdir -p \"" + path + "\"";
        return system(cmd.c_str()) == 0;
    }

    // Get priors path: var/tune.json if exists, else etc/tune.json
    inline std::string priorsPath(const std::string& r)
    {
        std::string learned = varTune(r);
        if (exists(learned))
            return learned;
        return etcTune(r);
    }

    // Initialize pqtr directory structure
    inline bool init(const std::string& r)
    {
        return makedirs(bin(r)) &&
               makedirs(etc(r)) &&
               makedirs(var(r)) &&
               makedirs(varTuneDir(r));
    }

    // Copy file if destination doesn't exist
    inline bool copyIfMissing(const std::string& src, const std::string& dst)
    {
        if (exists(dst))
            return true;  // Already exists

        std::ifstream in(src, std::ios::binary);
        if (!in.good())
            return false;

        std::ofstream out(dst, std::ios::binary);
        if (!out.good())
            return false;

        out << in.rdbuf();
        return true;
    }

    // Install built-in priors from source etc/ to ~/.pqtr/etc/
    // Call this with the path to the installed etc/tune.json
    inline bool installPriors(const std::string& r, const std::string& builtinPath)
    {
        std::string dst = etcTune(r);
        if (exists(dst))
            return true;

        return copyIfMissing(builtinPath, dst);
    }

} // namespace pqtr
