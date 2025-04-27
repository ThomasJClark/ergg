#include "fake_block.hpp"

#include "config.hpp"

#include <spdlog/spdlog.h>
#include <steam/isteamfriends.h>
#include <elden-x/utils/modutils.hpp>

#include <fstream>
#include <set>

using namespace std;

static auto blocked_players = set<CSteamID>{};

static auto out_stream = ofstream{};

struct steam_friends_vftable {
    void *unk0[5];
    EFriendRelationship (*get_friend_relationship)(ISteamFriends *_this, CSteamID steam_id);
    void *unk10[74];
};

static steam_friends_vftable steam_friends_patched_vftable;

static EFriendRelationship (*get_friend_relationship)(ISteamFriends *_this, CSteamID steam_id);
static EFriendRelationship get_friend_relationship_hook(ISteamFriends *_this, CSteamID steam_id) {
    if (gg::is_player_blocked(steam_id)) {
        return k_EFriendRelationshipIgnored;
    }

    return get_friend_relationship(_this, steam_id);
}

void gg::initialize_fake_block() {
    // Hook the player relationship lookup function so we can "block" without actually changing
    // any Steam settings. There is no public Steamworks SDK method for blocking someone, these
    // blocks only apply while the mod is running.
    auto &vftable = *(steam_friends_vftable **)(void *)SteamFriends();
    steam_friends_patched_vftable = *vftable;
    get_friend_relationship = steam_friends_patched_vftable.get_friend_relationship;
    steam_friends_patched_vftable.get_friend_relationship = get_friend_relationship_hook;
    vftable = &steam_friends_patched_vftable;

    // Load already blocked players from the .txt file
    auto file_path = gg::config::mod_folder / "blocked.txt";

    auto in_stream = ifstream{};
    in_stream.open(file_path);
    if (in_stream.is_open()) {
        string line;
        while (getline(in_stream, line)) {
            auto steam_id = strtoull(line.data(), nullptr, 10);
            blocked_players.insert(steam_id);
        }
        in_stream.close();

        if (!blocked_players.empty()) {
            SPDLOG_INFO("Loaded {} blocked players from {}", blocked_players.size(),
                        file_path.string());
        }
    }

    out_stream.open(file_path, ios_base::app);
}

void gg::block_player(CSteamID steam_id) {
    if (is_player_blocked(steam_id)) {
        return;
    }

    SPDLOG_INFO("Blocking player {}", steam_id.ConvertToUint64());

    blocked_players.insert(steam_id);

    out_stream << steam_id.ConvertToUint64() << endl;
    out_stream.flush();
}

bool gg::is_player_blocked(CSteamID steam_id) { return blocked_players.contains(steam_id); }
