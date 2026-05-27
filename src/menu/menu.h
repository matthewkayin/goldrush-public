#pragma once

#include "menu/types.h"
#include "menu/animation.h"
#include "menu/item_list.h"
#include "core/ui.h"
#include "network/types.h"
#include "shared/options_menu.h"
#include <string>

const int BUTTON_X = 44;
const int BUTTON_Y = 128;

#define CAMPAIGN_SAVE_ENTRY_NAME_BUFFER_SIZE 48
static const uint32_t CAMPAIGN_MISSION_COUNT = 12U;

struct CampaignSaveEntry {
    char name[CAMPAIGN_SAVE_ENTRY_NAME_BUFFER_SIZE];
    uint32_t missions_completed;
    uint32_t playtime_seconds;
};

#define CREDITS_ENTRY_BUFFER_SIZE 64

enum CreditsEntryType {
    CREDITS_ENTRY_HEADER,
    CREDITS_ENTRY_TEXT,
    CREDITS_ENTRY_BREAK
};

struct CreditsEntry {
    CreditsEntryType type;
    char text[CREDITS_ENTRY_BUFFER_SIZE];
};

struct MenuState {
    MenuMode mode;
    MenuAnimationState menu_animation;
    OptionsMenuState options_menu;
    UiContext ui_context;

    // Status text
    uint32_t status_timer;
    char status_text[128];

    // Connection timeout
    uint32_t connection_timeout;
    uint32_t match_load_countdown_timer;

    // Item list
    MenuItemList item_list;
    std::string item_list_item_to_rename;

    // Lobby
    std::string lobby_name;
    uint32_t lobby_privacy;
    std::vector<NetworkLobby> lobbies;

    // Chat
    std::string chat_message;
    std::vector<std::string> chat;

    // Username
    std::string username;

    // Music
    uint32_t music_begin_timer;

    // Campaign
    std::vector<CampaignSaveEntry> campaign_saves;
    std::vector<uint32_t> item_list_index_to_campaign_save_index;
    std::string campaign_name;
    uint32_t campaign_road_reveal_timer;
    uint32_t campaign_road_reveal_duration;
    uint32_t campaign_road_reveal_sound_track_index;
    uint32_t campaign_mission_selected;

    // Credits
    std::vector<CreditsEntry> credits_entries;
    uint32_t credits_timer;
    int credits_yoffset;
};

// Main API
MenuState* menu_init();
void menu_free(MenuState* menu_state);
void menu_handle_network_event(MenuState* state, NetworkEvent event);
void menu_update(MenuState* menu_state);
void menu_render(const MenuState* state);

// Internal
void menu_set_mode(MenuState* state, MenuMode mode);
void menu_set_mode_lobbylist(MenuState* state, NetworkBackend backend);
void menu_show_status(MenuState* state, const char* message);
bool menu_mode_is_replay_list(MenuMode mode);
const char* menu_get_selected_replay_filename(const MenuState* state);
bool menu_is_in_submenu(const MenuState* state);

// Steam vs non-steam
void menu_on_multiplayer_button_pressed(MenuState* state);
void menu_set_mode_lobbylist_steam(MenuState* state);
void menu_add_lobby_creation_chat_message(MenuState* state);

// Username
void menu_username_prompt_if_empty(MenuState* state);
void menu_username_button(MenuState* state);
void menu_username_dialog(MenuState* state);

// Debug vs release
void menu_start_match(MenuState* state);

// Create lobby
void menu_ui_create_lobby(MenuState* state);
void menu_ui_lobby_privacy(MenuState* state);

// Lobby list
void menu_lobby_list_init(MenuState* state);
void menu_lobby_list_search(MenuState* state);
void menu_lobby_list_update(MenuState* state);
void menu_lobby_list_on_lobby_found(MenuState* state, NetworkLobby lobby);

// Replay
void menu_replay_list_init(MenuState* state);
void menu_replay_list_search(MenuState* state);
void menu_replay_list_update(MenuState* state);
void menu_replay_list_rename_dialog(MenuState* state);
void menu_replay_list_confirm_dialog(MenuState* state);

// Lobby
void menu_lobby_update(MenuState* state);
void menu_lobby_player_list_update(MenuState* state);
const char* menu_lobby_get_player_status_string(NetworkPlayerStatus status);
void menu_lobby_match_settings_update(MenuState* state);
std::vector<std::string> menu_lobby_split_chat_message(std::string message);
void menu_lobby_chat_update(MenuState* state);
void menu_lobby_add_chat_message(MenuState* state, const char* message);
bool menu_lobby_is_ready(const char** error_message);

// Campaign List
std::string menu_get_campaign_saves_path();
void menu_load_campaign_saves(MenuState* state);
void menu_save_campaign_saves(const MenuState* state);
void menu_campaign_list_init(MenuState* state);
void menu_campaign_list_search(MenuState* state);
void menu_campaign_list_update(MenuState* state);
std::string menu_campaign_list_item_str(const CampaignSaveEntry& entry);
uint32_t menu_campaign_list_get_selected_campaign_save(const MenuState* state);
void menu_campaign_list_new_dialog(MenuState* state);
void menu_campaign_list_rename_dialog(MenuState* state);
void menu_campaign_list_delete_dialog(MenuState* state);

// Campaign
void menu_campaign_init(MenuState* state);
void menu_campaign_update(MenuState* state);
void menu_campaign_on_scenario_finished(MenuState* state, uint32_t playtime_seconds, bool victory);
uint32_t menu_campaign_get_roads_revealed(const MenuState* state);
void menu_campaign_begin_road_reveal(MenuState* state);
bool menu_campaign_is_in_road_reveal_delay(const MenuState* state);
void menu_campaign_render(const MenuState* state);

// Credits
void menu_credits_init(MenuState* state);
void menu_credits_update(MenuState* state);
void menu_credits_render(const MenuState* state);
bool menu_credits_are_over(const MenuState* state);
int menu_credits_header_height();
int menu_credits_text_height();
