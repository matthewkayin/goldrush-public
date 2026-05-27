#include "SDL3/SDL_timer.h"
#include "core/filesystem.h"
#include "shell.h"

static const uint32_t REPLAY_FILE_SIGNATURE = 0x46591214;
static const uint32_t REPLAY_FILE_VERSION = 0;

static const uint32_t REPLAY_CHECKPOINT_FREQ = 32U;

static std::string arg_replay_file;
static bool use_arg_replay_file = false;

// REPLAY FILE

void match_shell_replay_set_filename(const char* filename) {
    arg_replay_file = std::string(filename) + ".rep";
    use_arg_replay_file = true;
}

FILE* match_shell_replay_file_open(MatchPlayer players[MAX_PLAYERS], MapType map_type, const RawMap* raw_map, int lcg_seed) {
    // Determine replay path
    std::string replay_short_path = use_arg_replay_file
        ? arg_replay_file
        : FILESYSTEM_REPLAY_AUTOSAVE_PREFIX + filesystem_get_timestamp_str() + ".rep";
    std::string replay_path = filesystem_get_data_path() + FILESYSTEM_REPLAY_FOLDER_NAME + replay_short_path;

    // Open file
    FILE* file = fopen(replay_path.c_str(), "wb");
    if (file == NULL) {
        log_error("Could not open replay file for writing. Path %s.", replay_path.c_str());
        return NULL;
    }

    // Signature
    fwrite(&REPLAY_FILE_SIGNATURE, 1, sizeof(uint32_t), file);

    // Version byte
    fwrite(&REPLAY_FILE_VERSION, 1, sizeof(uint32_t), file);

    // LCG seed
    fwrite(&lcg_seed, 1, sizeof(int32_t), file);

    // Map Type
    uint8_t map_type_byte = (uint8_t)map_type;
    fwrite(&map_type_byte, 1, sizeof(uint8_t), file);

    // Map
    raw_map_fwrite(raw_map, file);

    // Players
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        fwrite(&players[player_id], 1, sizeof(MatchPlayer), file);
    }

    return file;
}

void match_shell_replay_file_close(FILE* file) {
    if (file == NULL) {
        return;
    }

    fclose(file);
}

void match_shell_replay_file_write_entry(FILE* file, const ReplayEntry& entry) {
    if (file == NULL) {
        return;
    }

    fwrite(&entry.type, 1, sizeof(entry.type), file);

    switch (entry.type) {
        case REPLAY_ENTRY_INPUT: {
            uint8_t out_buffer[NETWORK_INPUT_BUFFER_SIZE];
            size_t out_buffer_length = 0;

            match_input_serialize(out_buffer, out_buffer_length, entry.input);
            fwrite(&out_buffer_length, 1, sizeof(size_t), file);
            fwrite(out_buffer, 1, out_buffer_length, file);

            break;
        }
        case REPLAY_ENTRY_CHAT: {
            fwrite(&entry.chat_message, 1, sizeof(entry.chat_message), file);
            break;
        }
        case REPLAY_ENTRY_DISCONNECT: {
            fwrite(&entry.disconnect_player_id, 1, sizeof(entry.disconnect_player_id), file);
            break;
        }
        case REPLAY_ENTRY_NEW_TURN: {
            break;
        }
    }
}

