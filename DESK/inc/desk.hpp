// desk.hpp
// Public API for DESK - Project management interface for LABS
//
// User apps include only this header and link against desk binary.
// Implementation details are hidden (PIMPL pattern).

#pragma once

#include <string>
#include <memory>

namespace desk
{
    // ============================================================
    // Forward Declarations
    // ============================================================

    class App;
    class State;
    class Theme;

    // ============================================================
    // App - Main application entry point
    // ============================================================

    class App
    {
    public:
        virtual ~App() = default;

        // Run the application main loop
        // Returns exit code (0 = success)
        virtual int run() = 0;

        // Request application shutdown
        virtual void quit() = 0;
    };

    // ============================================================
    // Config - Application configuration
    // ============================================================

    struct Config
    {
        std::string root_folder = "var";  // Default project folder
        int window_width = 1280;
        int window_height = 720;
        bool maximized = true;
    };

    // ============================================================
    // Factory function
    // ============================================================

    // Create application instance with configuration
    std::unique_ptr<App> make(const Config& config = Config{});

} // namespace desk
