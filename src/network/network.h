#pragma once

#include "types.h"

#ifdef GOLD_STEAM
    #include <steam/steam_api.h>
#endif

// Init
bool network_init();
void network_quit();

NetworkBackend network_get_backend();
void network_set_backend(NetworkBackend backend);

const char* network_get_username();
void network_set_username(const char* value);
bool network_is_username_set();

NetworkStatus network_get_status();
bool network_is_host();
const NetworkPlayer& network_get_player(uint8_t player_id);
uint8_t network_get_player_id();
uint8_t network_get_player_count();

// Service

void network_service();
bool network_poll_events(NetworkEvent* event);
void network_cleanup_event(NetworkEvent* event);
void network_disconnect();

// Lobbies

void network_search_lobbies(const char* query);
void network_open_lobby(const char* lobby_name, NetworkLobbyPrivacy privacy);
void network_join_lobby(const NetworkConnectionInfo& connection_info);
const char* network_get_lobby_name();

#ifdef GOLD_STEAM
void network_steam_accept_invite(CSteamID lobby_id);
#endif

// Messages

void network_send_chat(const char* message);
void network_set_player_ready(bool ready);
void network_set_player_color(uint8_t player_id, uint8_t color);
void network_set_match_setting(uint8_t setting, uint8_t value);
uint8_t network_get_match_setting(uint8_t setting);
void network_set_player_team(uint8_t player_id, uint8_t team);
void network_add_bot();
void network_remove_bot(uint8_t player_id);
void network_begin_load_match_countdown();
void network_begin_loading_match(const RawMap* raw_map, int32_t lcg_seed);
void network_send_input(uint8_t* out_buffer, size_t out_buffer_length);
void network_send_checksum(uint32_t checksum);
void network_send_serialized_frame(uint8_t* state_buffer, size_t state_buffer_length);
