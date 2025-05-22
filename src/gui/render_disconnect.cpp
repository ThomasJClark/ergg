#include "render_disconnect.hpp"
#include "styles.hpp"
#include "utils.hpp"

#include "../config.hpp"
#include "../renderer/texture.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <elden-x/session.hpp>

#include <memory>
#include <string>

using namespace std;

static string text = "Press again to disconnect, or ESC to cancel";

void gg::gui::render_disconnect(bool &is_open, const ImVec2 &windowpos, const ImVec2 &windowsize) {
    if (ImGui::IsKeyPressed(gg::config::disconnect_key)) {
        // Require a second press to confirm, to prevent accidental disconnects
        if (is_open) {
            SPDLOG_INFO("Disconnecting from online session");

            auto session_man = er::CS::CSSessionManager::instance();
            if (session_man) {
                // AFAIK there's only one session at a time, but this seems to be treated as a list
                // according to vanilla session manager functions
                for (auto session : session_man->sessions()) {
                    session_man->end_session(session);
                }
            }
        }

        is_open = !is_open;
    }

    if (is_open && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        is_open = false;
    }

    static fade_in_out fade_in_out;
    if (fade_in_out.animate(is_open)) {
        render_player_list_action_text(windowpos, windowsize, text, fade_in_out.alpha);
    }
}
