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

static string prompt = "Press again to disconnect, or ESC to cancel";

shared_ptr<gg::renderer::texture> background_texture;

void gg::gui::initialize_disconnect() {
    background_texture = gg::renderer::load_texture_from_resource("MENU_FE_Warning");
}

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
        auto pos = windowpos;
        pos.y += windowsize.y + 24.f;

        auto text_color = white;
        text_color.w = fade_in_out.alpha;

        auto text_begin = prompt.data();
        auto text_end = prompt.data() + prompt.size();
        auto wrap_width = windowsize.x;

        auto size = ImGui::CalcTextSize(text_begin, text_end, false, wrap_width);

        auto padding = ImVec2{120.f, 24.f};

        render_nine_slice(ImGui::GetBackgroundDrawList(), background_texture->id(),
                          background_texture->size() * scale, pos - padding, size + padding * 2.f,
                          {120.f, 0.f}, fade_in_out.alpha * .6f);

        ImGui::GetForegroundDrawList()->AddText(nullptr, 0.f, pos, ImGui::GetColorU32(text_color),
                                                text_begin, text_end, wrap_width);
    }
}
