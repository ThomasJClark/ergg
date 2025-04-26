#include "render_logs.hpp"
#include "styles.hpp"
#include "utils.hpp"

#include "../logs.hpp"
#include "../renderer/texture.hpp"

#include <imgui.h>

using namespace std;

static shared_ptr<gg::renderer::texture> background_texture;

void gg::gui::initialize_logs() {
    background_texture = renderer::load_texture_from_resource("MENU_FL_d0");
}

void gg::gui::render_logs(ImVec2 pos, bool is_open) {
    static fade_in_out fade_in_out;

    if (!fade_in_out.animate(is_open)) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade_in_out.alpha);

    auto background_pos = pos + ImVec2{80.f - background_texture->width(), 19.f} * scale;
    auto text_pos = background_pos + ImVec2{80.f, 16.f} * scale;

    if (ImGui::IsKeyPressed(ImGuiKey_H, false)) {
        gg::logs::log("Hello, world!");
    }

    render_nine_slice(
        ImGui::GetBackgroundDrawList(), background_texture->id(), background_texture->size(),
        background_pos,
        ImVec2{(float)background_texture->width(), (32.f + gg::logs::size() * 20.f) * scale} *
            scale,
        ImVec2{0.f, 16.f}, fade_in_out.alpha * .8f);

    int i = 0;
    gg::logs::for_each([&](const gg::logs::log_entry &entry) {
        ImGui::GetBackgroundDrawList()->AddText(
            nullptr, 0.f, text_pos + ImVec2{0.f, i++ * 20.f} * scale,
            ImGui::GetColorU32(gg::gui::white), entry.message.c_str());
    });

    ImGui::PopStyleVar(ImGuiStyleVar_Alpha);
}
