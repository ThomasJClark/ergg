#pragma once

#include <imgui.h>

namespace gg {
namespace gui {

static constexpr auto virtual_size = ImVec2{1920.f, 1080.f};

static constexpr auto white = ImVec4{.797f, .797f, .797f, 1.f};
static constexpr auto pale_gold = ImVec4{.765f, .71f, .6f, 1.f};
static constexpr auto red = ImVec4{.784f, .161f, .184f, 1.f};
static constexpr auto green = ImVec4{.565f, .729f, .235f, 1};
static constexpr auto blue = ImVec4{.118f, .565f, 1, 1};
static constexpr auto gold = ImVec4{1, .843f, 0, 1};

static constexpr float font_size = 17;

static const auto player_list_avatar_size = ImVec2{24, 24};
static const float player_list_row_height = 32;

extern float scale;

}
}
// 90ba3c