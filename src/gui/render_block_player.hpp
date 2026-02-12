#pragma once

#include <imgui.h>
#include <cstdint>

namespace gg {
namespace gui {

void initialize_block_player();
bool render_block_player(bool &is_open,
                         const ImVec2 &window_pos,
                         const ImVec2 &windowsize,
                         int32_t player_count);

}
}
