#include "render_overlay.hpp"
#include "render_logs.hpp"
#include "render_player_list.hpp"
#include "styles.hpp"

#include "../config.hpp"

#include <spdlog/spdlog.h>

#include <backends/imgui_impl_dx12.h>
#include <imgui.h>
#include <misc/freetype/imgui_freetype.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace std;
namespace fs = filesystem;

float gg::gui::scale = 1.f;

static void load_font() {
    auto resource = gg::config::get_resource("font");
    if (resource.has_value()) {
        auto font_data = resource.value();

        auto font_config = ImFontConfig{};
        font_config.SizePixels = gg::gui::font_size;
        font_config.PixelSnapH = false;

        auto &io = ImGui::GetIO();
        io.Fonts->AddFontFromMemoryCompressedTTF(font_data.data(), font_data.size(), 0,
                                                 &font_config);
        io.Fonts->Build();
    }
}

void gg::gui::initialize_overlay() {
    ImGui::GetStyle().WindowBorderSize = 0;
    load_font();
    gg::gui::initialize_player_list();
    gg::gui::initialize_logs();
}

void gg::gui::render_overlay() {
    auto &io = ImGui::GetIO();
    auto viewport = ImGui::GetMainViewport();

    gg::gui::scale = fminf(io.DisplaySize.y / virtual_size.y, io.DisplaySize.x / virtual_size.x);
    io.FontGlobalScale = gg::gui::scale;

    auto overlay_pos = viewport->WorkPos + ImVec2{viewport->WorkSize.x, 0.f} -
                       (viewport->WorkSize - virtual_size * scale) / 2.f;

    static bool is_player_list_open = true;
    static bool is_logs_open = false;
    static bool player_list_priority = false;

    auto show_player_list = is_player_list_open && (!is_logs_open || player_list_priority);
    gg::gui::render_player_list(overlay_pos, show_player_list);
    if (ImGui::IsKeyPressed(gg::config::toggle_player_list_key)) {
        is_player_list_open = !show_player_list;
        if (is_player_list_open) {
            player_list_priority = true;
        }
    }

    auto show_logs = is_logs_open && (!is_player_list_open || !player_list_priority);
    gg::gui::render_logs(overlay_pos, show_logs);
    if (ImGui::IsKeyPressed(gg::config::toggle_logs_key)) {
        is_logs_open = !show_logs;
        if (is_logs_open) {
            player_list_priority = false;
        }
    }
}
