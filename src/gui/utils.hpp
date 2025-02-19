#pragma once

#include <imgui.h>

#include <algorithm>

namespace gg
{
namespace gui
{

template <float fade_time = .1f> struct fade_in_out_st
{
    float alpha{0.f};

    bool animate(bool visible)
    {
        alpha += ImGui::GetIO().DeltaTime / fade_time * (visible ? 1.f : -1.f);
        alpha = std::clamp(alpha, 0.f, 1.f);
        return alpha > 0.f;
    }
};

/**
 * Draw a 9-slice scaled texture to the ImGui background drawlist
 *
 * Applies global scaling factor to texture size and padding, but not to the target rect
 *
 * https://en.wikipedia.org/wiki/9-slice_scaling
 */
void render_nine_slice(ImDrawList *drawlist, ImTextureID texture_id, ImVec2 texture_size,
                       ImVec2 pos, ImVec2 size, ImVec2 padding, float opacity = 1.f,
                       bool debug = false);

}
}
