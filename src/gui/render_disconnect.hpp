#pragma once

#include <imgui.h>

namespace gg {
namespace gui {

void initialize_disconnect();
void render_disconnect(bool &is_open, const ImVec2 &windowpos, const ImVec2 &windowsize);

}
}
