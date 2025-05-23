#pragma once

#include <imgui.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <span>
#include <string>

namespace gg {
namespace config {

extern std::filesystem::path mod_folder;

extern bool show_in_game_name;
extern bool show_level;
extern bool show_steam_name;
extern bool show_steam_avatar;
extern bool show_steam_relationship;
extern bool show_ping;
extern unsigned int high_ping;
extern bool show_yourself;

extern ImGuiKey toggle_logs_key;
extern ImGuiKey toggle_player_list_key;
extern ImGuiKey block_player_key;
extern ImGuiKey disconnect_key;

extern bool debug;

void set_handle(HINSTANCE mod_handle);
void load();

std::optional<std::span<unsigned char>> get_resource(std::string name, std::string type = "DATA");

}
}
