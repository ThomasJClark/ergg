#define WIN32_LEAN_AND_MEAN

#include "render_view_player.hpp"
#include "styles.hpp"

#include "../config.hpp"
#include "../player_list.hpp"
#include "../renderer/texture.hpp"

#include <imgui.h>

#include <steam/isteamfriends.h>

#include <windows.h>

#include <algorithm>
#include <array>
#include <memory>

using namespace std;

static const auto number_key_files =
    array{"KG_Key_1.png", "KG_Key_2.png", "KG_Key_3.png", "KG_Key_4.png", "KG_Key_5.png",
          "KG_Key_6.png", "KG_Key_7.png", "KG_Key_8.png", "KG_Key_9.png"};

static auto number_key_textures =
    array<shared_ptr<gg::renderer::texture_st>, number_key_files.size()>{};

static constexpr float fade_time = 0.1f;

void gg::gui::initialize_view_player()
{
    ranges::transform(number_key_files, number_key_textures.begin(), [](auto file) {
        return renderer::load_texture_from_file(gg::config::mod_folder / "assets" / file);
    });
}

void gg::gui::render_view_player(bool &is_open, const ImVec2 &window_pos, int player_count)
{
    static float alpha = 0.f;
    static int last_player_count = 0;

    int effective_player_count = player_count;
    if (effective_player_count > number_key_textures.size())
    {
        effective_player_count = number_key_textures.size();
    }

    // Enter block mode when the configured key is pressed
    if (GetAsyncKeyState(gg::config::view_player_key) & 1)
    {
        is_open = !is_open;
    }

    if (is_open)
    {
        // When in block mode, a number key can be pressed to block a single player
        int slot = 0;
        for (auto &entry : player_list_entries)
        {
            if (!entry.has_value())
            {
                continue;
            }

            if ((GetAsyncKeyState('1' + slot) & 1) || (GetAsyncKeyState(VK_NUMPAD1 + slot) & 1))
            {
                if (entry->player && entry->player->session_holder.network_session)
                {
                    SteamFriends()->ActivateGameOverlayToUser(
                        "steamid",
                        CSteamID{entry->player->session_holder.network_session->steam_id});
                }
                is_open = false;
                break;
            }

            slot++;
            if (slot >= number_key_textures.size())
            {
                break;
            }
        }
    }

    // When a player joins or leaves the session, automatically hide the view UI so the player
    // doesn't accidentally view the wrong person
    if (player_count != last_player_count)
    {
        is_open = false;
        alpha = 0.f;
        last_player_count = player_count;
    }

    // When in view mode, show a keycode symbol next to each player and view that player if the
    // key is pressed
    alpha += ImGui::GetIO().DeltaTime / fade_time * (is_open ? 1.f : -1.f);
    alpha = clamp(alpha, 0.f, 1.f);
    if (alpha > 0.f)
    {
        auto color = ImGui::ColorConvertFloat4ToU32({1.f, 1.f, 1.f, alpha});
        auto size = gg::gui::player_list_avatar_size;
        auto pos = window_pos;
        pos.x -= size.x + 8.f;
        pos.y += ceilf((gg::gui::player_list_row_height - gg::gui::player_list_avatar_size.y) / 2);
        for (int i = 0; i < effective_player_count; i++)
        {
            auto &texture = number_key_textures[i];
            ImGui::GetForegroundDrawList()->AddImage(texture->desc.second.ptr, pos, pos + size,
                                                     {0.f, 0.f}, {1.f, 1.f}, color);
            pos.y += gg::gui::player_list_row_height;
        }
    }
}
