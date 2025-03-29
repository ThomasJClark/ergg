#include "render_logs.hpp"
#include "styles.hpp"
#include "utils.hpp"

#include "../renderer/texture.hpp"

#include <imgui.h>

using namespace std;

static shared_ptr<gg::renderer::texture_st> background_texture;

void gg::gui::initialize_logs()
{
    background_texture = renderer::load_texture_from_resource("MENU_FL_d0");
}

void gg::gui::render_logs(ImVec2 pos, bool is_open)
{
    static fade_in_out_st fade_in_out;

    if (!fade_in_out.animate(is_open))
    {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade_in_out.alpha);

    auto background_pos = pos + ImVec2{80.f - background_texture->width, 19.f} * scale;
    auto text_pos = background_pos + ImVec2{80.f, 16.f} * scale;

    render_nine_slice(ImGui::GetBackgroundDrawList(), background_texture->desc.second.ptr,
                      {(float)background_texture->width, (float)background_texture->height},
                      background_pos, ImVec2{(float)background_texture->width, 274.f} * scale,
                      ImVec2{0.f, 16.f}, fade_in_out.alpha * .8f);

    ImGui::GetBackgroundDrawList()->AddText(nullptr, 0.f, text_pos,
                                            ImGui::GetColorU32(gg::gui::white), "Bingus");
    ImGui::GetBackgroundDrawList()->AddText(nullptr, 0.f, text_pos + ImVec2{0.f, 20.f} * scale,
                                            ImGui::GetColorU32(gg::gui::white), "Bongus");
    ImGui::GetBackgroundDrawList()->AddText(nullptr, 0.f, text_pos + ImVec2{0.f, 40.f} * scale,
                                            ImGui::GetColorU32(gg::gui::white), "Bungus");
    ImGui::GetBackgroundDrawList()->AddText(nullptr, 0.f, text_pos + ImVec2{0.f, 60.f} * scale,
                                            ImGui::GetColorU32(gg::gui::white), "Bangus");
    ImGui::GetBackgroundDrawList()->AddText(nullptr, 0.f, text_pos + ImVec2{0.f, 80.f} * scale,
                                            ImGui::GetColorU32(gg::gui::white), "Bengus");
    ImGui::GetBackgroundDrawList()->AddText(nullptr, 0.f, text_pos + ImVec2{0.f, 100.f} * scale,
                                            ImGui::GetColorU32(gg::gui::white), "Bangus");

    ImGui::PopStyleVar(ImGuiStyleVar_Alpha);
}
