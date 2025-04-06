#pragma once

#include <steam/steamclientpublic.h>

namespace gg {

void initialize_fake_block();

/**
 * @returns true if the given player is on the mod's blocklist
 */
void block_player(CSteamID);

/**
 * Add the given player to the blocklist, and flush the list to disk
 */
bool is_player_blocked(CSteamID);

}
