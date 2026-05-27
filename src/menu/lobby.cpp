#include "menu.h"

#include "core/sound.h"
#include "core/input.h"
#include "network/network.h"

static const Rect PLAYERLIST_RECT = {
    .x = 16, .y = 4, .w = 432, .h = 128
};

static const Rect LOBBY_CHAT_RECT = {
    .x = 16, .y = 136, .w = 432, .h = 158
};

static const Rect MATCH_SETTINGS_RECT = {
    .x = PLAYERLIST_RECT.x + PLAYERLIST_RECT.w + 2,
    .y = PLAYERLIST_RECT.y,
    .w = 172,
    .h = PLAYERLIST_RECT.h
};

static const int PLAYERLIST_COLUMN_X = PLAYERLIST_RECT.x + 16;
static const int PLAYERLIST_COLUMN_Y = PLAYERLIST_RECT.y + 22;
static const int PLAYERLIST_COLUMN_NAME_WIDTH = 200;
static const int PLAYERLIST_COLUMN_STATUS_WIDTH = 64;
static const int PLAYERLIST_COLUMN_TEAM_WIDTH = 56;
static const int PLAYERLIST_ROW_HEIGHT = 20;

static const std::vector<std::string> PLAYER_COLOR_STRS = { "Blue", "Red", "Green", "Purple" };

static const uint32_t MENU_CHAT_MAX_LINE_LENGTH = 64U;
static const uint32_t MENU_CHAT_MAX_LINE_COUNT = 11U;
static const uint32_t MENU_CHAT_MESSAGE_BUFFER_SIZE = 60U;
static const uint32_t MENU_CHAT_MAX_MESSAGE_LENGTH = MENU_CHAT_MESSAGE_BUFFER_SIZE - 1U;

void menu_lobby_update(MenuState* state) {
    menu_lobby_player_list_update(state);
    menu_lobby_match_settings_update(state);
    menu_lobby_chat_update(state);

    // We don't need to worry about these buttons
    // being pressed during LOAD_MATCH_COUNTDOWN
    // because the input will be disabled on the UiContext

    // Lobby buttons
    ui_begin_row(state->ui_context, ivec2(
        LOBBY_CHAT_RECT.x + LOBBY_CHAT_RECT.w + 12,
        MATCH_SETTINGS_RECT.y + MATCH_SETTINGS_RECT.h + 4
    ), 4);
        if (ui_button(state->ui_context, "Back")) {
            network_disconnect();
            menu_set_mode(state, state->mode == MENU_MODE_LOBBY
                ? MENU_MODE_LOBBYLIST
                : MENU_MODE_SINGLEPLAYER);
        }
        if (network_is_host()) {
            if (ui_button(state->ui_context, "Start")) {
                // Validate lobby is ready
                const char* error_message;
                if (!menu_lobby_is_ready(&error_message)) {
                    char chat_message[128];
                    sprintf(chat_message, "Cannot start match: %s", error_message);
                    menu_lobby_add_chat_message(state, chat_message);
                }

                // Start match
                menu_start_match(state);
            }
        } else {
            if (ui_button(state->ui_context, "Ready")) {
                // Toggle player ready status
                bool is_ready = network_get_player(network_get_player_id()).status == NETWORK_PLAYER_STATUS_READY;
                network_set_player_ready(!is_ready);
            }
        }
    ui_end_container(state->ui_context);
}


