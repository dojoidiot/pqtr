// chat.hpp - Chat pane for tune progress
//
// Simple scrolling text log showing tune's work.

#pragma once

#include <string>

namespace desk {

// Add a message to the chat log
void chat(const std::string& message);

// Clear the chat log
void chat_clear();

// Render the chat panel (scrolling text log)
void render_chat_panel();

// Check if chat is active (tuning in progress)
bool is_chat_active();
void set_chat_active(bool active);

} // namespace desk
