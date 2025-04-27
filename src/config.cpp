#include "config.hpp"

#include <imgui.h>
#include <mini/ini.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <unordered_map>

using namespace std;
namespace fs = std::filesystem;

static HINSTANCE mod_handle;

fs::path gg::config::mod_folder;

bool gg::config::show_in_game_name = true;
bool gg::config::show_level = true;
bool gg::config::show_steam_name = true;
bool gg::config::show_steam_avatar = true;
bool gg::config::show_steam_relationship = true;
bool gg::config::show_ping = true;
unsigned int gg::config::high_ping = 100;
bool gg::config::show_yourself = false;

ImGuiKey gg::config::toggle_logs_key = ImGuiKey_GraveAccent;
ImGuiKey gg::config::toggle_player_list_key = ImGuiKey_F2;
ImGuiKey gg::config::block_player_key = ImGuiKey_F3;
ImGuiKey gg::config::disconnect_key = ImGuiKey_F4;

bool gg::config::debug = false;

static void try_parse_boolean(mINI::INIMap<string> &config, const string &name, bool &out) {
    if (!config.has(name)) {
        SPDLOG_WARN("Missing config \"{}\"", name);
        return;
    }

    auto &value = config[name];
    if (value == "true") {
        out = true;
    } else if (value == "false") {
        out = false;
    } else {
        SPDLOG_WARN("Invalid config value \"{} = {}\"", name, value);
    }
};

// clang-format off
static unordered_map<string, ImGuiKey> keycodes_by_name = {{
    {"tab", ImGuiKey_Tab}, {"leftarrow", ImGuiKey_LeftArrow}, {"rightarrow", ImGuiKey_RightArrow},
    {"uparrow", ImGuiKey_UpArrow}, {"downarrow", ImGuiKey_DownArrow}, {"pageup", ImGuiKey_PageUp},
    {"pagedown", ImGuiKey_PageDown}, {"home", ImGuiKey_Home}, {"end", ImGuiKey_End},
    {"insert", ImGuiKey_Insert}, {"delete", ImGuiKey_Delete}, {"backspace", ImGuiKey_Backspace},
    {"space", ImGuiKey_Space}, {"enter", ImGuiKey_Enter}, {"escape", ImGuiKey_Escape},
    {"leftctrl", ImGuiKey_LeftCtrl}, {"leftshift", ImGuiKey_LeftShift},
    {"leftalt", ImGuiKey_LeftAlt}, {"leftsuper", ImGuiKey_LeftSuper},
    {"rightctrl", ImGuiKey_RightCtrl}, {"rightshift", ImGuiKey_RightShift},
    {"rightalt", ImGuiKey_RightAlt}, {"rightsuper", ImGuiKey_RightSuper},
    {"menu", ImGuiKey_Menu}, {"0", ImGuiKey_0}, {"1", ImGuiKey_1}, {"2", ImGuiKey_2},
    {"3", ImGuiKey_3}, {"4", ImGuiKey_4}, {"5", ImGuiKey_5}, {"6", ImGuiKey_6}, {"7", ImGuiKey_7},
    {"8", ImGuiKey_8}, {"9", ImGuiKey_9}, {"a", ImGuiKey_A}, {"b", ImGuiKey_B}, {"c", ImGuiKey_C},
    {"d", ImGuiKey_D}, {"e", ImGuiKey_E}, {"f", ImGuiKey_F}, {"g", ImGuiKey_G}, {"h", ImGuiKey_H},
    {"i", ImGuiKey_I}, {"j", ImGuiKey_J}, {"k", ImGuiKey_K}, {"l", ImGuiKey_L}, {"m", ImGuiKey_M},
    {"n", ImGuiKey_N}, {"o", ImGuiKey_O}, {"p", ImGuiKey_P}, {"q", ImGuiKey_Q}, {"r", ImGuiKey_R},
    {"s", ImGuiKey_S}, {"t", ImGuiKey_T}, {"u", ImGuiKey_U}, {"v", ImGuiKey_V}, {"w", ImGuiKey_W},
    {"x", ImGuiKey_X}, {"y", ImGuiKey_Y}, {"z", ImGuiKey_Z}, {"f1", ImGuiKey_F1},
    {"f2", ImGuiKey_F2}, {"f3", ImGuiKey_F3}, {"f4", ImGuiKey_F4}, {"f5", ImGuiKey_F5},
    {"f6", ImGuiKey_F6}, {"f7", ImGuiKey_F7}, {"f8", ImGuiKey_F8}, {"f9", ImGuiKey_F9},
    {"f10", ImGuiKey_F10}, {"f11", ImGuiKey_F11}, {"f12", ImGuiKey_F12}, {"f13", ImGuiKey_F13},
    {"f14", ImGuiKey_F14}, {"f15", ImGuiKey_F15}, {"f16", ImGuiKey_F16}, {"f17", ImGuiKey_F17},
    {"f18", ImGuiKey_F18}, {"f19", ImGuiKey_F19}, {"f20", ImGuiKey_F20}, {"f21", ImGuiKey_F21},
    {"f22", ImGuiKey_F22}, {"f23", ImGuiKey_F23}, {"f24", ImGuiKey_F24},
    {"apostrophe", ImGuiKey_Apostrophe}, {"comma", ImGuiKey_Comma}, {"minus", ImGuiKey_Minus},
    {"period", ImGuiKey_Period}, {"slash", ImGuiKey_Slash}, {"semicolon", ImGuiKey_Semicolon},
    {"equal", ImGuiKey_Equal}, {"leftbracket", ImGuiKey_LeftBracket},
    {"backslash", ImGuiKey_Backslash}, {"rightbracket", ImGuiKey_RightBracket},
    {"graveaccent", ImGuiKey_GraveAccent}, {"capslock", ImGuiKey_CapsLock},
    {"scrolllock", ImGuiKey_ScrollLock}, {"numlock", ImGuiKey_NumLock},
    {"printscreen", ImGuiKey_PrintScreen}, {"pause", ImGuiKey_Pause}, {"keypad0", ImGuiKey_Keypad0},
    {"keypad1", ImGuiKey_Keypad1}, {"keypad2", ImGuiKey_Keypad2}, {"keypad3", ImGuiKey_Keypad3},
    {"keypad4", ImGuiKey_Keypad4}, {"keypad5", ImGuiKey_Keypad5}, {"keypad6", ImGuiKey_Keypad6},
    {"keypad7", ImGuiKey_Keypad7}, {"keypad8", ImGuiKey_Keypad8}, {"keypad9", ImGuiKey_Keypad9},
    {"keypaddecimal", ImGuiKey_KeypadDecimal}, {"keypaddivide", ImGuiKey_KeypadDivide},
    {"keypadmultiply", ImGuiKey_KeypadMultiply}, {"keypadsubtract", ImGuiKey_KeypadSubtract},
    {"keypadadd", ImGuiKey_KeypadAdd}, {"keypadenter", ImGuiKey_KeypadEnter},
    {"keypadequal", ImGuiKey_KeypadEqual}, {"appback", ImGuiKey_AppBack},
    {"appforward", ImGuiKey_AppForward}, {"gamepadstart", ImGuiKey_GamepadStart},
}};
// clang-format on

