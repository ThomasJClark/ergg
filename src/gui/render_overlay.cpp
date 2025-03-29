#include "render_overlay.hpp"
#include "render_logs.hpp"
#include "render_player_list.hpp"
#include "styles.hpp"

#include "../config.hpp"

#include <elden-x/chr/world_chr_man.hpp>

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

static void load_font()
{
    auto resource = gg::config::get_resource("font");
    if (resource.has_value())
    {
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

void gg::gui::initialize_overlay()
{
    ImGui::GetStyle().WindowBorderSize = 0;
    load_font();
    gg::gui::initialize_player_list();
    gg::gui::initialize_logs();
}

void gg::gui::render_overlay()
{
    auto &io = ImGui::GetIO();
    auto viewport = ImGui::GetMainViewport();

    gg::gui::scale = fminf(io.DisplaySize.y / virtual_size.y, io.DisplaySize.x / virtual_size.x);
    io.FontGlobalScale = gg::gui::scale;

    auto overlay_pos = viewport->WorkPos + ImVec2{viewport->WorkSize.x, 0.f} -
                       (viewport->WorkSize - virtual_size * scale) / 2.f;

    static bool is_player_list_open = true;
    static bool is_logs_open = false;

    if (GetAsyncKeyState(gg::config::toggle_player_list_key) & 1)
    {
        is_player_list_open = !is_player_list_open;
        if (is_player_list_open)
        {
            is_logs_open = false;
        }
    }

    if (GetAsyncKeyState(gg::config::toggle_logs_key) & 1)
    {
        auto world_chr_man = er::CS::WorldChrManImp::instance();
        if (world_chr_man && world_chr_man->main_player)
        {
            auto &chr_asm = world_chr_man->main_player->game_data->equip_game_data.chr_asm;
            SPDLOG_INFO("chr_asm = {}", (void *)&chr_asm);
        }

        is_logs_open = !is_logs_open;
        if (is_logs_open)
        {
            is_player_list_open = false;
        }
    }

    gg::gui::render_player_list(overlay_pos, is_player_list_open);
    gg::gui::render_logs(overlay_pos, is_logs_open);
}