bool match_shell_replay_file_read(const char* path, MatchState& state, std::vector<std::vector<ReplayEntry>>* replay_entries) {
    std::string replay_path = filesystem_get_data_path() + FILESYSTEM_REPLAY_FOLDER_NAME + path;
    FILE* file = fopen(replay_path.c_str(), "rb");
    if (file == NULL) {
        log_error("Could not open replay file for reading with path %s.", replay_path.c_str());
        return false;
    }

    // Signature
    uint32_t replay_signature;
    fread(&replay_signature, 1, sizeof(uint32_t), file);
    if (replay_signature != REPLAY_FILE_SIGNATURE) {
        log_error("Replay file signature was invalid %s.", replay_path.c_str());
        fclose(file);
        return false;
    }

    // Version
    uint32_t replay_version;
    fread(&replay_version, 1, sizeof(uint32_t), file);
    log_debug("Replay version: %u", replay_version);

    // LCG seed
    int32_t lcg_seed;
    fread(&lcg_seed, 1, sizeof(int32_t), file);

    // Map type
    uint8_t map_type_byte;
    fread(&map_type_byte, 1, sizeof(uint8_t), file);
    MapType map_type = (MapType)map_type_byte;

    // Map
    RawMap* raw_map = raw_map_fread(file);
    if (raw_map == NULL) {
        log_error("Replay could not alloc raw map");
        fclose(file);
        return false;
    }

    // Players
    MatchPlayer players[MAX_PLAYERS];
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        fread(&players[player_id], 1, sizeof(MatchPlayer), file);
    }

    // Init state
    match_init(state, players, map_type, raw_map, lcg_seed);
    match_spawn_players(state, raw_map);

    while (true) {
        uint8_t entry_type;
        size_t bytes_read = fread(&entry_type, 1, sizeof(uint8_t), file);
        if (bytes_read == 0) {
            goto success;
        }

        switch (entry_type) {
            case REPLAY_ENTRY_INPUT: {
                size_t in_buffer_length;
                uint8_t in_buffer[NETWORK_INPUT_BUFFER_SIZE];

                bytes_read = fread(&in_buffer_length, 1, sizeof(size_t), file);
                if (bytes_read != sizeof(size_t)) {
                    log_error("Replay file bytes read mismatch. Read %u Expected %u", bytes_read, sizeof(size_t));
                    goto fail;
                }
                bytes_read = fread(in_buffer, 1, in_buffer_length, file);
                if (bytes_read != in_buffer_length) {
                    log_error("Replay file bytes read mismatch. Read %u Expected %u", bytes_read, in_buffer_length);
                    goto fail;
                }

                size_t in_buffer_head = 0;
                MatchInput input = match_input_deserialize(in_buffer, in_buffer_head);

                if (input.type != MATCH_INPUT_NONE) {
                    char print_buffer[1024];
                    match_input_print(print_buffer, input);
                    log_debug("Replay entry turn %u input %s", (uint32_t)replay_entries->size() - 1U, print_buffer);
                }

                replay_entries->back().push_back((ReplayEntry) {
                    .type = REPLAY_ENTRY_INPUT,
                    .input = input
                });

                break;
            }
            case REPLAY_ENTRY_CHAT: {
                ReplayEntry entry;
                entry.type = REPLAY_ENTRY_CHAT;

                bytes_read = fread(&entry.chat_message, 1, sizeof(entry.chat_message), file);
                if (bytes_read != sizeof(entry.chat_message)) {
                    log_error("Replay file bytes read mismatch. Read %u Expected %u", bytes_read, sizeof(entry.chat_message));
                    goto fail;
                }

                log_debug("Replay entry turn %u chat message %s: %s", (uint32_t)replay_entries->size() - 1U, entry.chat_message.prefix, entry.chat_message.message);

                replay_entries->back().push_back(entry);

                break;
            }
            case REPLAY_ENTRY_DISCONNECT: {
                ReplayEntry entry;
                entry.type = REPLAY_ENTRY_DISCONNECT;

                bytes_read = fread(&entry.disconnect_player_id, 1, sizeof(entry.disconnect_player_id), file);
                if (bytes_read != sizeof(entry.disconnect_player_id)) {
                    log_error("Replay file bytes read mismatch. Read %u Expected %u", bytes_read, sizeof(entry.disconnect_player_id));
                    goto fail;
                }

                log_debug("Replay entry turn %u disconnect %u", (uint32_t)replay_entries->size() - 1U, entry.disconnect_player_id);

                replay_entries->back().push_back(entry);

                break;
            }
            case REPLAY_ENTRY_NEW_TURN: {
                replay_entries->push_back(std::vector<ReplayEntry>());
                break;
            }
            default: {
                log_error("Replay file has unexpected entry type %u.", entry_type);
                goto fail;
            }
        }
    }

success:
    if (raw_map != NULL) {
        free(raw_map);
    }
    fclose(file);

    return true;

fail:
    if (raw_map != NULL) {
        free(raw_map);
    }
    fclose(file);

    return false;
}

// REPLAY SHELL INIT

static int match_shell_load_replay_checkpoints(void* shell_ptr) {
    MatchShell* shell = (MatchShell*)shell_ptr;

    while (shell->replay_loading_match_timer < match_shell_replay_end_of_tape(shell)) {
        // Match update
        if (shell->replay_loading_match_timer % TURN_DURATION == 0) {
            match_shell_replay_handle_entries_for_turn(shell->replay_entries, shell->replay_loading_match_state, nullptr, shell->replay_loading_match_timer / TURN_DURATION);
        }
        match_update(shell->replay_loading_match_state);
        shell->replay_loading_match_state.events.clear();

        // Increment timer and save replay checkpoint
        SDL_LockMutex(shell->replay_loading_mutex);
        shell->replay_loading_match_timer++;
        if (shell->replay_loading_match_timer % REPLAY_CHECKPOINT_FREQ == 0) {
            shell->replay_checkpoints.push_back(shell->replay_loading_match_state);
        }
        SDL_UnlockMutex(shell->replay_loading_mutex);

        // Check for early exit
        if (SDL_TryLockMutex(shell->replay_loading_early_exit_mutex)) {
            bool should_break = shell->replay_loading_early_exit;
            SDL_UnlockMutex(shell->replay_loading_early_exit_mutex);
            if (should_break) {
                break;
            }
        }
    }

    return 0;
}