void menu_lobby_player_list_update(MenuState* state) {
    // Playerlist
    ui_frame_rect(state->ui_context, PLAYERLIST_RECT);
    ivec2 lobby_name_text_size = render_get_text_size(FONT_HACK_GOLD, network_get_lobby_name());
    ui_element_position(state->ui_context, ivec2(PLAYERLIST_RECT.x + (PLAYERLIST_RECT.w / 2) - (lobby_name_text_size.x / 2), PLAYERLIST_RECT.y + 6));
    ui_text(state->ui_context, FONT_HACK_GOLD, network_get_lobby_name());

    ui_begin_column(state->ui_context, ivec2(PLAYERLIST_COLUMN_X, PLAYERLIST_COLUMN_Y), 0);
        // Header row
        ui_element_size(state->ui_context, ivec2(0, PLAYERLIST_ROW_HEIGHT));
        ui_begin_row(state->ui_context, ivec2(0, 0), 0);
            ui_element_size(state->ui_context, ivec2(PLAYERLIST_COLUMN_NAME_WIDTH, 0));
            ui_text(state->ui_context, FONT_HACK_GOLD, "Name");

            ui_element_size(state->ui_context, ivec2(PLAYERLIST_COLUMN_STATUS_WIDTH, 0));
            ui_text(state->ui_context, FONT_HACK_GOLD, "Status");

            ui_element_size(state->ui_context, ivec2(PLAYERLIST_COLUMN_TEAM_WIDTH, 0));
            ui_text(state->ui_context, FONT_HACK_GOLD, "Team");

            ui_text(state->ui_context, FONT_HACK_GOLD, "Color");
        ui_end_container(state->ui_context);

        // Player rows
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const NetworkPlayer& player = network_get_player(player_id);
            if (player.status == NETWORK_PLAYER_STATUS_NONE) {
                continue;
            }

            ui_element_size(state->ui_context, ivec2(0, PLAYERLIST_ROW_HEIGHT));
            ui_begin_row(state->ui_context, ivec2(0, 0), 0);
                ui_element_size(state->ui_context, ivec2(PLAYERLIST_COLUMN_NAME_WIDTH, 0));
                ui_begin_row(state->ui_context, ivec2(0, 0), 2);
                    ui_element_position(state->ui_context, ivec2(0, 1));
                    ui_text(state->ui_context, FONT_HACK_GOLD, player.name);

                    if (network_is_host() && player.status == NETWORK_PLAYER_STATUS_BOT) {
                        ivec2 text_size = render_get_text_size(FONT_HACK_GOLD, player.name);
                        ui_element_position(state->ui_context, ivec2(text_size.x + 2, 0));
                        if (ui_team_picker(state->ui_context, 'X', false)) {
                            network_remove_bot(player_id);
                        }
                    }
                ui_end_container(state->ui_context);

                ui_element_size(state->ui_context, ivec2(PLAYERLIST_COLUMN_STATUS_WIDTH, 0));
                ui_element_position(state->ui_context, ivec2(0, 1));
                ui_text(state->ui_context, FONT_HACK_GOLD, menu_lobby_get_player_status_string((NetworkPlayerStatus)player.status));

                bool teams_enabled = network_get_match_setting(MATCH_SETTING_TEAMS) == TEAMS_ENABLED;
                char team_picker_char;
                if (teams_enabled) {
                    team_picker_char = (char)('1' + (int)player.team);
                } else {
                    team_picker_char = '-';
                }
                ui_element_size(state->ui_context, ivec2(PLAYERLIST_COLUMN_TEAM_WIDTH, 0));
                bool team_picker_enabled = teams_enabled &&
                                            (player_id == network_get_player_id() ||
                                            (network_is_host() && player.status == NETWORK_PLAYER_STATUS_BOT));
                if (ui_team_picker(state->ui_context, team_picker_char, !team_picker_enabled)) {
                    network_set_player_team(player_id, player.team == 0 ? 1 : 0);
                }

                uint32_t player_color = player.recolor_id;
                bool color_picker_enabled = player_id == network_get_player_id() ||
                                            (network_is_host() && player.status == NETWORK_PLAYER_STATUS_BOT);
                if (ui_dropdown(state->ui_context, UI_DROPDOWN_MINI, &player_color, PLAYER_COLOR_STRS, !color_picker_enabled)) {
                    network_set_player_color(player_id, (uint8_t)player_color);
                }
            ui_end_container(state->ui_context);
        }

        // Add bot button
        if (network_is_host() && network_get_player_count() < MAX_PLAYERS) {
            if (ui_slim_button(state->ui_context, "+ Add Bot")) {
                network_add_bot();
            }
        }
    ui_end_container(state->ui_context);
}

const char* menu_lobby_get_player_status_string(NetworkPlayerStatus status) {
    switch (status) {
        case NETWORK_PLAYER_STATUS_READY:
            return "READY";
        case NETWORK_PLAYER_STATUS_NOT_READY:
            return "NOT READY";
        case NETWORK_PLAYER_STATUS_HOST:
            return "HOST";
        case NETWORK_PLAYER_STATUS_BOT:
            return "BOT";
        default:
            GOLD_ASSERT(false);
            return "";
    }
}

