#pragma once

#include <imgui.h>

namespace gg {
namespace gui {

void initialize_block_player();
void render_block_player(bool &is_open, const ImVec2 &window_pos, int player_count);

}
}
