// auth.cpp - Login and OTP panes

#include "desk.hpp"
#include <cstring>

void render_login_screen() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.33f);
    ImVec2 dialog_size(360, 280);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(dialog_size);

    ImGui::Begin("Login", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Logo
    ImGui::SetCursorPosX((dialog_size.x - ImGui::CalcTextSize("PQTR").x * 2) * 0.5f);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "PQTR");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Email field
    ImGui::Text("Email");
    ImGui::SetNextItemWidth(-1);
    bool enter_pressed = ImGui::InputText("##email", g_state.email, sizeof(g_state.email),
        ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Enter email to login or register");
    ImGui::Spacing();

    // Error message
    if (g_state.error_message[0] != '\0') {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_state.error_message);

        if (strstr(g_state.error_message, "Rate limit")) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Wait a moment, then try again.");
        } else if (strstr(g_state.error_message, "locked")) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Contact support for help.");
        } else if (strstr(g_state.error_message, "send")) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Check email address and try again.");
        }
        ImGui::Spacing();
    }

    // Status message
    if (g_state.status_message[0] != '\0') {
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.0f), "%s", g_state.status_message);
    }

    ImGui::Spacing();

    // Submit buttons
    bool can_submit = strlen(g_state.email) > 0 && !g_state.request_pending;

    if (!can_submit) ImGui::BeginDisabled();
    if (ImGui::Button(g_state.request_pending ? "Sending..." : "Login", ImVec2(-1, 32)) || (enter_pressed && can_submit)) {
        g_state.error_message[0] = '\0';
        g_state.status_message[0] = '\0';
        sendLoginRequest();
    }
    if (!can_submit) ImGui::EndDisabled();

    ImGui::Spacing();

    if (!can_submit) ImGui::BeginDisabled();
    if (ImGui::Button("Register", ImVec2(-1, 32))) {
        g_state.error_message[0] = '\0';
        g_state.status_message[0] = '\0';
        sendRegisterRequest();
    }
    if (!can_submit) ImGui::EndDisabled();

    ImGui::End();
}

void render_otp_screen() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.33f);
    ImVec2 dialog_size(360, 300);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(dialog_size);

    ImGui::Begin("Enter Code", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Logo
    ImGui::SetCursorPosX((dialog_size.x - ImGui::CalcTextSize("PQTR").x * 2) * 0.5f);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "PQTR");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Status
    if (g_state.status_message[0] != '\0') {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", g_state.status_message);
        ImGui::Spacing();
    }

    ImGui::Text("Email: %s", g_state.email);
    ImGui::Spacing();

    // OTP field
    ImGui::Text("One-Time Code");
    ImGui::SetNextItemWidth(-1);
    if (g_state.otp[0] == '\0') {
        ImGui::SetKeyboardFocusHere();
    }
    bool enter_pressed = ImGui::InputText("##otp", g_state.otp, sizeof(g_state.otp),
        ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Code expires in 10 minutes");
    ImGui::Spacing();

    // Error message
    if (g_state.error_message[0] != '\0') {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_state.error_message);

        if (strstr(g_state.error_message, "expired")) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Go back and request a new code.");
        } else if (strstr(g_state.error_message, "incorrect") || strstr(g_state.error_message, "Invalid")) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Check the code and try again.");
        } else if (strstr(g_state.error_message, "No pending")) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Go back and request a new code.");
        }
        ImGui::Spacing();
    }

    // Submit button
    bool can_submit = strlen(g_state.otp) > 0 && !g_state.request_pending;
    if (!can_submit) ImGui::BeginDisabled();
    if (ImGui::Button(g_state.request_pending ? "Verifying..." : "Verify", ImVec2(-1, 32)) || (enter_pressed && can_submit)) {
        g_state.error_message[0] = '\0';
        sendVerifyRequest();
    }
    if (!can_submit) ImGui::EndDisabled();

    ImGui::Spacing();

    // Back button
    if (ImGui::SmallButton("< Back")) {
        g_state.screen = Screen::Login;
        g_state.otp[0] = '\0';
        g_state.error_message[0] = '\0';
        g_state.status_message[0] = '\0';
        g_state.request_pending = false;
    }

    ImGui::End();
}
