#include "menu.h"

#include "core/logger.h"
#include "core/cursor.h"
#include "core/input.h"
#include "core/sound.h"
#include "menu/item_list.h"
#include "menu/types.h"
#include "network/network.h"
#include "network/types.h"

static const uint32_t STATUS_DURATION = 2 * 60;
static const uint32_t CONNECTION_TIMEOUT = 5 * 60;
static const uint32_t MATCH_LOAD_COUNTDOWN_DURATION = 3U * 60U;
static const uint32_t MUSIC_BEGIN_DELAY = 15U;

MenuState* menu_init() {
    MenuState* state = new MenuState();

    state->menu_animation = menu_animation_init();
    state->ui_context = ui_init();
    state->options_menu.mode = OPTIONS_MENU_CLOSED;

    state->connection_timeout = 0U;
    state->match_load_countdown_timer = 0U;

    state->item_list = menu_item_list_init("");

    menu_set_mode(state, MENU_MODE_MAIN);
    menu_username_prompt_if_empty(state);

    state->music_begin_timer = MUSIC_BEGIN_DELAY;
    state->campaign_road_reveal_sound_track_index = SOUND_NOT_PLAYING;

    return state;
}

void menu_free(MenuState* state) {
    delete state;
}

void menu_handle_network_event(MenuState* state, NetworkEvent event) {
    switch (event.type) {
        case NETWORK_EVENT_LOBBY_FOUND: {
            if (state->mode == MENU_MODE_LOBBYLIST) {
                menu_lobby_list_on_lobby_found(state, event.lobby_found.lobby);
            }
            break;
        }
        case NETWORK_EVENT_LOBBY_CONNECTION_FAILED: {
            log_info("Menu received LOBBY_CONNECTION_FAILED.");
            if (state->mode == MENU_MODE_SKIRMISH_CONNECTING) {
                menu_set_mode(state, MENU_MODE_SINGLEPLAYER);
            } else {
                menu_set_mode(state, MENU_MODE_LOBBYLIST);
            }
            menu_show_status(state, "Connection failed.");
            break;
        }
        case NETWORK_EVENT_LOBBY_INVALID_VERSION: {
            log_info("Menu received LOBBY_INVALID_VERSION.");
            menu_set_mode(state, MENU_MODE_LOBBYLIST);
            menu_show_status(state, "Game version does not match server.");
            network_disconnect();
            break;
        }
        case NETWORK_EVENT_LOBBY_FULL: {
            log_info("Menu received LOBBY_FULL");
            menu_set_mode(state, MENU_MODE_LOBBYLIST);
            menu_show_status(state, "Lobby is full.");
            network_disconnect();
            break;
        }
        case NETWORK_EVENT_LOBBY_CONNECTED: {
            log_info("Menu received LOBBY_CONNECTED.");
            if (state->mode == MENU_MODE_SKIRMISH_CONNECTING) {
                menu_set_mode(state, MENU_MODE_SKIRMISH_LOBBY);
            } else {
                menu_set_mode(state, MENU_MODE_LOBBY);
            }

            // Send lobby chat
            menu_add_lobby_creation_chat_message(state);

            break;
        }
        case NETWORK_EVENT_CHAT: {
            if (state->mode == MENU_MODE_LOBBY) {
                menu_lobby_add_chat_message(state, event.chat.message);
            }
            return;
        }
        case NETWORK_EVENT_PLAYER_DISCONNECTED: {
            if (event.player_disconnected.player_id == 0) {
                network_disconnect();
                menu_set_mode(state, MENU_MODE_LOBBYLIST);
                menu_show_status(state, "The host closed the lobby.");
            } else {
                char message[128];
                sprintf(message, "%s left the lobby.", network_get_player(event.player_disconnected.player_id).name);
                menu_lobby_add_chat_message(state, message);
            }
            return;
        }
        case NETWORK_EVENT_PLAYER_CONNECTED: {
            char message[128];
            sprintf(message, "%s joined the lobby.", network_get_player(event.player_connected.player_id).name);
            menu_lobby_add_chat_message(state, message);
            network_send_chat(message);
            return;
        }
        case NETWORK_EVENT_MATCH_LOAD_COUNTDOWN: {
            menu_set_mode(state, MENU_MODE_LOAD_MATCH_COUNTDOWN);
            return;
        }
    #ifdef GOLD_STEAM
        case NETWORK_EVENT_STEAM_INVITE: {
            log_info("Menu received steam invite invite");
            if (state->mode == MENU_MODE_LOBBY) {
                log_info("Ignoring steam invite because we're in a lobby already.");
                return;
            }

            log_info("Connecting to Steam lobby.");
            network_disconnect();
            network_set_backend(NETWORK_BACKEND_STEAM);
            network_join_lobby(event.steam_invite.connection_info);
            menu_set_mode(state, MENU_MODE_CONNECTING);
            return;
        }
    #endif
        default:
            return;
    }
}

