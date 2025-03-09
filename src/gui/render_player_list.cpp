#include <steam/steamclientpublic.h>

#include "render_block_player.hpp"
#include "render_disconnect.hpp"
#include "render_player_list.hpp"
#include "styles.hpp"
#include "utils.hpp"

#include "../config.hpp"
#include "../player_list.hpp"
#include "../renderer/texture.hpp"

#include <imgui.h>

#include <algorithm>
#include <utility>

using namespace std;

static const auto vip_steam_id = CSteamID{108371544u, k_EUniversePublic, k_EAccountTypeIndividual};

static shared_ptr<gg::renderer::texture_st> container_background_texture;
static shared_ptr<gg::renderer::texture_st> entry_background_texture;
static shared_ptr<gg::renderer::texture_st> menu_fe_namebase;

/**
 * Draw saved information about a player in the game session to an ImGui table row
 */
static void render_player_list_entry(const gg::player_list_entry_st &entry, int index)
{
    auto text_offset_y = ceilf((gg::gui::player_list_row_height - gg::gui::font_size) / 2);
    auto avatar_offset_y =
        ceilf((gg::gui::player_list_row_height - gg::gui::player_list_avatar_size.y) / 2);

    // Color friends in green, blocked players in red, and me in blue
    auto color = ImVec4{};
    if (gg::config::show_steam_relationship &&
        entry.steam_relationship == k_EFriendRelationshipFriend)
    {
        color = gg::gui::green;
    }
    else if (gg::config::show_steam_relationship &&
             entry.steam_relationship == k_EFriendRelationshipIgnored)
    {
        color = gg::gui::red;
    }
    else if (entry.player && entry.player->session_holder.network_session &&
             entry.player->session_holder.network_session->steam_id == vip_steam_id)
    {
        color = gg::gui::blue;
    }

    ImGui::TableNextRow(ImGuiTableRowFlags_None, gg::gui::player_list_row_height * gg::gui::scale);

    // Column 1 - render the avatar on the player's Steam profile, if there is one
    if (gg::config::show_steam_avatar)
    {
        ImGui::TableNextColumn();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + avatar_offset_y * gg::gui::scale);
        if (entry.steam_avatar)
        {
            // Outline the avatar with the highlight color, if any
            if (color.w)
            {
                ImGui::GetForegroundDrawList()->AddRect(
                    ImGui::GetCursorScreenPos() - ImVec2{.5f, .5f} * gg::gui::scale,
                    ImGui::GetCursorScreenPos() +
                        (gg::gui::player_list_avatar_size + ImVec2{.5f, .5f}) * gg::gui::scale,
                    ImGui::GetColorU32(color), gg::gui::scale, ImDrawFlags_None,
                    2.f * gg::gui::scale);
            }

            ImGui::Image(entry.steam_avatar->desc.second.ptr,
                         gg::gui::player_list_avatar_size * gg::gui::scale);
        }
    }

    // Column 2 - render the player's in-game name and Steam profile name
    auto show_in_game_name = gg::config::show_in_game_name && !entry.in_game_name.empty();
    auto show_steam_name = gg::config::show_steam_name && !entry.steam_name.empty();
    auto &name_color = color.w ? color : gg::gui::white;

    ImGui::TableNextColumn();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + text_offset_y * gg::gui::scale);
    if (show_in_game_name)
    {
        ImGui::TextColored(name_color, "%s", entry.in_game_name.data());
        if (show_steam_name)
        {
            ImGui::SameLine(0.f, 0.f);
            ImGui::TextColored(gg::gui::pale_gold, " (%s)", entry.steam_name.data());
        }
    }
    else if (show_steam_name)
    {
        ImGui::TextColored(name_color, "%s", entry.steam_name.data());
    }
    else
    {
        ImGui::TextColored(name_color, "Player %d", index + 1);
    }

    // Column 3 - rune level
    if (gg::config::show_level)
    {
        ImGui::TableNextColumn();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + text_offset_y * gg::gui::scale);
        if (entry.player)
        {
            ImGui::TextColored(gg::gui::white, "Level %d", entry.player->game_data->rune_level);
        }
        else
        {
            ImGui::TextColored(gg::gui::white, "Level 90");
        }
    }

    // Column 4 - ping time estimated by Steam
    if (gg::config::show_ping)
    {
        ImGui::TableNextColumn();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + text_offset_y * gg::gui::scale);
        if (entry.steam_ping > 0)
        {
            auto color = entry.steam_ping > gg::config::high_ping ? gg::gui::red : gg::gui::white;
            ImGui::TextColored(color, "%dms", entry.steam_ping);
        }
    }
}

