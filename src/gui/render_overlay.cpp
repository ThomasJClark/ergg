#include "render_overlay.hpp"
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

static void load_font(fs::path filename)
{
    auto stream = basic_ifstream<unsigned char>{filename, ios::binary | ios::ate};
    if (stream.fail())
    {
        SPDLOG_CRITICAL("Failed to load font {}", filename.string());
        return;
    }

    auto size = stream.tellg();
    stream.seekg(0, ios::beg);

    auto data = vector<unsigned char>(size);
    if (!stream.read(data.data(), size))
    {
        SPDLOG_CRITICAL("Failed to load font {}", filename.string());
        return;
    }

    auto font_config = ImFontConfig{};
    font_config.SizePixels = gg::gui::font_size;
    font_config.PixelSnapH = false;

    auto &io = ImGui::GetIO();
    io.Fonts->AddFontFromMemoryTTF(data.data(), data.size(), 0, &font_config);
    io.Fonts->Build();
}

void gg::gui::initialize_overlay()
{
    ImGui::GetStyle().WindowBorderSize = 0;

    load_font(gg::config::mod_folder / "assets" / "FOT-Matisse ProN DB.ttf");

    initialize_player_list();
}

void gg::gui::render_overlay()
{
    static bool show_player_list = true;

    if (GetAsyncKeyState(gg::config::toggle_player_list_key) & 1)
    {
        show_player_list = !show_player_list;
    }

    if (show_player_list)
    {
        auto &io = ImGui::GetIO();

        gg::gui::scale = fminf(io.DisplaySize.y / virtual_height, io.DisplaySize.x / virtual_width);

        io.FontGlobalScale = gg::gui::scale;

        render_player_list();
    }
}