void menu_update(MenuState* state) {
    // Set cursor
    cursor_set(CURSOR_DEFAULT);

    // Update animation
    menu_animation_update(state->menu_animation, state->mode);

    // Music begin timer
    if (state->music_begin_timer > 0) {
        state->music_begin_timer--;
        if (state->music_begin_timer == 0) {
            sound_play_music(RESOURCE_MUSIC_MENU, MUSIC_OPTION_FADE_IN | MUSIC_OPTION_LOOP);
        }
    }

    // Status timer
    if (state->status_timer > 0) {
        state->status_timer--;
    }

    // Connection timeout
    if (state->mode == MENU_MODE_CONNECTING && state->connection_timeout > 0) {
        state->connection_timeout--;
        if (state->connection_timeout == 0) {
            network_disconnect();
            menu_set_mode(state, MENU_MODE_LOBBYLIST);
            menu_show_status(state, "Connection timed out.");
        }
    }

    // Match load countdown
    if (state->match_load_countdown_timer != 0 && (state->mode == MENU_MODE_LOAD_MATCH_COUNTDOWN || state->mode == MENU_MODE_LOAD_SCENARIO_COUNTDOWN)) {
        state->match_load_countdown_timer--;
        if (state->match_load_countdown_timer == 0 && state->mode == MENU_MODE_LOAD_MATCH_COUNTDOWN && network_is_host()) {
            menu_set_mode(state, MENU_MODE_LOAD_MATCH);
        }
        if (state->match_load_countdown_timer == 0 && state->mode == MENU_MODE_LOAD_SCENARIO_COUNTDOWN) {
            menu_set_mode(state, MENU_MODE_LOAD_SCENARIO);
        }
    }

    // UI
    ui_begin(state->ui_context);
    state->ui_context.input_enabled = !(
        menu_is_in_submenu(state) ||
        state->mode == MENU_MODE_LOAD_MATCH_COUNTDOWN
    );

    switch (state->mode) {
        case MENU_MODE_MAIN:
        case MENU_MODE_USERNAME:
        case MENU_MODE_OPTIONS: {
            ui_begin_column(state->ui_context, ivec2(BUTTON_X, BUTTON_Y), 4);
                if (ui_button(state->ui_context, "Single Player")) {
                    menu_set_mode(state, MENU_MODE_SINGLEPLAYER);
                }
                if (ui_button(state->ui_context, "Multiplayer")) {
                    menu_on_multiplayer_button_pressed(state);
                }
                if (ui_button(state->ui_context, "Replays")) {
                    menu_set_mode(state, MENU_MODE_REPLAYS);
                }
                if (ui_button(state->ui_context, "Options")) {
                    state->options_menu = options_menu_open();
                    menu_set_mode(state, MENU_MODE_OPTIONS);
                }
                if (ui_button(state->ui_context, "Exit")) {
                    menu_set_mode(state, MENU_MODE_EXIT);
                }
            ui_end_container(state->ui_context);

            // Username dialog
            menu_username_button(state);
            if (state->mode == MENU_MODE_USERNAME) {
                menu_username_dialog(state);
            }
            // Options sub-menu
            if (state->mode == MENU_MODE_OPTIONS) {
                options_menu_update(state->options_menu, state->ui_context);
                if (state->options_menu.mode == OPTIONS_MENU_CLOSED) {
                    menu_set_mode(state, MENU_MODE_MAIN);
                }
            }

            break;
        }
        case MENU_MODE_SINGLEPLAYER: {
            ui_begin_column(state->ui_context, ivec2(BUTTON_X, BUTTON_Y), 2);
                if (ui_button(state->ui_context, "Campaign")) {
                    menu_set_mode(state, MENU_MODE_CAMPAIGN_LIST);
                }
                if (ui_button(state->ui_context, "Skirmish")) {
                    if (network_get_status() != NETWORK_STATUS_OFFLINE) {
                        menu_show_status(state, "Error setting up game. Please try again.");
                        network_disconnect();
                    } else {
                        network_set_backend(NETWORK_BACKEND_LAN);
                        network_open_lobby("Skirmish", NETWORK_LOBBY_PRIVACY_SINGLEPLAYER);
                        menu_set_mode(state, MENU_MODE_SKIRMISH_CONNECTING);
                    }
                }
                if (ui_button(state->ui_context, "Back")) {
                    menu_set_mode(state, MENU_MODE_MAIN);
                }
            ui_end_container(state->ui_context);
            break;
        }
        case MENU_MODE_MULTIPLAYER: {
            ui_begin_column(state->ui_context, ivec2(BUTTON_X, BUTTON_Y), 2);
                if (ui_button(state->ui_context, "Online")) {
                    menu_set_mode_lobbylist_steam(state);
                }
                if (ui_button(state->ui_context, "Local Network")) {
                    menu_set_mode_lobbylist(state, NETWORK_BACKEND_LAN);
                }
                if (ui_button(state->ui_context, "Back")) {
                    menu_set_mode(state, MENU_MODE_MAIN);
                }
            ui_end_container(state->ui_context);
            break;
        }
        case MENU_MODE_LOBBYLIST:
        case MENU_MODE_CREATE_LOBBY: {
            menu_lobby_list_update(state);
            break;
        }
        case MENU_MODE_REPLAYS:
        case MENU_MODE_REPLAY_RENAME:
        case MENU_MODE_REPLAY_CONFIRM_CLEAR: {
            menu_replay_list_update(state);
            break;
        }
        case MENU_MODE_LOBBY:
        case MENU_MODE_SKIRMISH_LOBBY:
        case MENU_MODE_LOAD_MATCH_COUNTDOWN: {
            menu_lobby_update(state);
            break;
        }
        case MENU_MODE_CAMPAIGN_LIST:
        case MENU_MODE_CAMPAIGN_LIST_NEW:
        case MENU_MODE_CAMPAIGN_LIST_RENAME:
        case MENU_MODE_CAMPAIGN_LIST_DELETE: {
            menu_campaign_list_update(state);
            break;
        }
        case MENU_MODE_CAMPAIGN:
        case MENU_MODE_LOAD_SCENARIO_COUNTDOWN: {
            menu_campaign_update(state);
            break;
        }
        case MENU_MODE_CREDITS: {
            menu_credits_update(state);
            break;
        }
        default: {
            break;
        }
    }
}