MatchShell* match_shell_init_replay(const char* replay_path) {
    MatchShell* shell = match_shell_init_base();

    shell->replay_mode = true;
    shell->replay_ui_context = ui_init();

    // Read replay file
    if (!match_shell_replay_file_read(replay_path, shell->match_state, &shell->replay_entries)) {
        delete shell;
        return nullptr;
    }

    shell->replay_checkpoints.reserve(((match_shell_replay_end_of_tape(shell) + 1) / REPLAY_CHECKPOINT_FREQ) + 1);
    shell->replay_checkpoints.push_back(shell->match_state);

    shell->replay_loading_mutex = SDL_CreateMutex();
    shell->replay_loading_match_timer = 0;
    shell->replay_loaded_match_timer = 0;

    shell->replay_loading_early_exit_mutex = SDL_CreateMutex();
    shell->replay_loading_early_exit = false;

    shell->replay_loading_match_state = shell->match_state;
    shell->replay_loading_thread = SDL_CreateThread(match_shell_load_replay_checkpoints, "replay_loading_thread", shell);
    if (!shell->replay_loading_thread) {
        log_error("Error creating loading thread %s", SDL_GetError());
        delete shell;
        return nullptr;
    }

    match_shell_replay_scrub(shell, 0);

    // Init replay fog picker
    shell->replay_fog_index = REPLAY_FOG_EVERYONE;
    shell->replay_fog_player_ids.push_back(PLAYER_NONE);
    shell->replay_fog_texts.push_back("None");
    shell->replay_fog_player_ids.push_back(PLAYER_NONE);
    shell->replay_fog_texts.push_back("Everyone");
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (shell->match_state.players[player_id].mode != PLAYER_MODE_ACTIVE) {
            continue;
        }
        shell->replay_fog_player_ids.push_back(player_id);
        shell->replay_fog_texts.push_back(shell->match_state.players[player_id].name);
    }

    // Init camera
    for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
        const Entity& entity = shell->match_state.entities[entity_index];
        if (entity.type == ENTITY_MINER && entity.player_id == 0) {
            match_shell_center_camera_on_cell(shell, entity.cell);
            break;
        }
    }

    shell->mode = MATCH_SHELL_MODE_NONE;

    return shell;
}

// REPLAY UI

