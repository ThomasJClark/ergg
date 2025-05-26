#include <steam/isteamfriends.h>
#include <steam/isteamnetworkingmessages.h>
#include <steam/isteamuser.h>
#include <steam/isteamutils.h>
#include <steam/steamclientpublic.h>

#include "player_list.hpp"

#include "config.hpp"

#include <elden-x/chr/world_chr_man.hpp>
#include <elden-x/now_loading_helper.hpp>

#include <codecvt>

using namespace std;

vector<optional<gg::player_list_entry>> gg::player_list_entries = {};

static wstring_convert<codecvt_utf8_utf16<wchar_t>, wchar_t> utf16_convert;

/**
 * Get a player's Steam profile avatar if available for quick visual identification
 */
static shared_ptr<gg::renderer::texture> load_player_steam_avatar(CSteamID steam_id) {
    static vector<unsigned char> avatar_buffer(32 * 32 * 4);

    auto avatar = SteamFriends()->GetSmallFriendAvatar(steam_id);
    if (!avatar) {
        return nullptr;
    }

    unsigned int avatar_width;
    unsigned int avatar_height;
    if (!SteamUtils()->GetImageSize(avatar, &avatar_width, &avatar_height)) {
        return nullptr;
    }

    avatar_buffer.resize(avatar_width * avatar_height * 4);
    if (!SteamUtils()->GetImageRGBA(avatar, avatar_buffer.data(), avatar_buffer.size())) {
        return nullptr;
    }

    return gg::renderer::load_texture_from_raw_data(avatar_buffer.data(), avatar_width,
                                                    avatar_height);
}

void gg::update_player_list() {
    if (config::debug) {
        // When numpad 0 is pressed and debug mode is enabled, toggle some sample data for quickly
        // testing the mod without going online
        static bool show_test_data = false;
        if (GetAsyncKeyState(VK_NUMPAD0) & 1) {
            player_list_entries.clear();
            show_test_data = !show_test_data;
            if (show_test_data) {
                auto avatar = load_player_steam_avatar(SteamUser()->GetSteamID());
                player_list_entries.resize(3);
                player_list_entries[0].emplace(nullptr, "Tom", "Tom", "Tom (Tom)", avatar,
                                               k_EFriendRelationshipNone, 48);
                player_list_entries[1].emplace(nullptr, "Bingus", "Bingus", "Bingus (Bingus)",
                                               avatar, k_EFriendRelationshipIgnored, 31);
                player_list_entries[2].emplace(nullptr, "Guts", "John Steamfriend",
                                               "Guts (John Steamfriend)", avatar,
                                               k_EFriendRelationshipFriend, 93);
            }
        }

        if (show_test_data) {
            return;
        }
    }

    auto now_loading_helper = er::CS::CSNowLoadingHelper::instance();
    if ((!now_loading_helper || !now_loading_helper->loaded1)) {
        player_list_entries.clear();
        return;
    }

    auto world_chr_man = er::CS::WorldChrMan::instance();
    if (!world_chr_man) {
        player_list_entries.clear();
        return;
    }

    // Resize the player list to the current capacity. We don't shrink the list at this point since
    // we want to log disconnections for players that are no longer in the session.
    auto capacity = world_chr_man->player_chr_set.capacity();
    if (capacity > player_list_entries.size()) {
        player_list_entries.resize(capacity);
    }

    // Update each remaining entry based on the current player list
    for (int i = 0; i < player_list_entries.size(); i++) {
        auto player = world_chr_man->player_chr_set.at(i);
        auto &entry = player_list_entries.at(i);

        // If the player in this entry is no longer in the slot, remove the old entry
        if (entry && entry->player != player) {
            SPDLOG_INFO("Disconnected from {}", entry->debug_name);
            entry.reset();
        }

        // Only show ourself if configured to do so, otherwise just show other players
        if (player && player->session_holder.network_session &&
            (gg::config::show_yourself || player != world_chr_man->main_player)) {
            auto steam_id = player->session_holder.network_session->steam_id;
            bool just_connected = false;

            // When adding a player to the list for the first time, look up their avatar and name.
            // No need to refresh these every update.
            if (!entry) {
                entry = gg::player_list_entry{.player = player};

                if (gg::config::show_steam_avatar) {
                    entry->steam_avatar = load_player_steam_avatar(steam_id);
                }

                if (gg::config::show_in_game_name) {
                    entry->in_game_name = utf16_convert.to_bytes(player->game_data->name_c_str);
                }

                if (gg::config::show_steam_name) {
                    entry->steam_name = SteamFriends()->GetFriendPersonaName(steam_id);
                }

                just_connected = true;
            }

            // Ping changes throughout a session, and is already free from Steam
            if (gg::config::show_ping) {
                auto steam_connection_status = SteamNetConnectionRealTimeStatus_t{};
                auto steam_net_id = SteamNetworkingIdentity{};
                steam_net_id.SetSteamID(steam_id);
                SteamNetworkingMessages()->GetSessionConnectionInfo(steam_net_id, nullptr,
                                                                    &steam_connection_status);

                entry->steam_ping_cumulative_error +=
                    steam_connection_status.m_nPing - entry->steam_ping;

                // Only update ping if it's consistently far off, to avoid UI flickering
                if (entry->steam_ping <= 0 || abs(entry->steam_ping_cumulative_error) > 100) {
                    entry->steam_ping = steam_connection_status.m_nPing;
                    entry->steam_ping_cumulative_error = 0;
                }
            }

            // Steam relationship can change throughout a session if we friend or block someone,
            // and is also free from Steam
            if (gg::config::show_steam_relationship) {
                entry->steam_relationship = SteamFriends()->GetFriendRelationship(steam_id);
            }

            if (just_connected) {
                auto has_steam_name = !entry->steam_name.empty();
                auto has_in_game_name = !entry->in_game_name.empty();

                if (has_steam_name && has_in_game_name) {
                    entry->debug_name = entry->in_game_name + " (" + entry->steam_name + ")";
                } else if (has_steam_name) {
                    entry->debug_name = entry->steam_name;
                } else if (has_in_game_name) {
                    entry->debug_name = entry->in_game_name;
                } else {
                    entry->debug_name = to_string(steam_id.ConvertToUint64());
                }

                SPDLOG_INFO("Connected to {}", entry->debug_name);
            }

        } else {
            entry.reset();
        }
    }

    player_list_entries.resize(capacity);
}