void menu_render(const MenuState* state) {
    menu_animation_render_background(state->menu_animation);

    // Credits
    if (state->mode == MENU_MODE_CREDITS) {
        menu_credits_render(state);
    }

    menu_animation_render(state->menu_animation, state->mode);

    // Render title
    if (state->mode == MENU_MODE_MAIN ||
            state->mode == MENU_MODE_MULTIPLAYER ||
            state->mode == MENU_MODE_USERNAME ||
            state->mode == MENU_MODE_SINGLEPLAYER) {
        render_sprite_frame(SPRITE_UI_TITLE, ivec2(0, 0), ivec2(21, 21), RENDER_SPRITE_NO_CULL, 0);
    }

    // Render version
    {
        char version_text[32];
        sprintf(version_text, "Version %s", APP_VERSION);
        ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, version_text);
        render_text(FONT_WESTERN8_OFFBLACK, version_text, ivec2(2, SCREEN_HEIGHT - text_size.y - 2));
    }

    // Campaign map
    if (state->mode == MENU_MODE_CAMPAIGN || state->mode == MENU_MODE_LOAD_SCENARIO_COUNTDOWN) {
        menu_campaign_render(state);
    }

    // UI render
    ui_render(state->ui_context);

    // Status text
    if (state->status_timer != 0) {
        ivec2 text_size = render_get_text_size(FONT_HACK_OFFBLACK, state->status_text);
        int rect_width = text_size.x + 32;
        render_ninepatch(SPRITE_UI_FRAME, { .x = (SCREEN_WIDTH / 2) - (rect_width / 2), .y = 80, .w = rect_width, .h = 32 });
        render_text(FONT_HACK_GOLD, state->status_text, ivec2((SCREEN_WIDTH / 2) - (text_size.x / 2), 80 + 16 - (text_size.y / 2)));
    }
    if (state->mode == MENU_MODE_CONNECTING) {
        ivec2 text_size = render_get_text_size(FONT_HACK_OFFBLACK, "Connecting...");
        int rect_width = text_size.x + 32;
        render_ninepatch(SPRITE_UI_FRAME, { .x = (SCREEN_WIDTH / 2) - (rect_width / 2), .y = 80, .w = rect_width, .h = 32 });
        render_text(FONT_HACK_GOLD, "Connecting...", ivec2((SCREEN_WIDTH / 2) - (text_size.x / 2), 80 + 16 - (text_size.y / 2)));
    }
}

