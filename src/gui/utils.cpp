#include "utils.hpp"
#include "styles.hpp"

#include <array>

using namespace std;

void gg::gui::render_nine_slice(ImDrawList *drawlist,
                                ImTextureID texture_id,
                                ImVec2 texture_size,
                                ImVec2 pos,
                                ImVec2 size,
                                ImVec2 padding,
                                float opacity,
                                bool debug) {
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

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (verts[i].x == verts[i + 1].x || verts[j].y == verts[j + 1].y) continue;

            drawlist->AddImage(texture_id, {verts[i].x, verts[j].y},
                               {verts[i + 1].x, verts[j + 1].y}, {uvs[i].x, uvs[j].y},
                               {uvs[i + 1].x, uvs[j + 1].y}, color);
        }
    }

    if (debug) {
        drawlist->AddRect(verts[0], verts[3], ImGui::GetColorU32(blue));
        drawlist->AddRect(verts[1], verts[2], ImGui::GetColorU32(gold));
    }
}