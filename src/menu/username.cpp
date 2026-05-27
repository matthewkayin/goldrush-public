#include "menu.h"

#ifndef GOLD_STEAM

#include "network/network.h"

static const Rect USERNAME_RECT = {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 84,
    .w = 300,
    .h = 96
};

void menu_username_prompt_if_empty(MenuState* state) {
    if (!network_is_username_set()) {
        menu_set_mode(state, MENU_MODE_USERNAME);
    }
}

void menu_username_button(MenuState* state) {
    const SpriteInfo& profile_button_sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_PROFILE);
    ui_element_position(state->ui_context, ivec2(SCREEN_WIDTH - profile_button_sprite_info.frame_width - 2, 2));
    if (ui_sprite_button(state->ui_context, SPRITE_UI_BUTTON_PROFILE, false, false)) {
        menu_set_mode(state, MENU_MODE_USERNAME);
    }
}

void menu_username_dialog(MenuState* state) {
    ui_screen_shade(state->ui_context);
    state->ui_context.input_enabled = true;

    ui_frame_rect(state->ui_context, USERNAME_RECT);
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Choose Your Username");

    ui_element_position(state->ui_context, ivec2(USERNAME_RECT.x + (USERNAME_RECT.w / 2) - (header_text_size.x / 2), USERNAME_RECT.y + 6));
    ui_text(state->ui_context, FONT_HACK_GOLD, "Choose Your Username");

    ui_begin_column(state->ui_context, ivec2(USERNAME_RECT.x + 8, USERNAME_RECT.y + 32), 6);
    ui_text_input(state->ui_context, "Username: ", ivec2(284, 24), &state->username, NETWORK_LOBBY_NAME_BUFFER_SIZE - 1);

    ui_end_container(state->ui_context);

    if (network_is_username_set()) {
        ui_element_position(state->ui_context, ui_button_position_frame_bottom_left(USERNAME_RECT));
        if (ui_button(state->ui_context, "Back")) {
            menu_set_mode(state, MENU_MODE_MAIN);
        }
    }
    ui_element_position(state->ui_context, ui_button_position_frame_bottom_right(USERNAME_RECT, "Save"));
    if (ui_button(state->ui_context, "Save")) {
        if (state->username.length() == 0) {
            menu_show_status(state, "Please enter a username.");
        } else {
            network_set_username(state->username.c_str());
            menu_set_mode(state, MENU_MODE_MAIN);
        }
    }
}

#else

void menu_username_prompt_if_empty(MenuState* state) {}
void menu_username_button(MenuState* state) {}
void menu_username_dialog(MenuState* state) {}

#endif