// INTERNAL

void menu_set_mode(MenuState* state, MenuMode mode) {
    state->mode = mode;
    state->status_timer = 0;

    if (input_is_text_input_active()) {
        input_stop_text_input();
    }

    const bool should_play_music = !(
        mode == MENU_MODE_LOAD_MATCH_COUNTDOWN ||
        mode == MENU_MODE_LOAD_SCENARIO_COUNTDOWN ||
        mode == MENU_MODE_LOAD_MATCH ||
        mode == MENU_MODE_LOAD_REPLAY ||
        mode == MENU_MODE_LOAD_SCENARIO);
    if (should_play_music && !sound_is_music_playing()) {
        state->music_begin_timer = MUSIC_BEGIN_DELAY;
    }

    switch (state->mode) {
        case MENU_MODE_LOBBYLIST: {
            menu_lobby_list_init(state);
            break;
        }
        case MENU_MODE_REPLAYS: {
            menu_replay_list_init(state);
            break;
        }
        case MENU_MODE_CAMPAIGN_LIST: {
            menu_campaign_list_init(state);
            break;
        }
        case MENU_MODE_CAMPAIGN: {
            menu_campaign_init(state);
            break;
        }
        case MENU_MODE_CONNECTING:
        case MENU_MODE_SKIRMISH_CONNECTING: {
            state->connection_timeout = CONNECTION_TIMEOUT;
            break;
        }
        case MENU_MODE_LOBBY:
        case MENU_MODE_SKIRMISH_LOBBY: {
            state->chat.clear();
            break;
        }
        case MENU_MODE_CREATE_LOBBY: {
            state->lobby_name = std::string(network_get_username()) + "'s Game";
            break;
        }
        case MENU_MODE_LOAD_MATCH_COUNTDOWN:
        case MENU_MODE_LOAD_SCENARIO_COUNTDOWN: {
            sound_stop_music();
            if (state->mode == MENU_MODE_LOAD_MATCH_COUNTDOWN) {
                menu_lobby_add_chat_message(state, "The standoff is about to start...");
            }
            sound_play(SOUND_MATCH_START);
            state->match_load_countdown_timer = MATCH_LOAD_COUNTDOWN_DURATION;
            break;
        }
        case MENU_MODE_LOAD_MATCH: {
            sound_stop_music();
            break;
        }
        case MENU_MODE_USERNAME: {
            state->username = std::string(network_get_username());
            break;
        }
        case MENU_MODE_OPTIONS: {
            state->options_menu = options_menu_open();
            break;
        }
        case MENU_MODE_CREDITS: {
            menu_credits_init(state);
            break;
        }
        default:
            break;
    }
}

