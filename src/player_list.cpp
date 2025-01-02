#include "player_list.hpp"

#include "config.hpp"

#include <elden-x/chr/world_chr_man.hpp>
#include <elden-x/now_loading_helper.hpp>
#include <steam/isteamfriends.h>
#include <steam/isteamnetworkingmessages.h>
#include <steam/isteamuser.h>
#include <steam/isteamutils.h>

#include <codecvt>

using namespace std;

vector<optional<gg::player_list_entry_st>> gg::player_list_entries = {};

static wstring_convert<codecvt_utf8_utf16<wchar_t>, wchar_t> utf16_convert;

/**
 * Get a player's Steam profile avatar if available for quick visual identification
 */
static shared_ptr<gg::renderer::texture_st> load_player_steam_avatar(CSteamID steam_id)
{
    static vector<unsigned char> avatar_buffer(32 * 32 * 4);

    auto avatar = SteamFriends()->GetSmallFriendAvatar(steam_id);
    if (!avatar)
    {
        return nullptr;
    }

    unsigned int avatar_width;
    unsigned int avatar_height;
    if (!SteamUtils()->GetImageSize(avatar, &avatar_width, &avatar_height))
    {
        return nullptr;
    }

    avatar_buffer.resize(avatar_width * avatar_height * 4);
    if (!SteamUtils()->GetImageRGBA(avatar, avatar_buffer.data(), avatar_buffer.size()))
    {
        return nullptr;
    }

    return gg::renderer::load_texture_from_raw_data(avatar_buffer.data(), avatar_width,
                                                    avatar_height);
}

void gg::update_player_list()
{
    if (config::debug)
    {
        // When numpad 0 is pressed and debug mode is enabled, toggle some sample data for quickly
        // testing the mod without going online
        static bool show_test_data = false;
        if (GetAsyncKeyState(VK_NUMPAD0) & 1)
        {
            player_list_entries.clear();
            show_test_data = !show_test_data;
            if (show_test_data)
            {
                auto avatar = load_player_steam_avatar(SteamUser()->GetSteamID());
                player_list_entries.resize(3);
                player_list_entries[0].emplace(nullptr, "Tom", "Tom", avatar,
                                               k_EFriendRelationshipNone, 48);
                player_list_entries[1].emplace(nullptr, "Bingus", "Bingus", avatar,
                                               k_EFriendRelationshipIgnored, 31);
                player_list_entries[2].emplace(nullptr, "Guts", "John Steamfriend", avatar,
                                               k_EFriendRelationshipFriend, 93);
            }
        }

        if (show_test_data)
        {
            return;
        }
    }

    auto now_loading_helper = er::CS::CSNowLoadingHelper::instance();
    if ((!now_loading_helper || !now_loading_helper->loaded1))
    {
        player_list_entries.clear();
        return;
    }

    auto world_chr_man = er::CS::WorldChrManImp::instance();
    if (!world_chr_man)
    {
        player_list_entries.clear();
        return;
    }

    player_list_entries.resize(world_chr_man->player_chr_set.capacity());
    for (int i = 0; i < player_list_entries.size(); i++)
    {
        auto player = world_chr_man->player_chr_set.at(i);
        auto &entry = player_list_entries.at(i);

        if (player && player->session_holder.network_session &&
            (gg::config::show_yourself || player != world_chr_man->main_player))
        {
            auto steam_id = CSteamID{player->session_holder.network_session->steam_id};

            // If this slot was previously empty or had a different player, construct a new
            // entry for this player
            if (!entry || entry->player != player)
            {
                entry = gg::player_list_entry_st{};
                entry->player = player;

                if (gg::config::show_steam_avatar)
                {
                    entry->steam_avatar = load_player_steam_avatar(steam_id);
                }

                if (gg::config::show_in_game_name)
                {
                    entry->in_game_name = utf16_convert.to_bytes(player->game_data->name_c_str);
                }
            }

            if (gg::config::show_steam_name)
            {
                entry->steam_name = SteamFriends()->GetFriendPersonaName(steam_id);
            }

            if (gg::config::show_ping)
            {

                // Ping changes throughout a session, and is already free from Steam
                auto steam_connection_status = SteamNetConnectionRealTimeStatus_t{};
                auto steam_net_id = SteamNetworkingIdentity{};
                steam_net_id.SetSteamID(steam_id);
                SteamNetworkingMessages()->GetSessionConnectionInfo(steam_net_id, nullptr,
                                                                    &steam_connection_status);

                entry->steam_ping_cumulative_error +=
                    steam_connection_status.m_nPing - entry->steam_ping;

                // Only update ping if it's consistently far off, to avoid UI flickering
                if (entry->steam_ping <= 0 || abs(entry->steam_ping_cumulative_error) > 100)
                {
                    entry->steam_ping = steam_connection_status.m_nPing;
                    entry->steam_ping_cumulative_error = 0;
                }
            }

            if (gg::config::show_steam_relationship)
            {
                entry->steam_relationship = SteamFriends()->GetFriendRelationship(steam_id);
            }
        }
        else
        {
            entry.reset();
        }
    }
}