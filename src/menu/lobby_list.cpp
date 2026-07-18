#include "menu.h"
#include "menu/item_list.h"

#include "menu/types.h"
#include "network/network.h"

static const char* LOBBY_LIST_NO_ITEMS_MESSAGE = "Seems there's no lobbies in these parts...";
static const Rect LOBBY_CREATE_RECT = {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 128
};
static const std::vector<std::string> LOBBY_TYPE_STRS = { "Public", "Invite Only" };

void menu_lobby_list_init(MenuState* state) {
    state->item_list = menu_item_list_init(LOBBY_LIST_NO_ITEMS_MESSAGE);
    menu_lobby_list_search(state);
}

void menu_lobby_list_search(MenuState* state) {
    menu_item_list_clear(state->item_list);
    state->lobbies.clear();
    network_search_lobbies(state->item_list.search_query.c_str());
}

void menu_lobby_list_update(MenuState* state) {
    menu_item_list_update(state->item_list, state->ui_context);

    // Search
    if (state->item_list.user_requests_search) {
        menu_lobby_list_search(state);
    }

    // Button row
    ui_begin_row(state->ui_context, ivec2(BUTTON_X, ITEM_LIST_RECT.y + ITEM_LIST_RECT.h + 2), 2);
        if (ui_button(state->ui_context, "Back")) {
            #ifdef GOLD_STEAM
                menu_set_mode(state, MENU_MODE_MULTIPLAYER);
            #else
                menu_set_mode(state, MENU_MODE_MAIN);
            #endif
        }
        if (ui_button(state->ui_context, "Host")) {
            menu_set_mode(state, MENU_MODE_CREATE_LOBBY);
        }
        if (state->item_list.item_selected != MENU_ITEM_NONE) {
            if (ui_button(state->ui_context, "Join")) {
                network_join_lobby(state->lobbies[state->item_list.item_selected].connection_info);
                menu_set_mode(state, MENU_MODE_CONNECTING);
            }
        }
    ui_end_container(state->ui_context);

    // Lobby create dialog
    if (state->mode == MENU_MODE_CREATE_LOBBY) {
        menu_ui_create_lobby(state);
    }
}

void menu_lobby_list_on_lobby_found(MenuState* state, NetworkLobby lobby) {
    state->lobbies.push_back(lobby);

    char lobby_str[128];
    sprintf(lobby_str, "%s (%u/%u)", lobby.name, lobby.player_count, MAX_PLAYERS);
    state->item_list.items.push_back(std::string(lobby_str));
}

void menu_ui_create_lobby(MenuState* state) {
    // Frame
    state->ui_context.input_enabled = true;
    ui_screen_shade(state->ui_context);
    ui_frame_rect(state->ui_context, LOBBY_CREATE_RECT);

    // Header text
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Create Lobby");
    ui_element_position(state->ui_context, ivec2(LOBBY_CREATE_RECT.x + (LOBBY_CREATE_RECT.w / 2) - (header_text_size.x / 2), LOBBY_CREATE_RECT.y + 6));
    ui_text(state->ui_context, FONT_HACK_GOLD, "Create Lobby");

    // Content
    ui_begin_column(state->ui_context, ivec2(LOBBY_CREATE_RECT.x + 8, LOBBY_CREATE_RECT.y + 26), 6);
        ui_text_input(state->ui_context, "Name: ", ivec2(284, 24), &state->lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE - 1);
        menu_ui_lobby_privacy(state);
    ui_end_container(state->ui_context);

    // Back button
    ui_element_position(state->ui_context, ui_button_position_frame_bottom_left(LOBBY_CREATE_RECT));
    if (ui_button(state->ui_context, "Back")) {
        menu_set_mode(state, MENU_MODE_LOBBYLIST);
    }

    // Create button
    ui_element_position(state->ui_context, ui_button_position_frame_bottom_right(LOBBY_CREATE_RECT, "Create"));
    if (ui_button(state->ui_context, "Create")) {
        if (state->lobby_name.length() == 0) {
            menu_show_status(state, "Please enter a lobby name.");
        } else {
            network_open_lobby(state->lobby_name.c_str(), (NetworkLobbyPrivacy)state->lobby_privacy);
            menu_set_mode(state, MENU_MODE_CONNECTING);
        }
    }
}

#ifdef GOLD_STEAM

void menu_ui_lobby_privacy(MenuState* state) {
    if (network_get_backend() != NETWORK_BACKEND_STEAM) {
        return;
    }
    const SpriteInfo& dropdown_info = render_get_sprite_info(SPRITE_UI_DROPDOWN);

    ui_begin_row(state->ui_context, ivec2(0, 0), 0);
        ui_element_position(state->ui_context, ivec2(0, 3));
        ui_text(state->ui_context, FONT_WESTERN8_GOLD, "Privacy:");

        ui_element_position(state->ui_context, ivec2(LOBBY_CREATE_RECT.w - 16 - dropdown_info.frame_width, 0));
        ui_dropdown(state->ui_context, UI_DROPDOWN, &state->lobby_privacy, LOBBY_TYPE_STRS, false);
    ui_end_container(state->ui_context);
}

#else

void menu_ui_lobby_privacy(MenuState* /*state*/) {}

#endif