void gg::gui::initialize_player_list()
{
    container_background_texture =
        renderer::load_texture_from_file(gg::config::mod_folder / "assets/MENU_FL_Equip_waku.png");
    entry_background_texture =
        renderer::load_texture_from_file(gg::config::mod_folder / "assets/MENU_FL_Arts_waku3.png");
    menu_fe_namebase =
        renderer::load_texture_from_file(gg::config::mod_folder / "assets/MENU_FE_NameBase.png");

    initialize_block_player();
    initialize_disconnect();
}

void gg::gui::render_player_list()
{
    static bool is_block_player_open = false;
    static bool is_disconnect_open = false;

    // Alpha of the overlay, so we can do a cool fade-in animation when it appears
    static float alpha = 0.f;

    update_player_list();

    auto player_count =
        ranges::count_if(player_list_entries, [](auto entry) { return entry.has_value(); });

    // Skip rendering the overlay if there are no entries, so we don't ever show a blank rectangle
    if (player_count == 0)
    {
        alpha = 0.f;
        is_block_player_open = false;
        is_disconnect_open = false;
        return;
    }

    if (alpha < 1.f)
    {
        const float fade_in_time = 0.5f;
        alpha = fminf(alpha + ImGui::GetIO().DeltaTime / fade_in_time, 1.f);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{4, 0} * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    auto viewport = ImGui::GetMainViewport();

    auto next_window_pos = viewport->WorkPos + ImVec2{viewport->WorkSize.x, 0.f};

    // Offset to take into account aspects other than 16:9
    next_window_pos.x -= (viewport->WorkSize.x - virtual_width * scale) / 2.f;
    next_window_pos.y -= (viewport->WorkSize.y - virtual_height * scale) / 2.f;

    // Margin from edge of screen
    next_window_pos += ImVec2{-56, 35} * scale;

    ImGui::SetNextWindowPos(next_window_pos, ImGuiCond_Always, {1, 0});

    ImGui::SetNextWindowBgAlpha(0);
    ImGui::Begin("player_list", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav);

    int column_count = 1;
    if (config::show_steam_avatar)
        column_count++;
    if (config::show_ping)
        column_count++;
    if (config::show_level)
        column_count++;

    ImGui::BeginTable("player_list_table", column_count);
    int index = 0;
    for (auto &entry : player_list_entries)
    {
        if (entry.has_value())
        {
            index++;
            render_player_list_entry(entry.value(), index);
        }
    }
    ImGui::EndTable();

    auto windowpos = ImGui::GetWindowPos();
    auto windowsize = ImGui::GetWindowSize();

    ImGui::End();

    // Draw a background behind the entire overlay
    if (container_background_texture)
    {
        auto padding = ImVec2{32, 28} * scale;
        auto pos = windowpos - padding;
        auto size = windowsize + padding * 2.f;
        render_nine_slice(ImGui::GetBackgroundDrawList(),
                          container_background_texture->desc.second.ptr,
                          {(float)container_background_texture->width,
                           (float)container_background_texture->height},
                          pos, size, {56.f, 56.f}, .8f);
    }

    // Draw a transprent texture behind each entry in the list
    if (menu_fe_namebase)
    {
        auto padding = ImVec2{16.f, 0.f} * scale;
        auto pos = windowpos - padding;
        auto size = ImVec2{windowsize.x, player_list_row_height * scale} + padding * 2.f;
        for (int i = 0; i < player_count; i++)
        {
            render_nine_slice(ImGui::GetBackgroundDrawList(), menu_fe_namebase->desc.second.ptr,
                              {(float)menu_fe_namebase->width, (float)menu_fe_namebase->height},
                              pos, size, {36.f, 0.f}, .8f);

            pos.y += player_list_row_height * scale;
        }
    }

    // Cross out dead players
    if (entry_background_texture)
    {
        auto pos = windowpos - ImVec2{10.f, 8.f} * scale;
        auto size =
            ImVec2{windowsize.x, player_list_row_height * scale} + ImVec2{12.f, 15.f} * scale;
        for (auto entry : player_list_entries)
        {
            if (entry.has_value())
            {
                if (entry->player && entry->player->game_data->hp == 0)
                {
                    render_nine_slice(ImGui::GetForegroundDrawList(),
                                      entry_background_texture->desc.second.ptr,
                                      {(float)entry_background_texture->width / 2.f,
                                       (float)entry_background_texture->height / 2.f},
                                      pos, size, {8.5f, 11.f});
                }

                pos.y += player_list_row_height * scale;
            }
        }
    }

    render_block_player(is_block_player_open, windowpos, player_count);
    render_disconnect(is_disconnect_open, windowpos, windowsize);

    ImGui::PopStyleVar(ImGuiStyleVar_WindowBorderSize);
    ImGui::PopStyleVar(ImGuiStyleVar_WindowPadding);
    ImGui::PopStyleVar(ImGuiStyleVar_CellPadding);
    ImGui::PopStyleVar(ImGuiStyleVar_Alpha);
}