void match_shell_replay_ui_update(MatchShell* shell) {
    // Replay UI
    if (shell->replay_mode) {
        ui_begin(shell->replay_ui_context);
        shell->replay_ui_context.input_enabled = !match_shell_is_in_menu(shell);

        // Loading thread handling
        if (SDL_TryLockMutex(shell->replay_loading_mutex)) {
            shell->replay_loaded_match_timer = shell->replay_loading_match_timer;
            SDL_UnlockMutex(shell->replay_loading_mutex);
        }
        if (shell->replay_loading_thread != NULL && SDL_GetThreadState(shell->replay_loading_thread) == SDL_THREAD_COMPLETE) {
            // Clean up the thread
            SDL_WaitThread(shell->replay_loading_thread, NULL);
            shell->replay_loading_thread = NULL;
        }

        ui_begin_column(shell->replay_ui_context, ivec2(BUTTON_PANEL_RECT.x + 8, BUTTON_PANEL_RECT.y + 4), 2);
            ui_element_size(shell->replay_ui_context, ivec2(0, 16));
            ui_begin_row(shell->replay_ui_context, ivec2(0, 0), 4);
                ui_element_position(shell->replay_ui_context, ivec2(0, 2));
                ui_text(shell->replay_ui_context, FONT_HACK_WHITE, "Fog:");

                ui_element_position(shell->replay_ui_context, ivec2(render_get_text_size(FONT_HACK_WHITE, "Fog:").x, 0));
                ui_dropdown(shell->replay_ui_context, UI_DROPDOWN_MINI, &shell->replay_fog_index, shell->replay_fog_texts, false);
            ui_end_container(shell->replay_ui_context);

            uint32_t position = shell->match_timer;
            UiSliderParams slider_params = (UiSliderParams) {
                .display = UI_SLIDER_DISPLAY_NO_VALUE,
                .size = UI_SLIDER_SIZE_NORMAL,
                .min = 0,
                .max = (uint32_t)match_shell_replay_end_of_tape(shell),
                .step = 1
            };
            if (ui_slider(shell->replay_ui_context, &position, &shell->replay_loaded_match_timer, slider_params)) {
                shell->selection.clear();
                shell->chat.clear();
                match_shell_replay_scrub(shell, position);
            }

            ui_begin_row(shell->replay_ui_context, ivec2(0, 0), 6);
                if (ui_sprite_button(shell->replay_ui_context, shell->is_paused ? SPRITE_UI_REPLAY_PLAY : SPRITE_UI_REPLAY_PAUSE, false, false) ||
                        input_is_action_just_pressed(INPUT_ACTION_SPACE)) {
                    shell->is_paused = !shell->is_paused;
                }

                // Time elapsed text
                const double REPLAY_UPDATE_DURATION = 1.0 / UPDATES_PER_SECOND;
                uint32_t seconds_elapsed = shell->match_timer == 0 ? 0 : (uint32_t)((shell->match_timer - 1) * REPLAY_UPDATE_DURATION);
                Time time_elapsed = Time::from_seconds(seconds_elapsed);
                uint32_t seconds_total = (uint32_t)((match_shell_replay_end_of_tape(shell) - 1) * REPLAY_UPDATE_DURATION);
                Time time_total = Time::from_seconds(seconds_total);
                char time_text[16];
                ui_element_position(shell->replay_ui_context, ivec2(0, 2));
                sprintf(time_text, "%i:%02i:%02i/%i:%02i:%02i", time_elapsed.hours, time_elapsed.minutes, time_elapsed.seconds, time_total.hours, time_total.minutes, time_total.seconds);
                ui_text(shell->replay_ui_context, FONT_HACK_WHITE, time_text);
            ui_end_container(shell->replay_ui_context);
        ui_end_container(shell->replay_ui_context);
    }
}

// REPLAY PLAYBACK

void match_shell_replay_handle_entries_for_turn(const std::vector<std::vector<ReplayEntry>>& replay_entries, MatchState& match_state, CircularVector<ChatMessage, CHAT_MAX_LINES>* chat, uint32_t turn) {
    for (size_t entry_index = 0; entry_index < replay_entries[turn].size(); entry_index++) {
        const ReplayEntry& entry = replay_entries[turn][entry_index];

        switch (entry.type) {
            case REPLAY_ENTRY_INPUT: {
                char buffer[1024];
                match_input_print(buffer, entry.input);

                match_handle_input(match_state, entry.input);
                break;
            }
            case REPLAY_ENTRY_CHAT: {
                if (chat != nullptr) {
                    chat->push_back(entry.chat_message);
                }
                break;
            }
            case REPLAY_ENTRY_DISCONNECT: {
                match_state.players[entry.disconnect_player_id].mode = PLAYER_MODE_DEFEATED;
                break;
            }
            case REPLAY_ENTRY_NEW_TURN: {
                GOLD_ASSERT(false);
                break;
            }
        }
    }
}

void match_shell_replay_scrub(MatchShell* shell, uint32_t position) {
    if (shell->match_timer == position || position > shell->replay_loaded_match_timer) {
        return;
    }

    if (position < shell->match_timer || (position > shell->match_timer && position - shell->match_timer > REPLAY_CHECKPOINT_FREQ)) {
        uint32_t nearest_checkpoint = position / REPLAY_CHECKPOINT_FREQ;
        SDL_LockMutex(shell->replay_loading_mutex);
        shell->match_state = shell->replay_checkpoints[nearest_checkpoint];
        SDL_UnlockMutex(shell->replay_loading_mutex);
        shell->match_timer = nearest_checkpoint * REPLAY_CHECKPOINT_FREQ;
    }

    while (shell->match_timer < position) {
        if (shell->match_timer % TURN_DURATION == 0) {
            match_shell_replay_handle_entries_for_turn(shell->replay_entries, shell->match_state, &shell->chat, shell->match_timer / TURN_DURATION);
        }
        match_update(shell->match_state);
        shell->match_state.events.clear();
        shell->match_timer++;
    }
}

size_t match_shell_replay_end_of_tape(const MatchShell* state) {
    return (state->replay_entries.size() * 4) - 1;
}
