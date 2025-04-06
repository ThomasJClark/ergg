#pragma once

#include "renderer/texture.hpp"

#include <elden-x/chr/player.hpp>

#include <steam/isteamfriends.h>

#include <memory>
#include <string>
#include <vector>

namespace gg {

/**
 * A single entry for a player we're connected to in the current session. This module is responsible
 * for populating these structs with data, and player_list_ui.cpp renders them to a fancy overlay.
 */
struct player_list_entry {
    er::CS::PlayerIns *player{nullptr};
    std::string in_game_name;
    std::string steam_name;
    std::shared_ptr<gg::renderer::texture> steam_avatar;
    EFriendRelationship steam_relationship{k_EFriendRelationshipNone};
    int steam_ping{-1};
    int steam_ping_cumulative_error{0};

    float connection_quality_local{-1.f};
    float connection_quality_remote{-1.f};
};

/**
 * List of players in the current session, or empty for slots that don't currently have a player.
 * This includes empty slots so that players stay in the same index and we don't need to do
 * expensive initialization again.
 */
extern std::vector<std::optional<player_list_entry>> player_list_entries;

/**
 * Update the list of player info based on the current players in the session
 */
void update_player_list();

}
