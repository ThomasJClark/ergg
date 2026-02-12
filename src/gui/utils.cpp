#include "utils.hpp"
#include "../renderer/texture.hpp"
#include "styles.hpp"

#include <array>

using namespace std;

void gg::gui::render_nine_slice(ImDrawList *drawlist,
                                ImTextureID texture_id,
                                ImVec2 texture_size,
                                ImVec2 pos,
                                ImVec2 size,
                                ImVec2 padding,
                                float opacity) {
    texture_size *= scale;
    padding *= scale;

    auto color = ImGui::GetColorU32({1.f, 1.f, 1.f, opacity});

    auto verts = array{pos, pos + padding, pos + size - padding, pos + size};

    auto uvs = array{ImVec2{0, 0}, padding / texture_size, ImVec2{1, 1} - padding / texture_size,
                     ImVec2{1, 1}};

    // If the size is too small to fit the padding, shrink the top/bottom or left/right rects to
    // half of the size
    if (verts[1].x > verts[2].x) {
        verts[1].x = verts[2].x = pos.x + size.x / 2.f;
        uvs[1].x = size.x / 2.f / texture_size.x;
        uvs[2].x = 1.f - uvs[1].x;
    }

    if (verts[1].y > verts[2].y) {
        verts[1].y = verts[2].y = pos.y + size.y / 2.f;
        uvs[1].y = size.y / 2.f / texture_size.y;
        uvs[2].y = 1.f - uvs[1].y;
    }

    for (int32_t i = 0; i < 3; i++) {
        for (int32_t j = 0; j < 3; j++) {
            if (verts[i].x == verts[i + 1].x || verts[j].y == verts[j + 1].y) continue;

            drawlist->AddImage(texture_id, {verts[i].x, verts[j].y},
                               {verts[i + 1].x, verts[j + 1].y}, {uvs[i].x, uvs[j].y},
                               {uvs[i + 1].x, uvs[j + 1].y}, color);
        }
    }
}

void gg::gui::render_player_list_action_text(const ImVec2 &windowpos,
                                             const ImVec2 &windowsize,
                                             const string &text,
                                             float opacity) {
    static shared_ptr<gg::renderer::texture> background_texture;
    if (!background_texture) {
        background_texture = gg::renderer::load_texture_from_resource("MENU_FE_Warning");
    }

    auto pos = windowpos;
    pos.y += windowsize.y + 24.f;

    auto text_color = white;
    text_color.w = opacity;

    auto text_begin = text.data();
    auto text_end = text.data() + text.size();
    auto wrap_width = windowsize.x;

    auto size = ImGui::CalcTextSize(text_begin, text_end, false, wrap_width);

    auto padding = ImVec2{120.f, 24.f};

    render_nine_slice(ImGui::GetBackgroundDrawList(), background_texture->id(),
                      background_texture->size() * scale, pos - padding, size + padding * 2.f,
                      {120.f, 0.f}, opacity * .6f);

    ImGui::GetForegroundDrawList()->AddText(nullptr, 0.f, pos, ImGui::GetColorU32(text_color),
                                            text_begin, text_end, wrap_width);
}