void menu_set_mode_lobbylist(MenuState* state, NetworkBackend backend) {
    if (network_get_status() != NETWORK_STATUS_OFFLINE) {
        menu_show_status(state, "Error setting up connection. Please try again.");
        network_disconnect();
    } else {
        network_set_backend(backend);
        menu_set_mode(state, MENU_MODE_LOBBYLIST);
    }
}

void menu_show_status(MenuState* state, const char* message) {
    strcpy(state->status_text, message);
    state->status_timer = STATUS_DURATION;
}

bool menu_mode_is_replay_list(MenuMode mode) {
    return mode == MENU_MODE_REPLAYS ||
        mode == MENU_MODE_REPLAY_RENAME ||
        mode == MENU_MODE_REPLAY_CONFIRM_CLEAR;
}

const char* menu_get_selected_replay_filename(const MenuState* state) {
    if (state->item_list.item_selected == MENU_ITEM_NONE ||
            state->item_list.item_selected >= state->item_list.items.size()) {
        log_warn("Called menu_get_selected_replay_filename() when no valid filename is selected.");
        return "";
    }
    return state->item_list.items[state->item_list.item_selected].c_str();
}

bool menu_is_in_submenu(const MenuState* state) {
    return state->mode == MENU_MODE_OPTIONS ||
        state->mode == MENU_MODE_REPLAY_RENAME ||
        state->mode == MENU_MODE_REPLAY_CONFIRM_CLEAR ||
        state->mode == MENU_MODE_LOAD_MATCH_COUNTDOWN ||
        state->mode == MENU_MODE_USERNAME ||
        state->mode == MENU_MODE_CREATE_LOBBY ||
        state->mode == MENU_MODE_CAMPAIGN_LIST_NEW ||
        state->mode == MENU_MODE_CAMPAIGN_LIST_RENAME ||
        state->mode == MENU_MODE_CAMPAIGN_LIST_DELETE;
}

// STEAM vs NON-STEAM

#ifdef GOLD_STEAM

void menu_prompt_username(MenuState* state) {}

void menu_ui_profile_button(MenuState* /*state*/) {}

void menu_on_multiplayer_button_pressed(MenuState* state) {
    menu_set_mode(state, MENU_MODE_MULTIPLAYER);
}

void menu_set_mode_lobbylist_steam(MenuState* state) {
    menu_set_mode_lobbylist(state, NETWORK_BACKEND_STEAM);
}

void menu_add_lobby_creation_chat_message(MenuState* state) {
    if (network_is_host() && network_get_backend() == NETWORK_BACKEND_STEAM) {
        menu_add_chat_message(state, "You have created a lobby. You can invite your friends using the Steam overlay (SHIFT+TAB).");
    }
}

#else

void menu_on_multiplayer_button_pressed(MenuState* state) {
    menu_set_mode_lobbylist(state, NETWORK_BACKEND_LAN);
}

void menu_set_mode_lobbylist_steam(MenuState* /*state*/) {}
void menu_add_lobby_creation_chat_message(MenuState* /*state*/) { }

#endif

// DEBUG vs RELEASE

#ifdef GOLD_DEBUG

void menu_start_match(MenuState* state) {
    menu_set_mode(state, MENU_MODE_LOAD_MATCH);
}

#else

void menu_start_match(MenuState* state) {
    network_begin_load_match_countdown();
    menu_set_mode(state, MENU_MODE_LOAD_MATCH_COUNTDOWN);
}

#endif