void menu_lobby_match_settings_update(MenuState* state) {
    // Match settings
    ui_frame_rect(state->ui_context, MATCH_SETTINGS_RECT);
    ivec2 match_settings_text_size = render_get_text_size(FONT_HACK_GOLD, "Settings");
    ui_element_position(state->ui_context, ivec2(MATCH_SETTINGS_RECT.x + (MATCH_SETTINGS_RECT.w / 2) - (match_settings_text_size.x / 2), MATCH_SETTINGS_RECT.y + 6));
    ui_text(state->ui_context, FONT_HACK_GOLD, "Settings");

    ui_begin_column(state->ui_context, ivec2(MATCH_SETTINGS_RECT.x + 8, MATCH_SETTINGS_RECT.y + 22), 4);
        int dropdown_width = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI).frame_width;
        const int setting_name_element_size = MATCH_SETTINGS_RECT.w - 16 - dropdown_width;
        for (uint8_t index = 0; index < MATCH_SETTING_COUNT; index++) {
            const MatchSettingData& setting_data = match_setting_data((MatchSetting)index);
            ui_begin_row(state->ui_context, ivec2(0, 0), 0);
                ui_element_size(state->ui_context, ivec2(setting_name_element_size, 0));
                ui_element_position(state->ui_context, ivec2(0, 2));
                ui_text(state->ui_context, FONT_HACK_GOLD, setting_data.name);

                uint32_t match_setting_value = network_get_match_setting(index);
                if (ui_dropdown(state->ui_context, UI_DROPDOWN_MINI, &match_setting_value, setting_data.values, !network_is_host())) {
                    network_set_match_setting(index, (uint8_t)match_setting_value);
                }
            ui_end_container(state->ui_context);
        }
    ui_end_container(state->ui_context);
}

void menu_lobby_chat_update(MenuState* state) {
    // Chat
    ui_frame_rect(state->ui_context, LOBBY_CHAT_RECT);
    // Chat messages
    ui_begin_column(state->ui_context, ivec2(LOBBY_CHAT_RECT.x + 16, LOBBY_CHAT_RECT.y + 8), 0);
        for (const std::string& message : state->chat) {
            ui_text(state->ui_context, FONT_HACK_GOLD, message.c_str());
        }
    ui_end_container(state->ui_context);

    // Chat input
    ui_element_position(state->ui_context, ivec2(24, 298));
    ui_text_input(state->ui_context, "Chat: ", ivec2(416, 24), &state->chat_message, MENU_CHAT_MAX_MESSAGE_LENGTH);
    if (input_is_action_just_pressed(INPUT_ACTION_ENTER) && input_is_text_input_active()) {
        if (!state->chat_message.empty()) {
            char chat_message[128];
            sprintf(chat_message, "%s: %s", network_get_player(network_get_player_id()).name, state->chat_message.c_str());
            menu_lobby_add_chat_message(state, chat_message);
            network_send_chat(chat_message);
            state->chat_message = "";
        }
        sound_play(SOUND_UI_CLICK);
    }
}

std::vector<std::string> menu_lobby_split_chat_message(std::string message) {
    std::vector<std::string> words;
    while (message.length() != 0) {
        size_t space_index = message.find(' ');
        if (space_index == std::string::npos) {
            words.push_back(message);
            message = "";
        } else {
            words.push_back(message.substr(0, space_index));
            message = message.substr(space_index + 1);
        }
    }

    std::vector<std::string> lines;
    std::string line;
    while (!words.empty()) {
        std::string next = words[0];
        words.erase(words.begin());

        if (next.length() + line.length() > MENU_CHAT_MAX_LINE_LENGTH) {
            lines.push_back(line);
            line = "";
        } else if (!line.empty()) {
            line += " ";
        }
        line += next;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }

    return lines;
}

void menu_lobby_add_chat_message(MenuState* state, const char* message) {
    std::vector<std::string> lines = menu_lobby_split_chat_message(std::string(message));
    while (!lines.empty()) {
        if (state->chat.size() == MENU_CHAT_MAX_LINE_COUNT) {
            state->chat.erase(state->chat.begin());
        }
        state->chat.push_back(lines[0]);
        lines.erase(lines.begin());
    }
}

bool menu_lobby_is_ready(const char** error_message) {
    // Make sure all players are ready
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NOT_READY) {
            *error_message = "Some players are not ready.";
            return false;
        }
    }

    // Make sure all players have distinct colors
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const NetworkPlayer& player = network_get_player(player_id);
        if (player.status == NETWORK_PLAYER_STATUS_NONE) {
            continue;
        }
        for (uint8_t other_id = 0; other_id < MAX_PLAYERS; other_id++) {
            const NetworkPlayer& other_player = network_get_player(other_id);
            if (other_player.status == NETWORK_PLAYER_STATUS_NONE || player_id == other_id) {
                continue;
            }
            if (player.recolor_id == other_player.recolor_id) {
                *error_message = "Some players have selected the same color.";
                return false;
            }
        }
    }

    return true;
}
