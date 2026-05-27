#pragma once

#include "match/state/match.h"
#include "match/bot/bot.h"

constexpr size_t DESYNC_BUFFER_SIZE = sizeof(MatchState) + (MAX_PLAYERS * sizeof(Bot));
constexpr size_t DESYNC_STATE_BUFFER_HEADER_SIZE = sizeof(uint8_t) + sizeof(uint32_t);
constexpr size_t DESYNC_STATE_BUFFER_LENGTH = DESYNC_STATE_BUFFER_HEADER_SIZE + DESYNC_BUFFER_SIZE;

bool desync_init(const char* desync_foldername);
void desync_quit();

uint32_t desync_get_checksum_frequency();
void desync_write_frame(uint8_t* data, uint32_t frame);
uint8_t* desync_read_frame(uint32_t frame_number);
void desync_delete_frame(uint32_t frame);
void desync_send_frame(uint32_t frame);
void desync_handle_serialized_frame(uint8_t* incoming_state_buffer);
void desync_compare_frames(uint8_t* state_buffer, uint8_t* state_buffer2);
