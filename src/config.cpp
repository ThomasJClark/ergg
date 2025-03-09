#include "config.hpp"

#include <mini/ini.h>
#include <spdlog/spdlog.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace std;
namespace fs = std::filesystem;

fs::path gg::config::mod_folder;

bool gg::config::show_in_game_name = true;
bool gg::config::show_level = true;
bool gg::config::show_invaders = true;
bool gg::config::show_steam_name = true;
bool gg::config::show_steam_avatar = true;
bool gg::config::show_steam_relationship = true;
bool gg::config::show_ping = true;
unsigned int gg::config::high_ping = 100;
bool gg::config::show_yourself = false;

int gg::config::toggle_logs_key = VK_OEM_3;
int gg::config::toggle_player_list_key = VK_F2;
int gg::config::block_player_key = VK_F3;
int gg::config::disconnect_key = VK_F4;

bool gg::config::debug = false;

static void try_parse_boolean(mINI::INIMap<string> &config, const string &name, bool &out)
{
    if (!config.has(name))
    {
        SPDLOG_WARN("Missing config \"{}\"", name);
        return;
    }

    auto &value = config[name];
    if (value == "true")
    {
        out = true;
    }
    else if (value == "false")
    {
        out = false;
    }
    else
    {
        SPDLOG_WARN("Invalid config value \"{} = {}\"", name, value);
    }
};

static void try_parse_keycode(mINI::INIMap<string> &config, const string &name, int &out)
{
    if (!config.has(name))
    {
        SPDLOG_WARN("Missing config \"{}\"", name);
        return;
    }

    auto &value = config[name];
    int parsed_value;
    if (value.starts_with("0x") && (parsed_value = strtoul(value.data() + 2, nullptr, 16)))
    {
        out = parsed_value;
    }
    else
    {
        SPDLOG_WARN("Invalid config value \"{} = {}\"", name, value);
    }
};

void gg::config::load_config(const fs::path &ini_path)
{
    mod_folder = ini_path.parent_path();

    SPDLOG_INFO("Loading config from {}", ini_path.string());

    auto file = mINI::INIFile{ini_path.string()};
    auto ini = mINI::INIStructure{};
    if (!file.read(ini))
    {
        SPDLOG_WARN("Failed to read config");
        return;
    }

    auto &overlay = ini["overlay"];
    try_parse_boolean(overlay, "show_in_game_name", show_in_game_name);
    try_parse_boolean(overlay, "show_level", show_level);
    try_parse_boolean(overlay, "show_invaders", show_invaders);
    try_parse_boolean(overlay, "show_steam_name", show_steam_name);
    try_parse_boolean(overlay, "show_steam_avatar", show_steam_avatar);
    try_parse_boolean(overlay, "show_steam_relationship", show_steam_relationship);
    try_parse_boolean(overlay, "show_ping", show_ping);
    try_parse_boolean(overlay, "show_yourself", show_yourself);

    if (overlay.has("high_ping"))
    {
        high_ping = strtoul(overlay["high_ping"].data(), nullptr, 10);
    }
    else
    {
        SPDLOG_WARN("Missing config \"high_ping\"", high_ping);
    }

    auto &actions = ini["actions"];
    try_parse_keycode(actions, "toggle_logs", toggle_logs_key);
    try_parse_keycode(actions, "toggle_player_list", toggle_player_list_key);
    try_parse_keycode(actions, "block_player", block_player_key);
    try_parse_keycode(actions, "disconnect", disconnect_key);

    auto &misc = ini["misc"];
    try_parse_boolean(misc, "debug", debug);

    SPDLOG_INFO("mod_folder = {}", mod_folder.string());

    SPDLOG_INFO("show_in_game_name = {}", show_in_game_name);
    SPDLOG_INFO("show_level = {}", show_level);
    SPDLOG_INFO("show_invaders = {}", show_invaders);
    SPDLOG_INFO("show_steam_name = {}", show_steam_name);
    SPDLOG_INFO("show_steam_avatar = {}", show_steam_avatar);
    SPDLOG_INFO("show_steam_relationship = {}", show_steam_relationship);
    SPDLOG_INFO("show_ping = {}", show_ping);
    SPDLOG_INFO("high_ping = {}", high_ping);
    SPDLOG_INFO("show_yourself = {}", show_yourself);

    SPDLOG_INFO("toggle_player_list = 0x{:x}", toggle_player_list_key);
    SPDLOG_INFO("block_player = 0x{:x}", block_player_key);
    SPDLOG_INFO("disconnect = 0x{:x}", disconnect_key);

    SPDLOG_INFO("debug = {}", debug);
}
