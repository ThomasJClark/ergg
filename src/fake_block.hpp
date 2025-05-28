#pragma once

#include <steam/steamclientpublic.h>

#include "player_list.hpp"

namespace gg {

void initialize_fake_block();

/**
 * @returns true if the given player is on the mod's blocklist
 */
void block_player(const player_list_entry &);

/**
 * Add the given player to the blocklist, and flush the list to disk
 */
bool is_player_blocked(CSteamID);

}
