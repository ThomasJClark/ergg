#define WIN32_LEAN_AND_MEAN

#include <steam/steamclientpublic.h>

#include "render_block_player.hpp"
#include "styles.hpp"
#include "utils.hpp"

#include "../config.hpp"
#include "../fake_block.hpp"
#include "../player_list.hpp"
#include "../renderer/texture.hpp"

#include <imgui.h>

#include <array>
#include <memory>

using namespace std;

static const auto number_key_files =
    array{"KG_Key_1", "KG_Key_2", "KG_Key_3", "KG_Key_4", "KG_Key_5",
          "KG_Key_6", "KG_Key_7", "KG_Key_8", "KG_Key_9"};

static auto number_key_textures =
    array<shared_ptr<gg::renderer::texture>, number_key_files.size()>{};

void gg::gui::initialize_block_player() {
    ranges::transform(number_key_files, number_key_textures.begin(),
                      [](auto file) { return renderer::load_texture_from_resource(file); });

    initialize_fake_block();
}

void gg::gui::render_block_player(bool &is_open, const ImVec2 &window_pos, int player_count) {
    static int last_player_count = 0;

    int effective_player_count = player_count;
    if (effective_player_count > number_key_textures.size()) {
        effective_player_count = number_key_textures.size();
    }

    // Enter block mode when the configured key is pressed
    if (ImGui::IsKeyPressed(gg::config::block_player_key)) {
        is_open = !is_open;
    }

    if (is_open) {
        // When in block mode, a number key can be pressed to block a single player
        int slot = 0;
        for (auto &entry : player_list_entries) {
            if (!entry.has_value()) {
                continue;
            }

            if ((ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + slot))) ||
                (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_Keypad1 + slot)))) {
                if (entry->player && entry->player->session_holder.network_session) {
                    block_player(entry->player->session_holder.network_session->steam_id);
                }
                is_open = false;
                break;
            }

            slot++;
            if (slot >= number_key_textures.size()) {
                break;
            }
        }
    }

    // When a player joins or leaves the session, automatically hide the block UI so the player
    // doesn't accidentally block the wrong person
    if (player_count != last_player_count) {
        is_open = false;
        last_player_count = player_count;
    }

    // When in block mode, show a keycode symbol next to each player and block that player if the
    // key is pressed
    static fade_in_out fade_in_out;
    if (fade_in_out.animate(is_open)) {
        auto color = ImGui::ColorConvertFloat4ToU32({1.f, 1.f, 1.f, fade_in_out.alpha});
        auto size = gg::gui::player_list_avatar_size;
        auto pos = window_pos;
        pos.x -= size.x + 8.f;
        pos.y += ceilf((gg::gui::player_list_row_height - gg::gui::player_list_avatar_size.y) / 2);
        for (int i = 0; i < effective_player_count; i++) {
            auto &texture = number_key_textures[i];
            ImGui::GetForegroundDrawList()->AddImage(texture->id(), pos, pos + size, {0.f, 0.f},
                                                     {1.f, 1.f}, color);
            pos.y += gg::gui::player_list_row_height;
        }
    }
}