static void try_parse_keycode(mINI::INIMap<string> &config, const string &name, ImGuiKey &out) {
    if (!config.has(name)) {
        SPDLOG_WARN("Missing config \"{}\"", name);
        return;
    }

    auto value = config[name];
    ranges::transform(value, value.begin(), ::tolower);

    auto it = keycodes_by_name.find(value);
    if (it != keycodes_by_name.end()) {
        out = it->second;
    } else {
        SPDLOG_WARN("Invalid config value \"{} = {}\"", name, value);
    }
};

void gg::config::set_handle(HINSTANCE mod_handle) {
    ::mod_handle = mod_handle;

    wchar_t dll_filename[MAX_PATH] = {0};
    GetModuleFileNameW(mod_handle, dll_filename, MAX_PATH);
    mod_folder = fs::path{dll_filename}.parent_path();
}

void gg::config::load() {
    auto ini_path = mod_folder / "ergg.ini";
    SPDLOG_INFO("Loading config from {}", ini_path.string());

    auto file = mINI::INIFile{ini_path.string()};
    auto ini = mINI::INIStructure{};
    if (!file.read(ini)) {
        SPDLOG_WARN("Failed to read config");
        return;
    }

    auto &overlay = ini["overlay"];
    try_parse_boolean(overlay, "show_in_game_name", show_in_game_name);
    try_parse_boolean(overlay, "show_level", show_level);
    try_parse_boolean(overlay, "show_steam_name", show_steam_name);
    try_parse_boolean(overlay, "show_steam_avatar", show_steam_avatar);
    try_parse_boolean(overlay, "show_steam_relationship", show_steam_relationship);
    try_parse_boolean(overlay, "show_ping", show_ping);
    try_parse_boolean(overlay, "show_yourself", show_yourself);

    if (overlay.has("high_ping")) {
        high_ping = strtoul(overlay["high_ping"].data(), nullptr, 10);
    } else {
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
    SPDLOG_INFO("show_steam_name = {}", show_steam_name);
    SPDLOG_INFO("show_steam_avatar = {}", show_steam_avatar);
    SPDLOG_INFO("show_steam_relationship = {}", show_steam_relationship);
    SPDLOG_INFO("show_ping = {}", show_ping);
    SPDLOG_INFO("high_ping = {}", high_ping);
    SPDLOG_INFO("show_yourself = {}", show_yourself);

    SPDLOG_INFO("toggle_player_list = 0x{:x}", (int)toggle_player_list_key);
    SPDLOG_INFO("block_player = 0x{:x}", (int)block_player_key);
    SPDLOG_INFO("disconnect = 0x{:x}", (int)disconnect_key);

    SPDLOG_INFO("debug = {}", debug);
}

optional<span<unsigned char>> gg::config::get_resource(string name, string type) {
    auto res = FindResourceA(mod_handle, name.data(), type.data());
    if (!res) {
        SPDLOG_CRITICAL("Failed to find mod resource ({}) {} {}", GetLastError(), name, type);
        return {};
    }
    auto size = SizeofResource(mod_handle, res);
    if (!size) {
        SPDLOG_CRITICAL("Failed to get size of mod resource ({}) {} {}", GetLastError(), name,
                        type);
        return {};
    }
    auto handle = LoadResource(mod_handle, res);
    if (!handle) {
        SPDLOG_CRITICAL("Failed to load mod resource ({}) {} {}", GetLastError(), name, type);
        return {};
    }
    auto data = reinterpret_cast<unsigned char *>(LockResource(handle));
    if (!data) {
        SPDLOG_CRITICAL("Failed to get mod resource data ({}) {} {}", GetLastError(), name, type);
        return {};
    }

    return span{data, size};
}