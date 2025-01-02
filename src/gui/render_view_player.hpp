#pragma once

#include <imgui.h>

namespace gg
{
namespace gui
{

void initialize_view_player();
void render_view_player(bool &is_open, const ImVec2 &window_pos, int player_count);

}
}
