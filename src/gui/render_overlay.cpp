#include "render_overlay.hpp"
#include "render_player_list.hpp"
#include "styles.hpp"

#include "../config.hpp"

#include <elden-x/utils/modutils.hpp>
#include <steam/isteamfriends.h>
#include <steam/steam_api_flat.h>
#include <steam/steamclientpublic.h>

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

struct steam_friends_map_st
{
    // clang-format off
    virtual void unk0() {}
    virtual void unk1() {}
    virtual void unk2() {}
    virtual void unk3() {}
    virtual void unk4() {}
    virtual void unk5() {}
    virtual void unk6() {}
    virtual void unk7() {}
    virtual void unk8() {}
    virtual void unk9() {}
    virtual void unk10() {}
    virtual void unk11() {}
    virtual void unk12() {}
    // clang-format on
    virtual EFriendRelationship get_relationship(CSteamID)
    {
        return k_EFriendRelationshipNone;
    }
};

struct steam_friends_impl_st : public ISteamFriends
{
    steam_friends_map_st *friends_map;
};

static EFriendRelationship (*GetFriendRelationship)(ISteamFriends *, CSteamID);
static EFriendRelationship GetFriendRelationship_hook(ISteamFriends *_this, CSteamID steam_id)
{
    auto res = static_cast<steam_friends_impl_st *>(_this)->friends_map->get_relationship(steam_id);

    SPDLOG_INFO("GetFriendRelationship {} -> {}", steam_id.ConvertToUint64(), (int)res);

    return k_EFriendRelationshipNone;
}

void gg::gui::initialize_overlay()
{
    ImGui::GetStyle().WindowBorderSize = 0;

    load_font(gg::config::mod_folder / "assets" / "FOT-Matisse ProN DB.ttf");

    initialize_player_list();

    auto vftable = *(void ***)SteamFriends();

    // vftable[5] = &get_friend_relationship_hook;

    // GetFriendRelationship = (EFriendRelationship(*)(ISteamFriends *, CSteamID))vftable[5];

    // modutils::hook({.address = vftable[7]}, GetFriendPersonaName_hook, GetFriendPersonaName);
    // modutils::enable_hooks();

    // SPDLOG_INFO("steam friends = {}", (void *)SteamFriends());

    SPDLOG_INFO("SteamFriends() = {}", (void *)SteamFriends());
    SPDLOG_INFO("vftable[5] = {}", vftable[5]);

    modutils::hook({.address = vftable[5]}, GetFriendRelationship_hook, GetFriendRelationship);
    modutils::enable_hooks();
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
