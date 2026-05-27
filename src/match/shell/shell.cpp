#include "shell.h"

#include "core/input.h"
#include "core/resource.h"
#include "core/sound.h"
#include "core/options.h"
#include "core/cursor.h"
#include "defines.h"
#include "match/state/map_gen.h"
#include "match/state/match.h"
#include "match/state/upgrade.h"
#include "network/network.h"
#include "shared/options_menu.h"
#include "util/adler32.h"
#include "util/simplex_noise.h"
#include "match/shell/desync.h"
#include "match/shell/script/script.h"

#include <algorithm>

// Match over
static const uint32_t MATCH_OVER_TIMER_DURATION = 60U;
static const uint32_t MATCH_OVER_TIMER_NOT_STARTED = UINT32_MAX;

// Sound
static const int SOUND_LISTEN_MARGIN = 64;
static const Rect SOUND_LISTEN_RECT = (Rect) {
    .x = -SOUND_LISTEN_MARGIN,
    .y = -SOUND_LISTEN_MARGIN,
    .w = SCREEN_WIDTH + (SOUND_LISTEN_MARGIN * 2),
    .h = SCREEN_HEIGHT + (SOUND_LISTEN_MARGIN * 2),
};
static const uint32_t SOUND_COOLDOWN_DURATION = 5;
static const uint32_t MUSIC_BEGIN_DELAY = 15U;

// Chat
static const size_t CHAT_MAX_LENGTH = 64;
static const uint32_t CHAT_CURSOR_BLINK_DURATION = 30;

// Alerts
static const int ATTACK_ALERT_DISTANCE = 20;

// Selection
static const uint32_t MATCH_SHELL_DOUBLE_CLICK_DURATION = 30U;

// INIT

MatchShell* match_shell_init_base() {
    MatchShell* shell = new MatchShell();

    shell->mode = MATCH_SHELL_MODE_NONE;
    shell->ui_context = ui_init();
    shell->options_menu.mode = OPTIONS_MENU_CLOSED;
    shell->menu_help_page = 0;

    // Simulation timers
    shell->match_timer = 0;
    shell->disconnect_timer = 0;
    shell->match_over_timer = MATCH_OVER_TIMER_NOT_STARTED;
    shell->match_over_is_victory = false;
    shell->is_paused = false;

    // Camera
    shell->camera_offset = ivec2(0, 0);
    shell->camera_mode = CAMERA_MODE_FREE;
    shell->camera_pan_timer = 0;
    shell->camera_shake_seed = 0;
    for (uint32_t index = 0; index < MATCH_SHELL_CAMERA_HOTKEY_COUNT; index++) {
        shell->camera_hotkeys[index] = ivec2(-1, -1);
    }

    // Bots
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        shell->bots[player_id] = bot_empty();
    }

    // Selection
    shell->select_origin = ivec2(-1, -1);
    shell->double_click_timer = 0;
    shell->control_group_selected = MATCH_SHELL_CONTROL_GROUP_NONE;
    shell->control_group_double_tap_timer = 0;

    // Status
    shell->status_timer = 0;

    // Chat
    shell->chat_cursor_visible = false;
    shell->chat_cursor_blink_timer = 0;

    // Alerts
    shell->latest_alert_cell = ivec2(-1, -1);

    // Sound
    memset(shell->sound_cooldown_timers, 0, sizeof(shell->sound_cooldown_timers));
    shell->sound_fire_voice_index = SOUND_NOT_PLAYING;
    shell->music_begin_timer = MUSIC_BEGIN_DELAY;
    shell->music_next_track = RESOURCE_MUSIC_MATCH1;

    // Animations
    shell->rally_flag_animation = animation_create(ANIMATION_RALLY_FLAG);
    shell->building_fire_animation = animation_create(ANIMATION_FIRE_BURN);

    // Gold amounts
    memset(shell->displayed_gold_amounts, 0, sizeof(shell->displayed_gold_amounts));

    // Hotkey group
    for (int index = 0; index < HOTKEY_GROUP_SIZE; index++) {
        shell->hotkey_group[index] = INPUT_HOTKEY_NONE;
    }

    // Scenario
    shell->scenario_global_objective_counter.type = GLOBAL_OBJECTIVE_COUNTER_OFF;
    shell->scenario_lua_state = NULL;

    // Replay file
    shell->replay_file = NULL;

    // Checksum
    shell->next_checksum_frame = 0U;

    // Debug
    shell->debug_fog_level = MATCH_SHELL_FOG_ENABLED;
    shell->debug_show_region_lines = false;

    return shell;
}

MatchShell* match_shell_init_match(RawMap* raw_map, int32_t lcg_seed) {
    MatchShell* shell = match_shell_init_base();

    // Populate match player info using network player info
    MatchPlayer players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {

        const NetworkPlayer& network_player = network_get_player(player_id);
        if (network_player.status == NETWORK_PLAYER_STATUS_NONE) {
            continue;
        }

        players[player_id].mode = PLAYER_MODE_ACTIVE;
        strcpy(players[player_id].name, network_player.name);
        // Use the player_id as the "team" in a FFA game to ensure everyone is on a separate team
        players[player_id].team = network_get_match_setting(MATCH_SETTING_TEAMS) == TEAMS_ENABLED
                                        ? network_player.team
                                        : player_id;
        players[player_id].recolor_id = network_player.recolor_id;
    }

    // Open replay file for writing
    MapType map_type = (MapType)network_get_match_setting(MATCH_SETTING_MAP_TYPE);
    shell->replay_file = match_shell_replay_file_open(players, map_type, raw_map, lcg_seed);
    shell->replay_mode = false;

    // Init match
    match_init(shell->match_state, players, map_type, raw_map, lcg_seed);
    match_spawn_players(shell->match_state, raw_map);

    // Init camera
    for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
        const Entity& entity = shell->match_state.entities[entity_index];
        if (entity.type == ENTITY_MINER && entity.player_id == network_get_player_id()) {
            match_shell_center_camera_on_cell(shell, entity.cell);
            break;
        }
    }

    // Init input queues
    match_shell_init_input_queues(shell);

    // Init bots
    Difficulty difficulty = (Difficulty)network_get_match_setting(MATCH_SETTING_DIFFICULTY);
    int bot_lcg_seed = lcg_seed;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status != NETWORK_PLAYER_STATUS_BOT) {
            continue;
        }

        BotConfig bot_config = bot_config_init_from_difficulty(difficulty);
        bot_config.opener = bot_config_roll_opener(&bot_lcg_seed, difficulty);
        bot_config.preferred_unit_comp = bot_config_roll_preferred_unit_comp(&bot_lcg_seed);
        shell->bots[player_id] = bot_init(shell->match_state, player_id, bot_config);
    }

    // Scenario variables, allow all entities and upgrades
    for (uint32_t entity_type_index = 0; entity_type_index < ENTITY_TYPE_COUNT; entity_type_index++) {
        shell->scenario_allowed_entities[entity_type_index] = true;
    }
    for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
        shell->scenario_allowed_upgrades |= (1U << upgrade_index);
    }

    // TODO:
    // state->achievements_state = achievements_state_init(false);

    shell->mode = MATCH_SHELL_MODE_NOT_STARTED;

    return shell;
}

MatchShell* match_shell_init_scenario(const Scenario* scenario, const char* script_path) {
    MatchShell* shell = match_shell_init_base();

    // Determine which players are active
    // A player is active as long as there is one entity in the scenario which is owned by that player
    bool player_is_active[MAX_PLAYERS] = { false, false, false, false };
    for (uint32_t entity_index = 0; entity_index < scenario->entity_count; entity_index++) {
        if (scenario->entities[entity_index].player_id != PLAYER_NONE) {
            player_is_active[scenario->entities[entity_index].player_id] = true;
        }
    }

    // Populate player info
    MatchPlayer players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (!player_is_active[player_id]) {
            continue;
        }

        players[player_id].mode = PLAYER_MODE_ACTIVE;
        strcpy(players[player_id].name, player_id == network_get_player_id()
            ? network_get_username()
            : scenario->players[player_id].name);
        players[player_id].team = scenario->players[player_id].team;
        players[player_id].recolor_id = scenario->players[player_id].recolor_id;
        players[player_id].gold = scenario->players[player_id].starting_gold;
        players[player_id].gold_mined_total = 0;
        players[player_id].upgrades = 0;
        players[player_id].upgrades_in_progress = 0;
    }

    // Init match
    match_init(shell->match_state, players, scenario->map_type, scenario->raw_map, rand());

    // Create entities
    for (uint32_t entity_index = 0; entity_index < scenario->entity_count; entity_index++) {
        const ScenarioEntity& entity = scenario->entities[entity_index];

        if (entity_is_misc(entity.type)) {
            EntityId entity_id = entity_create_misc(shell->match_state, entity.type, entity.cell, entity.gold_held);
            if (entity.type == ENTITY_GOLDMINE && entity.is_rigged) {
                Entity& goldmine = shell->match_state.entities.get_by_id(entity_id);
                goldmine.mode = MODE_GOLDMINE_RIGGED;
            }
            continue;
        }

        if (entity_is_building(entity.type)) {
            entity_create_finished_building(shell->match_state, entity.type, entity.cell, entity.player_id);
        } else {
           entity_create(shell->match_state, entity.type, entity.cell, entity.player_id);
        }
    }

    // Allowed entities and upgrades
    memcpy(shell->scenario_allowed_entities, scenario->player_allowed_entities, sizeof(shell->scenario_allowed_entities));
    shell->scenario_allowed_upgrades = scenario->player_allowed_upgrades;

    // Bots
    for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
        if (!player_is_active[player_id]) {
            continue;
        }

        BotConfig bot_config = scenario->bot_config[player_id - 1];
        shell->bots[player_id] = bot_init(shell->match_state, player_id, bot_config);

        // Squads
        for (const ScenarioSquad& squad : scenario->squads) {
            if (squad.player_id != player_id) {
                continue;
            }

            ivec2 squad_average_position = ivec2(0, 0);
            EntityList squad_entity_list;
            for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_count; squad_entity_index++) {
                uint32_t entity_index = squad.entities[squad_entity_index];

                // Since we added entities from the scenario in-order,
                // the entity index in scenario should match the entity index in the match state
                squad_entity_list.push_back(shell->match_state.entities.get_id_of(entity_index));
                squad_average_position += scenario->entities[entity_index].cell;
            }

            if (squad_entity_list.empty()) {
                log_error("Squad %s entity list is empty.", squad.name);
                delete shell;
                return nullptr;
            }
            ivec2 squad_target_cell = squad_average_position / squad_entity_list.size();

            if (squad.type == BOT_SQUAD_TYPE_PATROL) {
                GOLD_ASSERT(squad.patrol_cell.x != -1);
            }
            bot_add_squad(shell->bots[player_id], {
                .type = squad.type,
                .target_cell = squad_target_cell,
                .patrol_cell = squad.type == BOT_SQUAD_TYPE_PATROL
                    ? squad.patrol_cell
                    : ivec2(-1, -1),
                .entity_list = squad_entity_list
            });
        }
    }

    match_shell_init_input_queues(shell);
    match_shell_center_camera_on_cell(shell, scenario->player_spawn);

    shell->mode = MATCH_SHELL_MODE_NOT_STARTED;

    // Script
    if (!script_init(shell, scenario, script_path)) {
        delete shell;
        return nullptr;
    }

    // TODO: init achievements state

    return shell;
}

void match_shell_init_input_queues(MatchShell* shell) {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (shell->match_state.players[player_id].mode != PLAYER_MODE_ACTIVE) {
            continue;
        }

        for (uint8_t i = 0; i < TURN_OFFSET - 1; i++) {
            shell->inputs[player_id].push({ (MatchInput) { .type = MATCH_INPUT_NONE } });
        }
    }
}

void match_shell_free(MatchShell* shell) {
    if (!match_shell_is_in_leave_match_mode(shell)) {
        match_shell_leave_match(shell, MATCH_SHELL_MODE_MATCH_OVER_DEFEAT);
    }
    delete shell;
}

// NETWORK EVENT

void match_shell_handle_network_event(MatchShell* shell, NetworkEvent event) {
    switch (event.type) {
        case NETWORK_EVENT_INPUT: {
            // Deserialize input
            std::vector<MatchInput> inputs;

            const uint8_t* in_buffer = event.input.in_buffer;
            size_t in_buffer_head = 1; // Advance the head by once since the first byte will contain the network message type

            while (in_buffer_head < event.input.in_buffer_length) {
                inputs.push_back(match_input_deserialize(in_buffer, in_buffer_head));
            }

            shell->inputs[event.input.player_id].push(inputs);
            break;
        }
        case NETWORK_EVENT_CHAT: {
            char prefix[MATCH_SHELL_CHAT_PREFIX_BUFFER_SIZE];
            match_shell_get_player_prefix(shell, event.chat.player_id, prefix);
            match_shell_add_chat_message(shell, match_shell_get_player_font(event.chat.player_id), prefix, event.chat.message, CHAT_MESSAGE_DURATION);
            break;
        }
        case NETWORK_EVENT_PLAYER_DISCONNECTED: {
            match_shell_handle_player_disconnect(shell, event.player_disconnected.player_id);
            break;
        }
        case NETWORK_EVENT_CHECKSUM: {
            shell->checksums[event.checksum.player_id].push(event.checksum.checksum);
            break;
        }
        case NETWORK_EVENT_SERIALIZED_FRAME: {
            desync_handle_serialized_frame(event.serialized_frame.state_buffer);
            break;
        }
        default:
            break;
    }
}

// UPDATE

void match_shell_update(MatchShell* shell) {
    ZoneScoped;

    // If we have left the match, return
    if (match_shell_is_in_leave_match_mode(shell)) {
        return;
    }

    // Await match start
    if (shell->mode == MATCH_SHELL_MODE_NOT_STARTED) {
        if (network_get_player(network_get_player_id()).status == NETWORK_PLAYER_STATUS_NOT_READY) {
            network_set_player_ready(true);
        }

        // Check that all players are ready
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NOT_READY) {
                return;
            }
        }
        // If we reached here, then all players are ready
        shell->mode = MATCH_SHELL_MODE_NONE;
        log_info("Match started.");
    }

    // MENU
    match_shell_menu_update(shell);

    // REPLAY UI
    if (shell->replay_mode) {
        match_shell_replay_ui_update(shell);
    }

    // PRE-MATCH-UPDATE INPUTS

    // Camera update
    match_shell_camera_update(shell);

    // Selection update
    match_shell_selection_update(shell);

    // Update displayed gold amounts
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (shell->match_state.players[player_id].mode == PLAYER_MODE_INACTIVE) {
            continue;
        }
        shell->displayed_gold_amounts[player_id] = match_shell_update_displayed_gold_amount(shell->displayed_gold_amounts[player_id], shell->match_state.players[player_id].gold);
        if (shell->scenario_global_objective_counter.type == GLOBAL_OBJECTIVE_COUNTER_GOLD) {
            shell->scenario_global_objective_counter.gold.values[player_id] = std::min(
                match_shell_update_displayed_gold_amount(shell->scenario_global_objective_counter.gold.values[player_id], shell->match_state.players[player_id].gold_mined_total),
                shell->scenario_global_objective_counter.gold.max_value);
        }
    }

    // Play fire sound effects
    bool is_fire_on_screen = match_shell_is_fire_on_screen(shell);
    if (is_fire_on_screen && shell->sound_fire_voice_index == SOUND_NOT_PLAYING) {
        shell->sound_fire_voice_index = sound_play(SOUND_FIRE_BURN, true);
    } else if (!is_fire_on_screen && shell->sound_fire_voice_index != SOUND_NOT_PLAYING) {
        sound_stop(shell->sound_fire_voice_index);
        shell->sound_fire_voice_index = SOUND_NOT_PLAYING;
    }

    // Music
    if (shell->music_begin_timer != 0) {
        shell->music_begin_timer--;
    }
    if (shell->music_begin_timer == 0 && !sound_is_music_playing()) {
        sound_play_music((ResourceName)shell->music_next_track, 0);
        shell->music_next_track++;
        if (shell->music_next_track == RESOURCE_MUSIC_MATCH1 + MATCH_SHELL_MUSIC_TRACK_COUNT) {
            shell->music_next_track = RESOURCE_MUSIC_MATCH1;
        }
    }

    // Update chat blinker
    if (input_is_text_input_active()) {
        shell->chat_cursor_blink_timer--;
        if (shell->chat_cursor_blink_timer == 0) {
            shell->chat_cursor_visible = !shell->chat_cursor_visible;
            shell->chat_cursor_blink_timer = CHAT_CURSOR_BLINK_DURATION;
        }
    }
    // Update chat
    for (uint32_t chat_index = 0; chat_index < shell->chat.size(); chat_index++) {
        shell->chat[chat_index].timer--;
        if (shell->chat[chat_index].timer == 0) {
            shell->chat.remove_at_ordered(chat_index);
            chat_index--;
        }
    }

    // Update timers and animations
    if (animation_is_playing(shell->move_animation)) {
        animation_update(shell->move_animation);
    }
    animation_update(shell->rally_flag_animation);
    animation_update(shell->building_fire_animation);
    if (shell->status_timer != 0) {
        shell->status_timer--;
    }
    if (shell->double_click_timer != 0) {
        shell->double_click_timer--;
    }
    if (shell->control_group_double_tap_timer != 0) {
        shell->control_group_double_tap_timer--;
    }
    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        if (shell->sound_cooldown_timers[sound] != 0) {
            shell->sound_cooldown_timers[sound]--;
        }
    }

    // Entity highlights update
    {
        uint32_t entity_highlight_index = 0;
        while (entity_highlight_index < shell->entity_highlights.size()) {
            EntityHighlight& entity_highlight = shell->entity_highlights[entity_highlight_index];
            const uint32_t entity_index = shell->match_state.entities.get_index_of(entity_highlight.entity_id);
            const bool highlight_is_still_active =
                animation_is_playing(entity_highlight.animation) &&
                entity_index != INDEX_INVALID &&
                entity_is_selectable(shell->match_state.entities[entity_index]);
            if (highlight_is_still_active) {
                animation_update(entity_highlight.animation);
                entity_highlight_index++;
            } else {
                shell->entity_highlights[entity_highlight_index] = shell->entity_highlights.back();
                shell->entity_highlights.pop_back();
            }
        }
    }

    // Alerts update
    {
        uint32_t alert_index = 0;
        while (alert_index < shell->alerts.size()) {
            shell->alerts[alert_index].timer--;
            if (shell->alerts[alert_index].timer == 0) {
                shell->alerts.erase(shell->alerts.begin() + alert_index);
            } else {
                alert_index++;
            }
        }
    }

    // PAUSED
    if (shell->replay_mode && shell->match_timer == match_shell_replay_end_of_tape(shell)) {
        shell->is_paused = true;
    }
    if (shell->is_paused ||
            (match_shell_is_in_menu(shell) && match_shell_is_in_single_player_game()) ||
            match_shell_is_in_leave_match_mode(shell) ||
            shell->mode == MATCH_SHELL_MODE_DESYNC) {
        return;
    }

    // BEGIN TURN

    const bool should_begin_turn = shell->match_timer % TURN_DURATION == 0;

    if (!shell->replay_mode && should_begin_turn) {
        if (!match_shell_begin_turn(shell)) {
            return;
        }
    }

    if (shell->replay_mode && should_begin_turn) {
        match_shell_replay_handle_entries_for_turn(shell->replay_entries, shell->match_state, &shell->chat, shell->match_timer / TURN_DURATION);
    }

    // CHECKSUM

    // Compute checksum
    if (!shell->replay_mode && shell->match_timer % desync_get_checksum_frequency() == 0) {
        uint32_t checksum = adler32_simd((uint8_t*)&shell->match_state, DESYNC_BUFFER_SIZE);
        desync_write_frame((uint8_t*)&shell->match_state, shell->match_timer);
        network_send_checksum(checksum);
        shell->checksums[network_get_player_id()].push(checksum);

        // This is only defined when GOLD_SIMD_CHECKSUM_TEST is defined
        adler32_test((uint8_t*)&shell->match_state, DESYNC_BUFFER_SIZE);
    }

    // Compare checksums
    if (match_shell_debug_has_next_checksums(shell)) {
        if (match_shell_debug_are_next_checksums_out_of_sync(shell)) {
            desync_send_frame(shell->next_checksum_frame);
            shell->mode = MATCH_SHELL_MODE_DESYNC;

            return;
        }

        // Pop checksums and delete serialized frame
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_READY) {
                shell->checksums[player_id].pop();
            }
        }
        desync_delete_frame(shell->next_checksum_frame);
        shell->next_checksum_frame += desync_get_checksum_frequency();
    }

    // MATCH UPDATE
    match_update(shell->match_state);
    shell->match_timer++;

    // Match events
    while (!shell->match_state.events.empty()) {
        const MatchEvent event = shell->match_state.events.front();
        shell->match_state.events.pop();
        switch (event.type) {
            case MATCH_EVENT_SOUND: {
                if (shell->sound_cooldown_timers[event.sound.sound] != 0) {
                    break;
                }
                if (!match_shell_is_cell_rect_revealed(shell, event.sound.position / TILE_SIZE, 1)) {
                    break;
                }
                if (SOUND_LISTEN_RECT.has_point(event.sound.position - shell->camera_offset)) {
                    sound_play(event.sound.sound);
                    shell->sound_cooldown_timers[event.sound.sound] = SOUND_COOLDOWN_DURATION;
                }

                break;
            }
            case MATCH_EVENT_ALERT: {
                if (shell->replay_mode || shell->match_state.players[network_get_player_id()].mode != PLAYER_MODE_ACTIVE) {
                    break;
                }
                if ((event.alert.type == MATCH_ALERT_TYPE_ATTACK && shell->match_state.players[event.alert.player_id].team != shell->match_state.players[network_get_player_id()].team) ||
                    (event.alert.type != MATCH_ALERT_TYPE_ATTACK && event.alert.player_id != network_get_player_id())) {
                    break;
                }

                // Check if an existing attack alert already exists nearby
                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    bool is_existing_attack_alert_nearby = false;
                    for (const Alert& existing_alert : shell->alerts) {
                        if (existing_alert.pixel == MINIMAP_PIXEL_WHITE && ivec2::manhattan_distance(existing_alert.cell, event.alert.cell) < ATTACK_ALERT_DISTANCE) {
                            is_existing_attack_alert_nearby = true;
                            break;
                        }
                    }
                    if (is_existing_attack_alert_nearby) {
                        break;
                    }
                }


                // Play the sound even if we don't show the alert
                switch (event.alert.type) {
                    case MATCH_ALERT_TYPE_BUILDING:
                        sound_play(SOUND_ALERT_BUILDING);
                        break;
                    case MATCH_ALERT_TYPE_UNIT:
                        sound_play(SOUND_ALERT_UNIT);
                        break;
                    case MATCH_ALERT_TYPE_RESEARCH:
                        sound_play(SOUND_ALERT_RESEARCH);
                        break;
                    case MATCH_ALERT_TYPE_MINE_COLLAPSE:
                        // Since the mine plays a sound effect that those nearby can hear,
                        // we should only play this sound if the player's camera is out-of-range of the sound
                        if (!SOUND_LISTEN_RECT.has_point((event.alert.cell * TILE_SIZE) - shell->camera_offset)) {
                            sound_play(SOUND_GOLD_MINE_COLLAPSE);
                        }
                        break;
                    default:
                        break;
                }
                shell->latest_alert_cell = event.alert.cell;

                Rect camera_rect = (Rect) {
                    .x = shell->camera_offset.x,
                    .y = shell->camera_offset.y,
                    .w = SCREEN_WIDTH,
                    .h = SCREEN_HEIGHT
                };
                Rect alert_rect = (Rect) {
                    .x = event.alert.cell.x * TILE_SIZE,
                    .y = event.alert.cell.y * TILE_SIZE,
                    .w = event.alert.cell_size * TILE_SIZE,
                    .h = event.alert.cell_size * TILE_SIZE
                };
                // If the player is already looking at the alert location, then don't show the alert
                if (camera_rect.intersects(alert_rect)) {
                    break;
                }

                MinimapPixel pixel;
                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    pixel = MINIMAP_PIXEL_WHITE;
                } else if (event.alert.type == MATCH_ALERT_TYPE_MINE_COLLAPSE || event.alert.type == MATCH_ALERT_TYPE_MINE_RUNNING_LOW) {
                    pixel = MINIMAP_PIXEL_GOLD;
                } else {
                    pixel = (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + shell->match_state.players[network_get_player_id()].recolor_id);
                }

                shell->alerts.push_back((Alert) {
                    .pixel = pixel,
                    .cell = event.alert.cell,
                    .cell_size = event.alert.cell_size,
                    .timer = ALERT_TOTAL_DURATION
                });

                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    match_shell_show_status(shell, event.alert.player_id == network_get_player_id()
                                                    ? MATCH_UI_STATUS_UNDER_ATTACK
                                                    : MATCH_UI_STATUS_ALLY_UNDER_ATTACK);
                    sound_play(SOUND_ALERT_BELL);
                }
                break;
            }
            case MATCH_EVENT_SELECTION_HANDOFF: {
                if (shell->replay_mode || shell->match_state.players[network_get_player_id()].mode != PLAYER_MODE_ACTIVE) {
                    break;
                }
                if (event.selection_handoff.player_id != network_get_player_id()) {
                    break;
                }

                if (shell->selection.size() == 1 && shell->selection[0] == event.selection_handoff.to_deselect) {
                    if (match_shell_is_in_menu(shell)) {
                        shell->selection.clear();
                    } else {
                        std::vector<EntityId> new_selection;
                        new_selection.push_back(event.selection_handoff.to_select);
                        match_shell_set_selection(shell, new_selection);
                    }
                }
                break;
            }
            case MATCH_EVENT_STATUS: {
                if (shell->replay_mode || shell->match_state.players[network_get_player_id()].mode != PLAYER_MODE_ACTIVE) {
                    break;
                }
                if (network_get_player_id() == event.status.player_id) {
                    match_shell_show_status(shell, event.status.message);
                }
                break;
            }
            case MATCH_EVENT_RESEARCH_COMPLETE: {
                if (shell->replay_mode || shell->match_state.players[network_get_player_id()].mode != PLAYER_MODE_ACTIVE) {
                    break;
                }
                if (event.research_complete.player_id != network_get_player_id()) {
                    break;
                }

                char message[128];
                sprintf(message, "%s research complete.", upgrade_get_data(event.research_complete.upgrade).name);
                match_shell_show_status(shell, message);
                break;
            }
            case MATCH_EVENT_PLAYER_DEFEATED: {
                char defeat_message[128];
                sprintf(defeat_message, "%s has been defeated.", shell->match_state.players[event.player_defeated.player_id].name);
                match_shell_add_chat_message(shell, FONT_HACK_WHITE, "", defeat_message, CHAT_MESSAGE_DURATION);

                if (!shell->replay_mode &&
                        event.player_defeated.player_id == network_get_player_id()) {
                    shell->match_over_timer = MATCH_OVER_TIMER_DURATION;
                    shell->match_over_is_victory = false;
                    break;
                }

                if (!shell->replay_mode &&
                        shell->scenario_lua_state == NULL &&
                        !match_shell_is_at_least_one_opponent_in_match(shell)) {
                    shell->match_over_timer = MATCH_OVER_TIMER_DURATION;
                    shell->match_over_is_victory = true;
                    break;
                }

                break;
            }
            case MATCH_EVENT_ENTITY_KILLED: {
                break;
            }
        }

        if (!shell->replay_mode) {
            // TODO:
            // achievements_handle_event(state->achievements_state, state->match_state, event);
        }
    }

    // Scenario script
    if (shell->scenario_lua_state != NULL) {
        script_update(shell);
    }

    // Scenario avalanche
    {
        const int AVALANCHE_SPEED = 2;

        uint32_t avalanche_index = 0;
        while (avalanche_index < shell->scenario_avalanche_columns.size()) {
            AvalancheColumn& avalanche = shell->scenario_avalanche_columns[avalanche_index];
            animation_update(avalanche.animation);
            avalanche.position += ivec2(0, AVALANCHE_SPEED);

            const Rect avalanche_rect = (Rect) {
                .x = avalanche.position.x - 10,
                .y = avalanche.position.y - 10,
                .w = 20,
                .h = 20
            };

            for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
                Entity& entity = shell->match_state.entities[entity_index];
                if (entity_is_misc(entity.type) ||
                        !entity_is_selectable(entity) ||
                        entity_get_data(entity.type).cell_layer == CELL_LAYER_SKY) {
                    continue;
                }
                const Rect entity_rect = entity_get_rect(entity);
                if (entity_rect.intersects(avalanche_rect)) {
                    entity.health = 0;
                }
            }

            if (avalanche.position.y >= avalanche.destination.y) {
                shell->scenario_avalanche_columns[avalanche_index] = shell->scenario_avalanche_columns.back();
                shell->scenario_avalanche_columns.pop_back();
            } else {
                avalanche_index++;
            }
        }
    }

    // Achievements
    if (!shell->replay_mode) {
        // TODO:
        // achievements_update(state->achievements_state, state->match_state);
    }

    // Handle input
    if (!match_shell_is_in_menu(shell)) {
        match_shell_handle_input(shell);
    }

    // Clear hidden units from selection
    {
        uint32_t selection_index = 0;
        while (selection_index < shell->selection.size()) {
            Entity& selected_entity = shell->match_state.entities.get_by_id(shell->selection[selection_index]);
            if (!match_shell_can_keep_selecting_entity(shell, selected_entity)) {
                shell->selection.erase(shell->selection.begin() + selection_index);
            } else {
                selection_index++;
            }
        }
    }

    // Update cursor
    cursor_set(match_shell_is_targeting(shell) && (!match_shell_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position())) ? CURSOR_TARGET : CURSOR_DEFAULT);
    if ((match_shell_is_targeting(shell) || shell->mode == MATCH_SHELL_MODE_BUILDING_PLACE || match_shell_is_in_hotkey_submenu(shell)) && shell->selection.empty()) {
        shell->mode = MATCH_SHELL_MODE_NONE;
    }

    // Update UI buttons
    if (!shell->replay_mode) {
        for (uint32_t index = 0; index < HOTKEY_GROUP_SIZE; index++) {
            shell->hotkey_group[index] = INPUT_HOTKEY_NONE;
        }
        if (shell->mode == MATCH_SHELL_MODE_BUILD) {
            shell->hotkey_group[0] = INPUT_HOTKEY_HALL;
            shell->hotkey_group[1] = INPUT_HOTKEY_HOUSE;
            shell->hotkey_group[2] = INPUT_HOTKEY_SALOON;
            shell->hotkey_group[3] = INPUT_HOTKEY_BUNKER;
            shell->hotkey_group[4] = INPUT_HOTKEY_WORKSHOP;
            shell->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
        } else if (shell->mode == MATCH_SHELL_MODE_BUILD2) {
            shell->hotkey_group[0] = INPUT_HOTKEY_SMITH;
            shell->hotkey_group[1] = INPUT_HOTKEY_COOP;
            shell->hotkey_group[2] = INPUT_HOTKEY_BARRACKS;
            shell->hotkey_group[3] = INPUT_HOTKEY_SHERIFFS;
            shell->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
        } else if (shell->mode == MATCH_SHELL_MODE_BUILDING_PLACE || match_shell_is_targeting(shell)) {
            shell->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
        } else if (!shell->selection.empty()) {
            Entity& first_entity = shell->match_state.entities.get_by_id(shell->selection[0]);
            if (first_entity.player_id == network_get_player_id() && first_entity.mode == MODE_BUILDING_IN_PROGRESS && shell->selection.size() == 1) {
                shell->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
            } else if (first_entity.player_id == network_get_player_id()) {
                bool is_selection_uniform = true;
                bool has_garrisoned_units = !first_entity.garrisoned_units.empty();
                if (first_entity.mode == MODE_BUILDING_IN_PROGRESS) {
                    is_selection_uniform = false;
                }
                for (uint32_t selection_index = 1; selection_index < shell->selection.size(); selection_index++) {
                    const Entity& entity = shell->match_state.entities.get_by_id(shell->selection[selection_index]);
                    if (!entity.garrisoned_units.empty()) {
                        has_garrisoned_units = true;
                    }
                    // If units are not the same type, selection is not uniform
                    if (entity.type != first_entity.type) {
                        is_selection_uniform = false;
                        continue;
                    }
                    // Check invisibility values, ensures that camo / decamo command is the same
                    if (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) != entity_check_flag(first_entity, ENTITY_FLAG_INVISIBLE)) {
                        is_selection_uniform = false;
                        continue;
                    }
                    // Selection is not uniform for in-progress buildings, prevents showing unit train icons when one of the selected buildings is in-progress
                    if (entity.mode == MODE_BUILDING_IN_PROGRESS) {
                        is_selection_uniform = false;
                        continue;
                    }
                }

                if (entity_is_unit(first_entity.type)) {
                    shell->hotkey_group[0] = INPUT_HOTKEY_ATTACK;
                    shell->hotkey_group[1] = INPUT_HOTKEY_STOP;
                    shell->hotkey_group[2] = INPUT_HOTKEY_DEFEND;
                }
                if (entity_is_building(first_entity.type) && !first_entity.queue.empty() && shell->selection.size() == 1) {
                    shell->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
                }

                if (is_selection_uniform) {
                    if (has_garrisoned_units) {
                        shell->hotkey_group[3] = INPUT_HOTKEY_UNLOAD;
                    }
                    switch (first_entity.type) {
                        case ENTITY_MINER: {
                            shell->hotkey_group[3] = INPUT_HOTKEY_REPAIR;
                            shell->hotkey_group[4] = INPUT_HOTKEY_BUILD;
                            shell->hotkey_group[5] = INPUT_HOTKEY_BUILD2;
                            break;
                        }
                        case ENTITY_HALL: {
                            shell->hotkey_group[0] = INPUT_HOTKEY_MINER;
                            break;
                        }
                        case ENTITY_SALOON: {
                            shell->hotkey_group[0] = INPUT_HOTKEY_COWBOY;
                            shell->hotkey_group[1] = INPUT_HOTKEY_BANDIT;
                            break;
                        }
                        case ENTITY_WORKSHOP: {
                            shell->hotkey_group[0] = INPUT_HOTKEY_SAPPER;
                            shell->hotkey_group[1] = INPUT_HOTKEY_PYRO;
                            shell->hotkey_group[2] = INPUT_HOTKEY_BALLOON;
                            shell->hotkey_group[3] = INPUT_HOTKEY_RESEARCH_LANDMINES;
                            break;
                        }
                        case ENTITY_SMITH: {
                            shell->hotkey_group[0] = INPUT_HOTKEY_RESEARCH_GETAWAY_BOOTS;
                            shell->hotkey_group[1] = INPUT_HOTKEY_RESEARCH_IRON_SIGHTS;
                            shell->hotkey_group[2] = INPUT_HOTKEY_RESEARCH_WAGON_ARMOR;
                            shell->hotkey_group[3] = INPUT_HOTKEY_RESEARCH_BAYONETS;
                            break;
                        }
                        case ENTITY_COOP: {
                            shell->hotkey_group[0] = INPUT_HOTKEY_JOCKEY;
                            shell->hotkey_group[1] = INPUT_HOTKEY_WAGON;
                            break;
                        }
                        case ENTITY_BARRACKS: {
                            shell->hotkey_group[0] = INPUT_HOTKEY_SOLDIER;
                            shell->hotkey_group[1] = INPUT_HOTKEY_CANNON;
                            break;
                        }
                        case ENTITY_SHERIFFS: {
                            shell->hotkey_group[0] = INPUT_HOTKEY_DETECTIVE;
                            shell->hotkey_group[3] = INPUT_HOTKEY_RESEARCH_PRIVATE_EYE;
                            break;
                        }
                        case ENTITY_PYRO: {
                            shell->hotkey_group[3] = INPUT_HOTKEY_MOLOTOV;
                            shell->hotkey_group[4] = INPUT_HOTKEY_LANDMINE;
                            break;
                        }
                        case ENTITY_DETECTIVE: {
                            shell->hotkey_group[3] = INPUT_HOTKEY_CAMO;
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
        }

        // Conditionally clear hotkeys
        for (uint32_t hotkey_index = 0; hotkey_index < HOTKEY_GROUP_SIZE; hotkey_index++) {
            if (shell->hotkey_group[hotkey_index] == INPUT_HOTKEY_NONE) {
                continue;
            }
            const HotkeyButtonInfo& info = hotkey_get_button_info(shell->hotkey_group[hotkey_index]);
            if (!match_shell_is_hotkey_available(shell, info)) {
                shell->hotkey_group[hotkey_index] = INPUT_HOTKEY_NONE;
            }
        }
    }

    // Match over timer
    if (shell->match_over_timer != MATCH_OVER_TIMER_NOT_STARTED) {
        shell->match_over_timer--;
        if (shell->match_over_timer == 0) {
            if (shell->match_over_is_victory) {
                GOLD_ASSERT(shell->scenario_lua_state == NULL);
                shell->mode = MATCH_SHELL_MODE_MATCH_OVER_VICTORY;
            } else if (shell->scenario_lua_state != NULL) {
                shell->mode = MATCH_SHELL_MODE_SCENARIO_DEFEAT;
            } else {
                shell->mode = MATCH_SHELL_MODE_MATCH_OVER_DEFEAT;
            }
        }
    }
}

bool match_shell_begin_turn(MatchShell* shell) {
    // Bot inputs
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        // Filter down to active, bot players
        if (!(shell->match_state.players[player_id].mode == PLAYER_MODE_ACTIVE &&
                network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT)) {
            continue;
        }

        if (shell->inputs[player_id].empty()) {
            MatchInput bot_input = bot_get_turn_input(shell->match_state, shell->bots[player_id], shell->match_timer);
            shell->inputs[player_id].push({ bot_input });

            // Buffer empty inputs. This way the bot can always assume that all its inputs have been applied when deciding on the next one
            for (uint32_t index = 0; index < TURN_OFFSET - 1; index++) {
                shell->inputs[player_id].push({ (MatchInput) { .type = MATCH_INPUT_NONE } });
            }

            // Check for bot surrender
            if (bot_should_surrender(shell->match_state, shell->bots[player_id], shell->match_timer)) {
                char prefix[MATCH_SHELL_CHAT_PREFIX_BUFFER_SIZE];
                match_shell_get_player_prefix(shell, player_id, prefix);
                match_shell_add_chat_message(shell, match_shell_get_player_font(player_id), prefix, "gg", CHAT_MESSAGE_DURATION);
                match_shell_handle_player_disconnect(shell, player_id);
                // handle_player_disconnect should have set the bot to defeated for us
                GOLD_ASSERT(shell->match_state.players[player_id].mode == PLAYER_MODE_DEFEATED);
            }
        }
    }

    // Check that all inputs have been received
    bool all_inputs_received = true;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (shell->match_state.players[player_id].mode != PLAYER_MODE_ACTIVE) {
            continue;
        }

        if (shell->inputs[player_id].empty() || shell->inputs[player_id].front().empty()) {
            all_inputs_received = false;
            continue;
        }
    }

    if (!all_inputs_received) {
        shell->disconnect_timer++;
        return false;
    }

    // Reset the disconnect timer if we recevied inputs
    shell->disconnect_timer = 0;

    // All inputs received. Begin next turn
    match_shell_replay_file_write_entry(shell->replay_file, (ReplayEntry) { .type = REPLAY_ENTRY_NEW_TURN });

    // Handle input
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (shell->match_state.players[player_id].mode != PLAYER_MODE_ACTIVE) {
            continue;
        }

        for (const MatchInput& input : shell->inputs[player_id].front()) {
            // Write input to replay file
            if (input.type != MATCH_INPUT_NONE) {
                match_shell_replay_file_write_entry(shell->replay_file, (ReplayEntry) {
                    .type = REPLAY_ENTRY_INPUT,
                    .input = input
                });
            }

            // Handle input
            match_handle_input(shell->match_state, input);

            // Log input
            if (input.type != MATCH_INPUT_NONE) {
                char debug_buffer[512];
                char* out_ptr = debug_buffer;
                out_ptr += sprintf(out_ptr, "TURN %u PLAYER %u ", shell->match_timer / TURN_DURATION, player_id);
                match_input_print(out_ptr, input);
                log_info(debug_buffer);
            }
        }
        shell->inputs[player_id].pop();
    }

    // Flush input
    if (shell->match_state.players[network_get_player_id()].mode == PLAYER_MODE_ACTIVE) {
        // Always send at least one input per turn
        if (shell->input_queue.empty()) {
            shell->input_queue.push_back((MatchInput) { .type = MATCH_INPUT_NONE });
        }

        // Serialize the inputs
        uint8_t out_buffer[NETWORK_INPUT_BUFFER_SIZE];
        size_t out_buffer_length = 1;
        for (const MatchInput& input : shell->input_queue) {
            match_input_serialize(out_buffer, out_buffer_length, input);
            GOLD_ASSERT(out_buffer_length <= NETWORK_INPUT_BUFFER_SIZE);
        }
        shell->inputs[network_get_player_id()].push(shell->input_queue);
        shell->input_queue.clear();

        // Send inputs to other players
        network_send_input(out_buffer, out_buffer_length);
    }

    return true;
}

void match_shell_handle_input(MatchShell* shell) {
    if (!match_shell_is_camera_free(shell) || match_shell_is_selecting(shell)) {
        return;
    }

    const bool spectator_mode = shell->replay_mode || shell->match_state.players[network_get_player_id()].mode != PLAYER_MODE_ACTIVE;

    // Begin chat
    if (input_is_action_just_pressed(INPUT_ACTION_ENTER) && !input_is_text_input_active() && !shell->replay_mode) {
        shell->chat_message = "";
        shell->chat_cursor_blink_timer = CHAT_CURSOR_BLINK_DURATION;
        shell->chat_cursor_visible = false;
        input_start_text_input(&shell->chat_message, CHAT_MAX_LENGTH);
        sound_play(SOUND_UI_CLICK);
        return;
    }

    // End chat
    if (input_is_action_just_pressed(INPUT_ACTION_ENTER) && input_is_text_input_active() && !shell->replay_mode) {
        if (!shell->chat_message.empty()) {
            match_shell_debug_handle_chat_message(shell, shell->chat_message);
            network_send_chat(shell->chat_message.c_str());

            char prefix[MATCH_SHELL_CHAT_PREFIX_BUFFER_SIZE];
            match_shell_get_player_prefix(shell, network_get_player_id(), prefix);
            match_shell_add_chat_message(shell, match_shell_get_player_font(network_get_player_id()), prefix, shell->chat_message.c_str(), CHAT_MESSAGE_DURATION);
        }
        sound_play(SOUND_UI_CLICK);
        input_stop_text_input();
        return;
    }

    // Hotkey click
    if (!spectator_mode) {
        for (uint32_t hotkey_index = 0; hotkey_index < HOTKEY_GROUP_SIZE; hotkey_index++) {
            InputHotkey hotkey = shell->hotkey_group[hotkey_index];
            if (hotkey == INPUT_HOTKEY_NONE) {
                continue;
            }
            if (!match_shell_does_player_meet_hotkey_requirements(shell->match_state, hotkey)) {
                continue;
            }

            const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
            ivec2 hotkey_position = HOTKEY_BUTTON_POSITIONS[hotkey_index];
            Rect hotkey_rect = (Rect) {
                .x = hotkey_position.x, .y = hotkey_position.y,
                .w = sprite_info.frame_width, .h = sprite_info.frame_height
            };

            if ((input_is_hotkey_just_pressed(hotkey) && !input_is_text_input_active()) || (
                input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) && hotkey_rect.has_point(input_get_mouse_position())
            )) {
                const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey);

                if (hotkey_info.type == HOTKEY_BUTTON_ACTION || hotkey_info.type == HOTKEY_BUTTON_TOGGLED_ACTION) {
                    switch (hotkey) {
                        case INPUT_HOTKEY_ATTACK: {
                            shell->mode = MATCH_SHELL_MODE_TARGET_ATTACK;
                            break;
                        }
                        case INPUT_HOTKEY_BUILD: {
                            shell->mode = MATCH_SHELL_MODE_BUILD;
                            break;
                        }
                        case INPUT_HOTKEY_BUILD2: {
                            shell->mode = MATCH_SHELL_MODE_BUILD2;
                            break;
                        }
                        case INPUT_HOTKEY_CANCEL: {
                            if (match_shell_is_in_hotkey_submenu(shell) || match_shell_is_targeting(shell)) {
                                shell->mode = MATCH_SHELL_MODE_NONE;
                            } else if (shell->mode == MATCH_SHELL_MODE_BUILDING_PLACE) {
                                shell->mode = MATCH_SHELL_MODE_NONE;
                            } else if (shell->selection.size() == 1) {
                                const Entity& entity = shell->match_state.entities.get_by_id(shell->selection[0]);
                                if (entity.mode == MODE_BUILDING_IN_PROGRESS) {
                                    shell->input_queue.push_back((MatchInput) {
                                        .type = MATCH_INPUT_BUILD_CANCEL,
                                        .build_cancel = (MatchInputBuildCancel) {
                                            .building_id = shell->selection[0]
                                        }
                                    });
                                } else if (entity.mode == MODE_BUILDING_FINISHED && !entity.queue.empty()) {
                                    shell->input_queue.push_back((MatchInput) {
                                        .type = MATCH_INPUT_BUILDING_DEQUEUE,
                                        .building_dequeue = (MatchInputBuildingDequeue) {
                                            .building_id = shell->selection[0],
                                            .index = BUILDING_DEQUEUE_POP_FRONT
                                        }
                                    });
                                }
                            }
                            break;
                        }
                        case INPUT_HOTKEY_STOP:
                        case INPUT_HOTKEY_DEFEND: {
                            MatchInput input;
                            input.type = hotkey == INPUT_HOTKEY_STOP ? MATCH_INPUT_STOP : MATCH_INPUT_DEFEND;
                            input.stop.entity_count = (uint8_t)shell->selection.size();
                            memcpy(&input.stop.entity_ids, &shell->selection[0], input.stop.entity_count * sizeof(EntityId));
                            shell->input_queue.push_back(input);
                            break;
                        }
                        case INPUT_HOTKEY_REPAIR: {
                            shell->mode = MATCH_SHELL_MODE_TARGET_REPAIR;
                            break;
                        }
                        case INPUT_HOTKEY_UNLOAD: {
                            if (entity_is_unit(shell->match_state.entities.get_by_id(shell->selection[0]).type)) {
                                shell->mode = MATCH_SHELL_MODE_TARGET_UNLOAD;
                            } else {
                                MatchInput input;
                                input.type = MATCH_INPUT_UNLOAD;
                                input.unload.carrier_count = (uint8_t)shell->selection.size();
                                memcpy(&input.unload.carrier_ids, &shell->selection[0], input.unload.carrier_count * sizeof(EntityId));
                                shell->input_queue.push_back(input);
                            }
                            break;
                        }
                        case INPUT_HOTKEY_MOLOTOV: {
                            if (!match_shell_selection_has_enough_energy(shell, shell->selection, MOLOTOV_ENERGY_COST)) {
                                match_shell_show_status(shell, MATCH_UI_STATUS_NOT_ENOUGH_ENERGY);
                                break;
                            }
                            shell->mode = MATCH_SHELL_MODE_TARGET_MOLOTOV;
                            break;
                        }
                        case INPUT_HOTKEY_CAMO: {
                            bool activate = !entity_check_flag(shell->match_state.entities.get_by_id(shell->selection[0]), ENTITY_FLAG_INVISIBLE);
                            if (activate && !match_shell_selection_has_enough_energy(shell, shell->selection, CAMO_ENERGY_COST)) {
                                match_shell_show_status(shell, MATCH_UI_STATUS_NOT_ENOUGH_ENERGY);
                                break;
                            }
                            MatchInput input;
                            input.type = activate ? MATCH_INPUT_CAMO : MATCH_INPUT_DECAMO;
                            input.camo.unit_count = (uint8_t)shell->selection.size();
                            memcpy(&input.camo.unit_ids, &shell->selection[0], input.camo.unit_count * sizeof(EntityId));
                            shell->input_queue.push_back(input);
                            break;
                        }
                        default:
                            break;
                    }
                } else if (hotkey_info.type == HOTKEY_BUTTON_BUILD) {
                    const EntityData& building_data = entity_get_data(hotkey_info.entity_type);
                    bool costs_energy = (building_data.building_data.options & BUILDING_COSTS_ENERGY) == BUILDING_COSTS_ENERGY;
                    if (costs_energy && !match_shell_selection_has_enough_energy(shell, shell->selection, building_data.gold_cost)) {
                        match_shell_show_status(shell, MATCH_UI_STATUS_NOT_ENOUGH_ENERGY);
                    } else if (!costs_energy && shell->match_state.players[network_get_player_id()].gold < entity_get_data(hotkey_info.entity_type).gold_cost) {
                        match_shell_show_status(shell, MATCH_UI_STATUS_NOT_ENOUGH_GOLD);
                    } else {
                        shell->mode = MATCH_SHELL_MODE_BUILDING_PLACE;
                        shell->building_type = hotkey_info.entity_type;
                    }
                } else if (hotkey_info.type == HOTKEY_BUTTON_TRAIN || hotkey_info.type == HOTKEY_BUTTON_RESEARCH) {
                    // Build the queue item
                    BuildingQueueItem item = hotkey_info.type == HOTKEY_BUTTON_TRAIN
                                                ? (BuildingQueueItem) {
                                                    .type = BUILDING_QUEUE_ITEM_UNIT,
                                                    .unit_type = hotkey_info.entity_type
                                                }
                                                : (BuildingQueueItem) {
                                                    .type = BUILDING_QUEUE_ITEM_UPGRADE,
                                                    .upgrade = hotkey_info.upgrade
                                                };

                    // Check to make sure at least one building queue has room
                    bool are_building_queues_full = true;
                    for (EntityId building_id : shell->selection) {
                        const Entity& building = shell->match_state.entities.get_by_id(building_id);
                        if (building.queue.size() < BUILDING_QUEUE_MAX) {
                            are_building_queues_full = false;
                        }
                    }

                    if (are_building_queues_full) {
                        match_shell_show_status(shell, MATCH_UI_STATUS_BUILDING_QUEUE_FULL);
                    } else if (shell->match_state.players[network_get_player_id()].gold < building_queue_item_cost(item)) {
                        match_shell_show_status(shell, MATCH_UI_STATUS_NOT_ENOUGH_GOLD);
                    } else {
                        if (match_shell_is_in_hotkey_submenu(shell)) {
                            shell->mode = MATCH_SHELL_MODE_NONE;
                        }
                        MatchInput input;
                        input.type = MATCH_INPUT_BUILDING_ENQUEUE;
                        input.building_enqueue.item_type = (uint8_t)item.type;
                        input.building_enqueue.item_subtype = item.type == BUILDING_QUEUE_ITEM_UNIT
                                                                ? (uint32_t)item.unit_type
                                                                : item.upgrade;
                        input.building_enqueue.building_count = (uint8_t)shell->selection.size();
                        memcpy(&input.building_enqueue.building_ids, &shell->selection[0], shell->selection.size() * sizeof(EntityId));
                        shell->input_queue.push_back(input);
                    }
                }

                sound_play(SOUND_UI_CLICK);
                return;
            }
        }
    }

    // Selection list click
    if (shell->selection.size() > 1 && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        for (uint32_t selection_index = 0; selection_index < shell->selection.size(); selection_index++) {
            Rect icon_rect = match_shell_get_selection_list_item_rect(selection_index);
            if (icon_rect.has_point(input_get_mouse_position())) {
                if (input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                    shell->selection.erase(shell->selection.begin() + selection_index);
                } else {
                    std::vector<EntityId> selection;
                    selection.push_back(shell->selection[selection_index]);
                    match_shell_set_selection(shell, selection);
                }
                sound_play(SOUND_UI_CLICK);
                return;
            }
        }
    }

    // Building queue click
    if (shell->selection.size() == 1 && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !spectator_mode &&
            match_shell_is_camera_free(shell) &&
            !match_shell_is_selecting(shell)) {
        const Entity& building = shell->match_state.entities.get_by_id(shell->selection[0]);
        const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
        for (uint32_t building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
            Rect icon_rect = (Rect) {
                .x = BUILDING_QUEUE_POSITIONS[building_queue_index].x,
                .y = BUILDING_QUEUE_POSITIONS[building_queue_index].y,
                .w = icon_sprite_info.frame_width,
                .h = icon_sprite_info.frame_height
            };
            if (icon_rect.has_point(input_get_mouse_position())) {
                shell->input_queue.push_back((MatchInput) {
                    .type = MATCH_INPUT_BUILDING_DEQUEUE,
                    .building_dequeue = (MatchInputBuildingDequeue) {
                        .building_id = shell->selection[0],
                        .index = (uint8_t)building_queue_index
                    }
                });
                sound_play(SOUND_UI_CLICK);
                return;
            }
        }
    }

    // Garrisoned unit click
    if (shell->selection.size() == 1 && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !spectator_mode) {
        const Entity& carrier = shell->match_state.entities.get_by_id(shell->selection[0]);
        if (carrier.type == ENTITY_GOLDMINE || carrier.player_id == network_get_player_id()) {
            const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
            int index = 0;
            for (uint32_t garrisoned_units_index = 0; garrisoned_units_index < carrier.garrisoned_units.size(); garrisoned_units_index++) {
                EntityId entity_id = carrier.garrisoned_units[garrisoned_units_index];
                const Entity& garrisoned_unit = shell->match_state.entities.get_by_id(entity_id);
                // We have to make this check here because goldmines might have both allied and enemy units in them
                if (garrisoned_unit.player_id != network_get_player_id()) {
                    continue;
                }

                Rect icon_rect = (Rect) {
                    .x = GARRISON_ICON_POSITIONS[index].x,
                    .y = GARRISON_ICON_POSITIONS[index].y,
                    .w = icon_sprite_info.frame_width,
                    .h = icon_sprite_info.frame_height
                };
                if (icon_rect.has_point(input_get_mouse_position())) {
                    shell->input_queue.push_back((MatchInput) {
                        .type = MATCH_INPUT_SINGLE_UNLOAD,
                        .single_unload = (MatchInputSingleUnload) {
                            .entity_id = entity_id
                        }
                    });
                    sound_play(SOUND_UI_CLICK);
                    return;
                }

                index++;
            }
        }
    }

    // Order move
    if (!spectator_mode) {
        // Check that the left or right mouse button is being pressed
        InputAction action_required = match_shell_is_targeting(shell) ? INPUT_ACTION_LEFT_CLICK : INPUT_ACTION_RIGHT_CLICK;
        bool is_action_pressed = input_is_action_just_pressed(action_required);
        // Check that a move command can be executed in the current UI state
        bool is_movement_allowed = shell->mode != MATCH_SHELL_MODE_BUILDING_PLACE &&
                                    match_shell_is_camera_free(shell) &&
                                    match_shell_get_selection_type(shell, shell->selection)
                                        == MATCH_SHELL_SELECTION_UNITS;
        // Check that the mouse is either in the world or in the minimap
        bool is_mouse_in_position = !match_shell_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position());

        if (is_action_pressed && is_movement_allowed && is_mouse_in_position) {
            match_shell_order_move(shell);
            return;
        }
    }

    // Set building rally
    if (input_is_action_just_pressed(INPUT_ACTION_RIGHT_CLICK) &&
            match_shell_get_selection_type(shell, shell->selection) == MATCH_SHELL_SELECTION_BUILDINGS &&
            !spectator_mode &&
            match_shell_is_camera_free(shell) &&
            !(match_shell_is_selecting(shell) || match_shell_is_targeting(shell)) &&
            (!match_shell_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position()))) {
        // Check to make sure that all buildings can rally
        for (EntityId id : shell->selection) {
            const Entity& entity = shell->match_state.entities.get_by_id(id);
            if ((entity_get_data(entity.type).building_data.options & BUILDING_CAN_RALLY) != BUILDING_CAN_RALLY) {
                return;
            }
        }

        // Create rally input
        MatchInput input;
        input.type = MATCH_INPUT_RALLY;

        // Determine rally point
        if (match_shell_is_mouse_in_ui()) {
            ivec2 minimap_pos = input_get_mouse_position() - ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y);
            input.rally.rally_point = ivec2((shell->match_state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                                            (shell->match_state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        } else {
            input.rally.rally_point = input_get_mouse_position() + shell->camera_offset;
        }

        // Check if rallied onto gold
        Cell cell = map_get_cell(shell->match_state.map, CELL_LAYER_GROUND, input.rally.rally_point / TILE_SIZE);
        if (cell.type == CELL_GOLDMINE) {
            // If they rallied onto their gold, adjust their rally point and play an animation to clearly indicate that units will rally out to the mine
            // But only do this if the goldmine has been explored, otherwise we would reveal to players that there is a goldmine where they clicked
            const Entity& goldmine = shell->match_state.entities.get_by_id(cell.id);
            if (match_is_cell_rect_explored(shell->match_state, shell->match_state.players[network_get_player_id()].team, goldmine.cell, entity_get_data(goldmine.type).cell_size)) {
                input.rally.rally_point = (goldmine.cell * TILE_SIZE) + ivec2(23, 16);
                shell->move_animation = animation_create(ANIMATION_UI_MOVE_ENTITY);
                shell->move_animation_entity_id = cell.id;
                shell->move_animation_position = goldmine.cell * TILE_SIZE;
            }
        }

        // Add buildings to rally point input
        input.rally.building_count = shell->selection.size();
        memcpy(&input.rally.building_ids, &shell->selection[0], shell->selection.size() * sizeof(EntityId));

        shell->input_queue.push_back(input);
        sound_play(SOUND_FLAG_THUMP);

        return;
    }

    // Building placement
    if (shell->mode == MATCH_SHELL_MODE_BUILDING_PLACE &&
            !match_shell_is_mouse_in_ui() &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        if (!match_shell_building_can_be_placed(shell)) {
            match_shell_show_status(shell, MATCH_UI_STATUS_CANT_BUILD);
            return;
        }

        MatchInput input;
        input.type = MATCH_INPUT_BUILD;
        input.build.shift_command = input_is_action_pressed(INPUT_ACTION_SHIFT);
        input.build.building_type = shell->building_type;
        input.build.entity_count = shell->selection.size();
        memcpy(&input.build.entity_ids, &shell->selection[0], shell->selection.size() * sizeof(EntityId));
        input.build.target_cell = match_shell_get_building_cell(entity_get_data(shell->building_type).cell_size, shell->camera_offset);
        shell->input_queue.push_back(input);

        if (!input.build.shift_command) {
            shell->mode = MATCH_SHELL_MODE_NONE;
        }

        sound_play(SOUND_BUILDING_PLACE);
        return;
    }

    // Control group key press
    if (!spectator_mode && !input_is_text_input_active()) {
        uint32_t number_key_pressed;
        for (number_key_pressed = 0; number_key_pressed < 10; number_key_pressed++) {
            if (input_is_action_just_pressed((InputAction)(INPUT_ACTION_NUM1 + number_key_pressed))) {
                break;
            }
        }
        if (number_key_pressed != 10) {
            uint32_t control_group_index = number_key_pressed;
            // Set control group
            if (input_is_action_pressed(INPUT_ACTION_CTRL)) {
                if (shell->selection.empty() || shell->match_state.entities.get_by_id(shell->selection[0]).player_id != network_get_player_id()) {
                    return;
                }

                shell->control_groups[control_group_index] = shell->selection;
                shell->control_group_selected = control_group_index;
                return;
            }

            // Append control group
            if (input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                // Don't append if the selection is empty
                // Don't append if we are not selecting allied entities
                if (shell->selection.empty() || shell->match_state.entities.get_by_id(shell->selection[0]).player_id != network_get_player_id()) {
                    return;
                }
                // Don't append if the selection types are different
                // Unless the control group type is NONE, in which case it's okay
                MatchShellSelectionType control_group_selection_type = match_shell_get_selection_type(shell, shell->control_groups[control_group_index]);
                if (match_shell_get_selection_type(shell, shell->selection) != control_group_selection_type && control_group_selection_type != MATCH_SHELL_SELECTION_NONE) {
                    return;
                }

                // First, remove any dead entities from the existing control group
                uint32_t existing_control_group_index = 0;
                while (existing_control_group_index < shell->control_groups[control_group_index].size()) {
                    uint32_t entity_index = shell->match_state.entities.get_index_of(shell->control_groups[control_group_index][existing_control_group_index]);
                    if (entity_index == INDEX_INVALID || !entity_is_selectable(shell->match_state.entities[entity_index])) {
                        shell->control_groups[control_group_index][existing_control_group_index] = shell->control_groups[control_group_index].back();
                        shell->control_groups[control_group_index].pop_back();
                    } else {
                        existing_control_group_index++;
                    }
                }

                for (EntityId entity_id : shell->selection) {
                    if (shell->control_groups[control_group_index].size() == SELECTION_LIMIT) {
                        break;
                    }
                    if (std::find(shell->control_groups[control_group_index].begin(), shell->control_groups[control_group_index].end(), entity_id)
                            == shell->control_groups[control_group_index].end()) {
                        shell->control_groups[control_group_index].push_back(entity_id);
                    }
                }

                return;
            }

            // Snap to control group
            if (shell->control_group_double_tap_timer != 0 &&
                    shell->control_group_selected == control_group_index &&
                    !shell->selection.empty()) {
                std::vector<Rect> group_rects;
                std::vector<uint32_t> group_rect_entity_counts;
                for (uint32_t selection_index = 0; selection_index < shell->selection.size(); selection_index++) {
                    const Entity& entity = shell->match_state.entities.get_by_id(shell->selection[selection_index]);

                    const int entity_cell_size = entity_get_data(entity.type).cell_size;
                    Rect entity_rect = (Rect) {
                        .x = entity.cell.x,
                        .y = entity.cell.y,
                        .w = entity_cell_size,
                        .h = entity_cell_size
                    };

                    uint32_t existing_group_index;
                    for (existing_group_index = 0; existing_group_index < group_rects.size(); existing_group_index++) {
                        Rect group_rect = group_rects[existing_group_index];

                        // If the entity is inside this rect, then use it
                        if (group_rect.has_point(entity.cell)) {
                            break;
                        }

                        // If the entity is outside the rect but close to it, then use it
                        if (Rect::euclidean_distance_squared_between(entity_rect, group_rect) < 64) {
                            break;
                        }
                    }

                    if (existing_group_index < group_rects.size()) {
                        // If the unit is outside the rect, then extend the rect to contain the unit
                        Rect group_rect = group_rects[existing_group_index];
                        if (!group_rect.has_point(entity.cell)) {
                            group_rect.x = std::min(group_rect.x, entity_rect.x);
                            group_rect.y = std::min(group_rect.y, entity_rect.y);
                            int group_rect_max_x = std::max(group_rect.x + group_rect.w, entity_rect.x + entity_rect.w);
                            int group_rect_max_y = std::max(group_rect.y + group_rect.h, entity_rect.y + entity_rect.h);
                            group_rect.w = group_rect_max_x - group_rect.x;
                            group_rect.h = group_rect_max_y - group_rect.y;

                            group_rects[existing_group_index] = group_rect;
                        }

                        group_rect_entity_counts[existing_group_index]++;
                        continue;
                    }

                    // Otherwise, if the unit did not find a nearby rect, begin a new rect for the unit
                    group_rects.push_back(entity_rect);
                    group_rect_entity_counts.push_back(1);
                }

                // Chose the group rect with the most entities
                GOLD_ASSERT(!group_rects.empty() && group_rects.size() == group_rect_entity_counts.size());
                uint32_t most_populated_group_rect_index = 0;
                for (uint32_t group_rect_index = 1; group_rect_index < group_rects.size(); group_rect_index++) {
                    if (group_rect_entity_counts[group_rect_index] > group_rect_entity_counts[most_populated_group_rect_index]) {
                        most_populated_group_rect_index = group_rect_index;
                    }
                }

                // Snap to the group rect's center
                Rect group_rect = group_rects[most_populated_group_rect_index];
                ivec2 group_center = ivec2(group_rect.x + (group_rect.w / 2), group_rect.y + (group_rect.h / 2));
                map_clamp_cell(shell->match_state.map, group_center);
                match_shell_center_camera_on_cell(shell, group_center);
                return;
            }

            // Select control group
            if (!shell->control_groups[control_group_index].empty()) {
                match_shell_set_selection(shell, shell->control_groups[control_group_index]);
                if (!shell->selection.empty()) {
                    shell->control_group_double_tap_timer = MATCH_SHELL_DOUBLE_CLICK_DURATION;
                    shell->control_group_selected = control_group_index;
                }
            }
        }
    }

    // Jump to latest alert
    if (input_is_action_just_pressed(INPUT_ACTION_SPACE) &&
            shell->latest_alert_cell.x != -1 &&
            !input_is_text_input_active() &&
            !spectator_mode) {
        match_shell_center_camera_on_cell(shell, shell->latest_alert_cell);
    }

    // Camera hotkeys
    for (uint32_t index = 0; index < MATCH_SHELL_CAMERA_HOTKEY_COUNT; index++) {
        if (input_is_action_just_pressed((InputAction)(INPUT_ACTION_F1 + index))) {
            if (input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                shell->camera_hotkeys[index] = shell->camera_offset;
            } else if (shell->camera_hotkeys[index].x != -1) {
                shell->camera_offset = shell->camera_hotkeys[index];
            }
        }
    }
}

// HELPERS

void match_shell_show_status(MatchShell* shell, const char* message) {
    const uint32_t STATUS_DURATION = 60U;
    shell->status_message = std::string(message);
    shell->status_timer = STATUS_DURATION;
}

void match_shell_order_move(MatchShell* shell) {
    // Determine move target
    ivec2 move_target;
    if (match_shell_is_mouse_in_ui()) {
        ivec2 minimap_pos = input_get_mouse_position() - ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y);
        move_target = ivec2((shell->match_state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                            (shell->match_state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
    } else {
        move_target = input_get_mouse_position() + shell->camera_offset;
    }

    // Create move input
    MatchInput input;
    input.move.shift_command = input_is_action_pressed(INPUT_ACTION_SHIFT);
    input.move.target_cell = move_target / TILE_SIZE;
    input.move.target_id = ID_NULL;
    EntityId remembered_entity_id = ID_NULL;
    bool input_move_target_is_balloon = false;

    // Checks if clicked on entity
    const uint8_t player_team = shell->match_state.players[network_get_player_id()].team;
    int fog_value = match_get_fog(shell->match_state, player_team, input.move.target_cell);
    // If clicked on hidden fog or the minimap, then don't check for entity click
    if (fog_value != FOG_HIDDEN && !match_shell_is_mouse_in_ui()) {
        for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
            const Entity& entity = shell->match_state.entities[entity_index];
            // Can't select units under explored fog
            if (fog_value == FOG_EXPLORED && entity_is_unit(entity.type)) {
                continue;
            }
            // Non-units *can* be selected under explored fog, but only if we have explored that the building exists
            if (fog_value == FOG_EXPLORED && !match_team_remembers_entity(shell->match_state, player_team, shell->match_state.entities.get_id_of(entity_index))) {
                continue;
            }
            // Don't target unselectable units
            if (!entity_is_selectable(entity)) {
                continue;
            }
            // Don't target invisible units (unless we have detection)
            if (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && shell->match_state.detection[player_team][input.move.target_cell.x + (input.move.target_cell.y * shell->match_state.map.width)] == 0) {
                continue;
            }

            Rect entity_rect = entity_get_rect(shell->match_state.entities[entity_index]);
            if (entity_rect.has_point(move_target)) {
                if (input.move.target_id == ID_NULL ||
                        (shell->match_state.entities[entity_index].type == ENTITY_BALLOON && !input_move_target_is_balloon)) {
                    input.move.target_id = shell->match_state.entities.get_id_of(entity_index);
                    input_move_target_is_balloon = shell->match_state.entities[entity_index].type == ENTITY_BALLOON;
                }
                if (fog_value == FOG_EXPLORED) {
                    remembered_entity_id = shell->match_state.entities.get_id_of(entity_index);
                }
            }
        }
    }

    if (shell->mode == MATCH_SHELL_MODE_TARGET_UNLOAD) {
        input.type = MATCH_INPUT_MOVE_UNLOAD;
    } else if (shell->mode == MATCH_SHELL_MODE_TARGET_REPAIR) {
        input.type = MATCH_INPUT_MOVE_REPAIR;
    } else if (shell->mode == MATCH_SHELL_MODE_TARGET_MOLOTOV) {
        input.type = MATCH_INPUT_MOVE_MOLOTOV;
    } else if (input.move.target_id != ID_NULL && !entity_is_misc(shell->match_state.entities.get_by_id(input.move.target_id).type) &&
            (shell->mode == MATCH_SHELL_MODE_TARGET_ATTACK || match_shell_is_entity_an_enemy_of_player(shell, input.move.target_id))) {
        input.type = MATCH_INPUT_MOVE_ATTACK_ENTITY;
    } else if (input.move.target_id == ID_NULL && shell->mode == MATCH_SHELL_MODE_TARGET_ATTACK) {
        input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
    } else if (input.move.target_id != ID_NULL) {
        input.type = MATCH_INPUT_MOVE_ENTITY;
    } else {
        input.type = MATCH_INPUT_MOVE_CELL;
    }

    // Populate move input entity ids
    input.move.entity_count = shell->selection.size();
    memcpy(input.move.entity_ids, &shell->selection[0], shell->selection.size() * sizeof(EntityId));

    if (input.type == MATCH_INPUT_MOVE_REPAIR) {
        bool is_repair_target_valid = true;
        if (input.move.target_id == ID_NULL) {
            is_repair_target_valid = false;
        } else {
            const Entity& repair_target = shell->match_state.entities.get_by_id(input.move.target_id);
            if (shell->match_state.players[repair_target.player_id].team != shell->match_state.players[network_get_player_id()].team ||
                    !entity_is_building(repair_target.type)) {
                is_repair_target_valid = false;
            }
        }

        if (!is_repair_target_valid) {
            match_shell_show_status(shell, MATCH_UI_STATUS_REPAIR_TARGET_INVALID);
            shell->mode = MATCH_SHELL_MODE_NONE;
            return;
        }
    }

    shell->input_queue.push_back(input);

    // Play animation
    if (remembered_entity_id != ID_NULL) {
        // Find remembered entity index
        uint32_t remembered_entity_index;
        for (remembered_entity_index = 0; remembered_entity_index < shell->match_state.remembered_entities[player_team].size(); remembered_entity_index++) {
            if (shell->match_state.remembered_entities[player_team][remembered_entity_index].entity_id == remembered_entity_id) {
                break;
            }
        }
        GOLD_ASSERT(remembered_entity_index != shell->match_state.remembered_entities[player_team].size());

        EntityType remembered_entity_type = shell->match_state.remembered_entities[player_team][remembered_entity_index].type;
        shell->move_animation = animation_create(entity_is_misc(remembered_entity_type)
                                                        ? ANIMATION_UI_MOVE_ENTITY
                                                        : ANIMATION_UI_MOVE_ATTACK_ENTITY);
        shell->move_animation_position = cell_center(input.move.target_cell).to_ivec2();
        shell->move_animation_entity_id = remembered_entity_id;
    } else if (input.type == MATCH_INPUT_MOVE_CELL || input.type == MATCH_INPUT_MOVE_ATTACK_CELL || input.type == MATCH_INPUT_MOVE_UNLOAD || input.type == MATCH_INPUT_MOVE_MOLOTOV) {
        shell->move_animation = animation_create(ANIMATION_UI_MOVE_CELL);
        shell->move_animation_position = move_target;
        shell->move_animation_entity_id = ID_NULL;
    } else {
        shell->move_animation = animation_create(input.type == MATCH_INPUT_MOVE_ATTACK_ENTITY ? ANIMATION_UI_MOVE_ATTACK_ENTITY : ANIMATION_UI_MOVE_ENTITY);
        shell->move_animation_position = cell_center(input.move.target_cell).to_ivec2();
        shell->move_animation_entity_id = input.move.target_id;
    }

    // Reset UI mode if targeting
    if (match_shell_is_targeting(shell) && !input_is_action_pressed(INPUT_ACTION_SHIFT)) {
        shell->mode = MATCH_SHELL_MODE_NONE;
    }
}

bool match_shell_is_entity_an_enemy_of_player(const MatchShell* shell, EntityId entity_id) {
    const Entity& entity = shell->match_state.entities.get_by_id(entity_id);
    if (entity_is_misc(entity.type)) {
        return false;
    }
    return shell->match_state.players[entity.player_id].team !=
        shell->match_state.players[network_get_player_id()].team;
}

bool match_shell_does_player_meet_hotkey_requirements(const MatchState& state, InputHotkey hotkey) {
    const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey);

    switch (hotkey_info.requirements.type) {
        case HOTKEY_REQUIRES_NONE: {
            return true;
        }
        case HOTKEY_REQUIRES_BUILDING: {
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                const Entity& entity = state.entities[entity_index];
                if (entity.player_id == network_get_player_id() && entity.type == hotkey_info.requirements.building && entity.mode == MODE_BUILDING_FINISHED) {
                    return true;
                }
            }
            return false;
        }
        case HOTKEY_REQUIRES_UPGRADE: {
            return match_player_has_upgrade(state, network_get_player_id(), hotkey_info.requirements.upgrade);
        }
    }
}

bool match_shell_is_hotkey_available(const MatchShell* shell, const HotkeyButtonInfo& info) {
    if ((info.type == HOTKEY_BUTTON_TRAIN || info.type == HOTKEY_BUTTON_BUILD) &&
            !shell->scenario_allowed_entities[info.entity_type]) {
        return false;
    }
    if (info.type == HOTKEY_BUTTON_RESEARCH &&
            (!match_player_upgrade_is_available(shell->match_state, network_get_player_id(), info.upgrade) ||
             (shell->scenario_allowed_upgrades & info.upgrade) != info.upgrade)) {
        return false;
    }

    return true;
}

uint32_t match_shell_get_player_entity_count(const MatchShell* shell, uint8_t player_id, EntityType entity_type) {
    uint32_t count = 0;

    for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
        const Entity& entity = shell->match_state.entities[entity_index];
        if (entity.type != entity_type ||
                entity.player_id != player_id ||
                entity.health == 0 ||
                entity.mode == MODE_BUILDING_IN_PROGRESS) {
            continue;
        }
        count++;
    }

    return count;
}

uint32_t match_shell_update_displayed_gold_amount(uint32_t current_displayed_value, uint32_t current_actual_value) {
    if (current_displayed_value == current_actual_value) {
        return current_displayed_value;
    }

    int difference = std::abs((int)current_displayed_value - (int)current_actual_value);
    int step = 1;
    if (difference > 100) {
        step = 10;
    } else if (difference > 10) {
        step = 5;
    } else if (difference > 1) {
        step = 2;
    }
    if (current_displayed_value < current_actual_value) {
        return current_displayed_value + step;
    } else {
        return current_displayed_value - step;
    }
}

void match_shell_leave_match(MatchShell* shell, MatchShellMode mode) {
    if (shell->replay_mode) {
        SDL_LockMutex(shell->replay_loading_early_exit_mutex);
        shell->replay_loading_early_exit = true;
        SDL_UnlockMutex(shell->replay_loading_early_exit_mutex);

        SDL_WaitThread(shell->replay_loading_thread, NULL);

        SDL_DestroyMutex(shell->replay_loading_mutex);
        SDL_DestroyMutex(shell->replay_loading_early_exit_mutex);
    } else {
        network_disconnect();
        match_shell_replay_file_close(shell->replay_file);
    }

    if (shell->scenario_lua_state != NULL) {
        lua_close(shell->scenario_lua_state);
    }

    shell->mode = mode;
}

// STATE QUERIES

bool match_shell_is_mouse_in_ui() {
    ivec2 mouse_position = input_get_mouse_position();
    return (mouse_position.y >= SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) ||
           (mouse_position.x <= 136 && mouse_position.y >= SCREEN_HEIGHT - 136) ||
           (mouse_position.x >= SCREEN_WIDTH - 132 && mouse_position.y >= SCREEN_HEIGHT - 106);
}

bool match_shell_is_selecting(const MatchShell* shell) {
    return shell->select_origin.x != -1;
}

bool match_shell_is_in_menu(const MatchShell* shell) {
    return shell->mode == MATCH_SHELL_MODE_MENU ||
            shell->mode == MATCH_SHELL_MODE_MENU_SURRENDER ||
            shell->mode == MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP ||
            shell->mode == MATCH_SHELL_MODE_MENU_SURRENDER_RESTART ||
            shell->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY ||
            shell->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT ||
            shell->mode == MATCH_SHELL_MODE_SCENARIO_VICTORY ||
            shell->mode == MATCH_SHELL_MODE_SCENARIO_DEFEAT ||
            shell->mode == MATCH_SHELL_MODE_DESYNC;
}

bool match_shell_is_in_hotkey_submenu(const MatchShell* shell) {
    return shell->mode == MATCH_SHELL_MODE_BUILD || shell->mode == MATCH_SHELL_MODE_BUILD2;
}

bool match_shell_is_targeting(const MatchShell* shell) {
    return shell->mode >= MATCH_SHELL_MODE_TARGET_ATTACK && shell->mode < MATCH_SHELL_MODE_CHAT;
}

bool match_shell_is_in_leave_match_mode(const MatchShell* shell) {
    return shell->mode == MATCH_SHELL_MODE_LEAVE_MATCH ||
        shell->mode == MATCH_SHELL_MODE_EXIT_PROGRAM ||
        shell->mode == MATCH_SHELL_MODE_LEAVE_SCENARIO_VICTORY ||
        shell->mode == MATCH_SHELL_MODE_LEAVE_SCENARIO_DEFEAT ||
        shell->mode == MATCH_SHELL_MODE_LEAVE_SCENARIO_RESTART;
}

bool match_shell_is_at_least_one_opponent_in_match(const MatchShell* shell) {
    return match_player_has_at_least_one_active_opponent(shell->match_state, network_get_player_id());
}

bool match_shell_is_in_single_player_game() {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (player_id == network_get_player_id()) {
            continue;
        }
        if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_READY) {
            return false;
        }
    }

    return true;
}

bool match_shell_is_surrender_required_to_leave(const MatchShell* shell) {
    if (shell->replay_mode) {
        return false;
    }
    return shell->match_state.players[network_get_player_id()].mode == PLAYER_MODE_ACTIVE &&
        match_shell_is_at_least_one_opponent_in_match(shell);
}

// CAMERA

void match_shell_camera_update(MatchShell* shell) {
    const int CAMERA_DRAG_MARGIN = 4;

    if (!match_shell_is_selecting(shell) && match_shell_is_camera_free(shell) && shell->camera_pan_timer == 0) {
        ivec2 camera_drag_direction = ivec2(0, 0);
        if (input_get_mouse_position().x < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = -1;
        } else if (input_get_mouse_position().x > SCREEN_WIDTH - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = 1;
        }
        if (input_get_mouse_position().y < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = -1;
        } else if (input_get_mouse_position().y > SCREEN_HEIGHT - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = 1;
        }
        shell->camera_offset += camera_drag_direction * option_get_value(OPTION_CAMERA_SPEED);
        match_shell_clamp_camera(shell);
    }

    // Begin minimap drag
    if (MINIMAP_RECT.has_point(input_get_mouse_position()) &&
            !match_shell_is_targeting(shell) &&
            !match_shell_is_selecting(shell) &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !match_shell_is_in_menu(shell) &&
            match_shell_is_camera_free(shell)) {
        shell->camera_mode = CAMERA_MODE_MINIMAP_DRAG;
    }

    // End minimap drag
    if (shell->camera_mode == CAMERA_MODE_MINIMAP_DRAG && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        shell->camera_mode = CAMERA_MODE_FREE;
    }

    // During minimap drag
    if (shell->camera_mode == CAMERA_MODE_MINIMAP_DRAG) {
        ivec2 minimap_pos = ivec2(
            std::clamp(input_get_mouse_position().x - MINIMAP_RECT.x, 0, MINIMAP_RECT.w),
            std::clamp(input_get_mouse_position().y - MINIMAP_RECT.y, 0, MINIMAP_RECT.h));
        ivec2 map_pos = ivec2(
            (shell->match_state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (shell->match_state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        match_shell_center_camera_on_cell(shell, map_pos / TILE_SIZE);
    }

    // Camera pan
    if (shell->camera_mode == CAMERA_MODE_PAN) {
        shell->camera_pan_timer--;

        if (shell->camera_pan_timer == 0) {
            shell->camera_offset = shell->camera_pan_end_offset;
            shell->camera_mode = CAMERA_MODE_FREE;
            shell->camera_shake_seed = 0;
        } else {
            float percent =
                (float)(shell->camera_pan_duration - shell->camera_pan_timer) /
                (float)shell->camera_pan_duration;
            ivec2 difference = shell->camera_pan_end_offset - shell->camera_pan_start_offset;
            shell->camera_offset = shell->camera_pan_start_offset + ivec2(
                (int)((float)difference.x * percent),
                (int)((float)difference.y * percent)
            );

            if (shell->camera_shake_seed != 0) {
                double CAMERA_SHAKE_FREQUENCY = 32.0;
                float shake_percentage =
                    (float)(shell->camera_pan_duration - shell->camera_pan_timer) /
                    (float)shell->camera_pan_duration;
                double noise_result_x = simplex_noise((uint64_t)shell->camera_shake_seed, shake_percentage * CAMERA_SHAKE_FREQUENCY, shake_percentage * CAMERA_SHAKE_FREQUENCY);
                double noise_result_y = simplex_noise((uint64_t)shell->camera_shake_seed, shake_percentage * CAMERA_SHAKE_FREQUENCY * 0.5, shake_percentage * CAMERA_SHAKE_FREQUENCY * 0.5);
                ivec2 shake_offset = ivec2((int)(3 * noise_result_x), (int)(3 * noise_result_y));
                shell->camera_offset += shake_offset;
                match_shell_clamp_camera(shell);
            }
        }
    }
}

void match_shell_clamp_camera(MatchShell* shell) {
    shell->camera_offset.x = std::clamp(shell->camera_offset.x, 0, (shell->match_state.map.width * TILE_SIZE) - SCREEN_WIDTH);
    shell->camera_offset.y = std::clamp(shell->camera_offset.y, 0, (shell->match_state.map.height * TILE_SIZE) - SCREEN_HEIGHT + MATCH_SHELL_UI_HEIGHT);
}

void match_shell_center_camera_on_cell(MatchShell* shell, ivec2 cell) {
    shell->camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    shell->camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2);
    match_shell_clamp_camera(shell);
}

bool match_shell_is_camera_free(const MatchShell* shell) {
    return shell->camera_mode == CAMERA_MODE_FREE;
}

void match_shell_begin_camera_pan(MatchShell* shell, ivec2 end_cell, double duration, bool shake) {
    // Convert to cell into camera offset
    shell->camera_pan_end_offset.x = (end_cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    shell->camera_pan_end_offset.y = (end_cell.y * TILE_SIZE) + (TILE_SIZE / 2) - ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2);
    shell->camera_pan_end_offset.x = std::clamp(shell->camera_pan_end_offset.x, 0, (shell->match_state.map.width * TILE_SIZE) - SCREEN_WIDTH);
    shell->camera_pan_end_offset.y = std::clamp(shell->camera_pan_end_offset.y, 0, (shell->match_state.map.height * TILE_SIZE) - SCREEN_HEIGHT + MATCH_SHELL_UI_HEIGHT);

    shell->camera_pan_start_offset = shell->camera_offset;
    shell->camera_pan_timer = (uint32_t)(duration * (double)UPDATES_PER_SECOND);
    shell->camera_pan_duration = shell->camera_pan_timer;

    if (shake) {
        shell->camera_shake_seed = rand();
        if (shell->camera_shake_seed == 0) {
            shell->camera_shake_seed = 1;
        }
    }

    shell->camera_mode = CAMERA_MODE_PAN;
}

// SELECTION

void match_shell_selection_update(MatchShell* shell) {
    EntityList idle_miners = match_shell_find_idle_miners(shell);
    if (match_shell_is_idle_miner_button_pressed(shell, idle_miners)) {
        match_shell_select_idle_miner(shell, idle_miners);
        return;
    }

    // Begin selecting
    if (!match_shell_is_mouse_in_ui() &&
            !(match_shell_has_pressed_idle_miner_button() && !idle_miners.empty()) &&
            (shell->mode == MATCH_SHELL_MODE_NONE || match_shell_is_in_hotkey_submenu(shell)) &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            match_shell_is_camera_free(shell)) {
        shell->select_origin = input_get_mouse_position() + shell->camera_offset;
    }

    // End selecting
    if (match_shell_is_selecting(shell) && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        ivec2 world_pos = input_get_mouse_position() + shell->camera_offset;
        Rect select_rect = (Rect) {
            .x = std::min(world_pos.x, shell->select_origin.x),
            .y = std::min(world_pos.y, shell->select_origin.y),
            .w = std::max(1, std::abs(shell->select_origin.x - world_pos.x)),
            .h = std::max(1, std::abs(shell->select_origin.y - world_pos.y)),
        };

        std::vector<EntityId> selection = match_shell_create_selection(shell, select_rect);
        shell->select_origin = ivec2(-1, -1);
        shell->control_group_selected = MATCH_SHELL_CONTROL_GROUP_NONE;

        // Append selection
        if (input_is_action_pressed(INPUT_ACTION_SHIFT)) {
            MatchShellSelectionType current_selection_type = match_shell_get_selection_type(shell, shell->selection);
            // Don't append selection if selection is not allied units or buildings
            if (!(current_selection_type == MATCH_SHELL_SELECTION_UNITS || current_selection_type == MATCH_SHELL_SELECTION_BUILDINGS)) {
                return;
            }
            // Don't append selection if units are not same type
            if (match_shell_get_selection_type(shell, selection) != current_selection_type) {
                return;
            }
            for (EntityId incoming_id : selection) {
                if (shell->selection.size() == SELECTION_LIMIT) {
                    break;
                }
                if (std::find(shell->selection.begin(), shell->selection.end(), incoming_id) == shell->selection.end()) {
                    shell->selection.push_back(incoming_id);
                }
            }

            match_shell_set_selection(shell, shell->selection);
            return;
        }

        if (selection.size() == 1) {
            if ((input_is_action_pressed(INPUT_ACTION_CTRL) ||
                    (shell->double_click_timer != 0 && shell->selection.size() == 1 && shell->selection[0] == selection[0])) &&
                    shell->match_state.entities.get_by_id(selection[0]).player_id == network_get_player_id()) {
                EntityType selected_type = shell->match_state.entities.get_by_id(selection[0]).type;
                selection.clear();

                for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
                    if (shell->match_state.entities[entity_index].type != selected_type || shell->match_state.entities[entity_index].player_id != network_get_player_id()) {
                        continue;
                    }

                    Rect entity_rect = entity_get_rect(shell->match_state.entities[entity_index]);
                    entity_rect.x -= shell->camera_offset.x;
                    entity_rect.y -= shell->camera_offset.y;
                    if (SCREEN_RECT.intersects(entity_rect)) {
                        selection.push_back(shell->match_state.entities.get_id_of(entity_index));
                    }
                }
            } else if (shell->double_click_timer == 0 && !input_is_action_pressed(INPUT_ACTION_CTRL)) {
                shell->double_click_timer = MATCH_SHELL_DOUBLE_CLICK_DURATION;
            }
        }

        match_shell_set_selection(shell, selection);
        return;
    }
}

std::vector<EntityId> match_shell_create_selection(const MatchShell* shell, Rect select_rect) {
    std::vector<EntityId> selection;

    // Select player units
    for (uint32_t index = 0; index < shell->match_state.entities.size(); index++) {
        const Entity& entity = shell->match_state.entities[index];
        // Select only selectable units
        if (!entity_is_unit(entity.type) || !entity_is_selectable(entity)) {
            continue;
        }
        // In non-replay mode, select only player units
        if (!shell->replay_mode && entity.player_id != network_get_player_id()) {
            continue;
        }
        // In replay mode, select only visible units
        if (shell->replay_mode && !match_shell_is_entity_visible(shell, entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(shell->match_state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select player buildings
    for (uint32_t index = 0; index < shell->match_state.entities.size(); index++) {
        const Entity& entity = shell->match_state.entities[index];
        // Select only selectable buildings
        if (!entity_is_building(entity.type) || !entity_is_selectable(entity)) {
            continue;
        }
        // In non-replay mode, select only player buildings
        if (!shell->replay_mode && entity.player_id != network_get_player_id()) {
            continue;
        }
        // In replay mode, select only visible units
        if (shell->replay_mode && !match_shell_is_entity_visible(shell, entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(shell->match_state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    if (!shell->replay_mode) {
        // Select enemy units
        for (uint32_t index = 0; index < shell->match_state.entities.size(); index++) {
            const Entity& entity = shell->match_state.entities[index];
            if (entity.player_id == network_get_player_id() ||
                    !entity_is_unit(entity.type) ||
                    !entity_is_selectable(entity) ||
                    !match_shell_is_entity_visible(shell, entity)) {
                continue;
            }

            Rect entity_rect = entity_get_rect(entity);
            if (entity_rect.intersects(select_rect)) {
                selection.push_back(shell->match_state.entities.get_id_of(index));
                return selection;
            }
        }

        // Select enemy buildings
        for (uint32_t index = 0; index < shell->match_state.entities.size(); index++) {
            const Entity& entity = shell->match_state.entities[index];
            if (entity.player_id == network_get_player_id() ||
                    entity_is_unit(entity.type) ||
                    !entity_is_selectable(entity) ||
                    !match_shell_is_entity_visible(shell, entity)) {
                continue;
            }

            Rect entity_rect = entity_get_rect(entity);
            if (entity_rect.intersects(select_rect)) {
                selection.push_back(shell->match_state.entities.get_id_of(index));
                return selection;
            }
        }
    }

    // Select misc
    for (uint32_t index = 0; index < shell->match_state.entities.size(); index++) {
        const Entity& entity = shell->match_state.entities[index];
        if (!entity_is_misc(entity.type) ||
                !match_shell_is_entity_visible(shell, entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(shell->match_state.entities.get_id_of(index));
            return selection;
        }
    }

    // Returns an empty selection
    return selection;
}

void match_shell_set_selection(MatchShell* shell, std::vector<EntityId>& selection) {
    shell->selection = selection;

    for (uint32_t selection_index = 0; selection_index < shell->selection.size(); selection_index++) {
        uint32_t entity_index = shell->match_state.entities.get_index_of(shell->selection[selection_index]);
        if (entity_index == INDEX_INVALID || !match_shell_is_entity_selectable(shell, shell->match_state.entities[entity_index])) {
            shell->selection.erase(shell->selection.begin() + selection_index);
            selection_index--;
            continue;
        }
    }

    while (shell->selection.size() > SELECTION_LIMIT) {
        shell->selection.pop_back();
    }

    shell->mode = MATCH_SHELL_MODE_NONE;
}

MatchShellSelectionType match_shell_get_selection_type(const MatchShell* shell, const std::vector<EntityId>& selection) {
    // Find the first non-dead unit
    uint32_t first_entity_index = INDEX_INVALID;
    for (EntityId entity_id : selection) {
        uint32_t entity_index = shell->match_state.entities.get_index_of(entity_id);
        if (entity_index != INDEX_INVALID) {
            first_entity_index = entity_index;
            break;
        }
    }

    if (first_entity_index == INDEX_INVALID) {
        return MATCH_SHELL_SELECTION_NONE;
    }

    const Entity& entity = shell->match_state.entities[first_entity_index];
    if (entity_is_unit(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return MATCH_SHELL_SELECTION_UNITS;
        } else {
            return MATCH_SHELL_SELECTION_ENEMY_UNIT;
        }
    } else if (entity_is_building(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return MATCH_SHELL_SELECTION_BUILDINGS;
        } else {
            return MATCH_SHELL_SELECTION_ENEMY_BUILDING;
        }
    } else {
        return MATCH_SHELL_SELECTION_GOLD;
    }
}

bool match_shell_selection_has_enough_energy(const MatchShell* shell, const std::vector<EntityId>& selection, uint32_t cost) {
    for (EntityId id : selection) {
        if (shell->match_state.entities.get_by_id(id).energy >= cost) {
            return true;
            break;
        }
    }

    return false;
}

bool match_shell_is_entity_player_controlled_and_in_goldmine(const MatchShell* shell, const Entity& entity) {
    return (!shell->replay_mode &&
            entity.health != 0 &&
            entity.player_id == network_get_player_id() &&
            entity_is_in_mine(shell->match_state, entity));
}

bool match_shell_is_entity_selectable(const MatchShell* shell, const Entity& entity) {
    // Special-case for allowing players to select
    // their own miners that have entered a goldmine
    if (match_shell_is_entity_player_controlled_and_in_goldmine(shell, entity)) {
        return true;
    }

    return entity_is_selectable(entity);
}

bool match_shell_can_keep_selecting_entity(const MatchShell* shell, const Entity& entity) {
    if (match_shell_is_entity_player_controlled_and_in_goldmine(shell, entity)) {
        return true;
    }

    return entity_is_selectable(entity) && match_shell_is_entity_visible(shell, entity);
}

// IDLE MINER BUTTON

EntityList match_shell_find_idle_miners(const MatchShell* shell) {
    return match_find_entities(shell->match_state, [](const Entity& entity, EntityId /*miner_id*/) {
        return entity_is_idle_miner(entity) && entity.player_id == network_get_player_id();
    });
}

bool match_shell_is_idle_miner_button_pressed(const MatchShell* shell, const EntityList& idle_miners) {
    if (shell->replay_mode) {
        return false;
    }
    if (!match_shell_is_camera_free(shell)) {
        return false;
    }
    if (match_shell_is_selecting(shell)) {
        return false;
    }
    if (idle_miners.empty()) {
        return false;
    }

    if (input_is_hotkey_just_pressed(INPUT_HOTKEY_IDLE_MINER)) {
        return true;
    }
    if (match_shell_get_idle_miner_button_rect().has_point(input_get_mouse_position()) && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        return true;
    }
    return false;
}

void match_shell_select_idle_miner(MatchShell* shell, const EntityList& idle_miners) {
    uint32_t miner_to_select_index = 0;
    if (shell->selection.size() == 1) {
        uint32_t miner_index;
        for (miner_index = 0; miner_index < idle_miners.size(); miner_index++) {
            if (idle_miners[miner_index] == shell->selection[0]) {
                break;
            }
        }
        if (miner_index < idle_miners.size()) {
            miner_to_select_index = (miner_index + 1) % idle_miners.size();
        }
    }

    EntityId miner_id = idle_miners[miner_to_select_index];
    std::vector<EntityId> miner_selection = { miner_id };
    match_shell_set_selection(shell, miner_selection);
    match_shell_center_camera_on_cell(shell, shell->match_state.entities.get_by_id(miner_id).cell);
}

// VISION

bool match_shell_is_entity_visible(const MatchShell* shell, const Entity& entity) {
    if (shell->replay_mode) {
        if (shell->replay_fog_index == REPLAY_FOG_NONE) {
            return 1;
        }
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if ((shell->replay_fog_player_ids[shell->replay_fog_index] == PLAYER_NONE ||
                    shell->replay_fog_player_ids[shell->replay_fog_index] == player_id) &&
                    entity_is_visible_to_player(shell->match_state, entity, player_id)) {
                return true;
            }
        }

        return false;
    } else {
        #ifdef GOLD_DEBUG
            if (shell->debug_fog_level == MATCH_SHELL_FOG_BOT_VISION) {
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT &&
                            entity_is_visible_to_player(shell->match_state, entity, player_id)) {
                        return true;
                    }
                }
            } else if (shell->debug_fog_level == MATCH_SHELL_FOG_DISABLED) {
                return entity.garrison_id == ID_NULL;
            }
        #endif

        return entity_is_visible_to_player(shell->match_state, entity, network_get_player_id());
    }
}

bool match_shell_is_cell_rect_revealed(const MatchShell* shell, ivec2 cell, int cell_size) {
    if (shell->replay_mode) {
        if (shell->replay_fog_index == REPLAY_FOG_NONE) {
            return true;
        }
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if ((shell->replay_fog_player_ids[shell->replay_fog_index] == PLAYER_NONE ||
                    shell->replay_fog_player_ids[shell->replay_fog_index] == player_id) &&
                    match_is_cell_rect_revealed(shell->match_state, shell->match_state.players[player_id].team, cell, cell_size)) {
                return true;
            }
        }

        return false;
    } else {
        #ifdef GOLD_DEBUG
            if (shell->debug_fog_level == MATCH_SHELL_FOG_BOT_VISION) {
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT &&
                            match_is_cell_rect_revealed(shell->match_state, shell->match_state.players[player_id].team, cell, cell_size)) {
                        return true;
                    }
                }
            } else if (shell->debug_fog_level == MATCH_SHELL_FOG_DISABLED) {
                return true;
            }
        #endif

        return match_is_cell_rect_revealed(shell->match_state, shell->match_state.players[network_get_player_id()].team, cell, cell_size);
    }
}

int match_shell_get_fog(const MatchShell* shell, ivec2 cell) {
    if (shell->replay_mode) {
        if (shell->replay_fog_index == REPLAY_FOG_NONE) {
            return 1;
        }
        int fog_value = FOG_HIDDEN;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (shell->replay_fog_player_ids[shell->replay_fog_index] == PLAYER_NONE ||
                    shell->replay_fog_player_ids[shell->replay_fog_index] == player_id) {
                int player_fog_value = match_get_fog(shell->match_state, shell->match_state.players[player_id].team, cell);
                // If at least one player has revealed fog, then return a revealed fog value
                if (player_fog_value > 0) {
                    return 1;
                }

                // If set the returned value to explored, but keep iterating in case we get a revealed value
                if (player_fog_value == FOG_EXPLORED) {
                    fog_value = FOG_EXPLORED;
                }
            }
        }

        return fog_value;
    } else {
        #ifdef GOLD_DEBUG
            if (shell->debug_fog_level == MATCH_SHELL_FOG_BOT_VISION) {
                int fog_value = FOG_HIDDEN;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if ((network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT || network_get_player_id() == player_id)) {
                        int player_fog_value = match_get_fog(shell->match_state, shell->match_state.players[player_id].team, cell);

                        // If at least one player has revealed fog, then return a revealed fog value
                        if (player_fog_value > 0) {
                            return 1;
                        }
                        // If set the returned value to explored, but keep iterating in case we get a revealed value
                        if (player_fog_value == FOG_EXPLORED) {
                            fog_value = FOG_EXPLORED;
                        }
                    }
                }
                return fog_value;
            } else if (shell->debug_fog_level == MATCH_SHELL_FOG_DISABLED) {
                return 1;
            }
        #endif

        return match_get_fog(shell->match_state, shell->match_state.players[network_get_player_id()].team, cell);
    }
}

// CHAT

void match_shell_get_player_prefix(const MatchShell* state, uint8_t player_id, char* prefix) {
    sprintf(prefix, "%s:", state->match_state.players[player_id].name);
}

FontName match_shell_get_player_font(uint8_t player_id) {
    return (FontName)(FONT_HACK_PLAYER0 + player_id);
}

void match_shell_add_chat_message(MatchShell* shell, FontName prefix_font, const char* prefix, const char* message, uint32_t duration) {
    ChatMessage chat_message;
    chat_message.prefix_font = prefix_font;
    chat_message.timer = duration;
    strncpy(chat_message.prefix, prefix, MATCH_SHELL_CHAT_PREFIX_BUFFER_SIZE);
    strncpy(chat_message.message, message, MATCH_SHELL_CHAT_MESSAGE_BUFFER_SIZE);
    shell->chat.push_back(chat_message);

    match_shell_replay_file_write_entry(shell->replay_file, (ReplayEntry) {
        .type = REPLAY_ENTRY_CHAT,
        .chat_message = chat_message
    });
}

void match_shell_handle_player_disconnect(MatchShell* shell, uint8_t player_id) {
    char message[128];
    sprintf(message, "%s left the game.", network_get_player(player_id).name);
    match_shell_add_chat_message(shell, FONT_HACK_WHITE, "", message, CHAT_MESSAGE_DURATION);

    match_shell_replay_file_write_entry(shell->replay_file, (ReplayEntry) {
        .type = REPLAY_ENTRY_DISCONNECT,
        .disconnect_player_id = player_id
    });

    // Show the victory banner to other players when this player leaves,
    // but only if the player was still active. If they are not active, it means they've
    // been defeated already, so don't show the victory banner for that player
    if (shell->match_state.players[player_id].mode == PLAYER_MODE_ACTIVE) {
        shell->match_state.players[player_id].mode = PLAYER_MODE_DEFEATED;
        if (shell->match_state.players[network_get_player_id()].mode == PLAYER_MODE_ACTIVE && !match_shell_is_at_least_one_opponent_in_match(shell)) {
            shell->mode = MATCH_SHELL_MODE_MATCH_OVER_VICTORY;
        }
    }
}

// BUILDING PLACEMENT

ivec2 match_shell_get_building_cell(int building_size, ivec2 camera_offset) {
    ivec2 offset = building_size == 1 ? ivec2(0, 0) : ivec2(building_size, building_size) - ivec2(2, 2);
    ivec2 building_cell = ((input_get_mouse_position() + camera_offset) / TILE_SIZE) - offset;
    building_cell.x = std::max(0, building_cell.x);
    building_cell.y = std::max(0, building_cell.y);
    return building_cell;
}

bool match_shell_building_can_be_placed(const MatchShell* shell) {
    const EntityData& entity_data = entity_get_data(shell->building_type);
    ivec2 building_cell = match_shell_get_building_cell(entity_data.cell_size, shell->camera_offset);
    ivec2 miner_cell = shell->match_state.entities.get_by_id(match_get_nearest_builder(shell->match_state, shell->selection, building_cell)).cell;

    // Check that the building is in bounds
    if (!map_is_cell_rect_in_bounds(shell->match_state.map, building_cell, entity_data.cell_size)) {
        return false;
    }

    // If building a hall, check that it is not too close to goldmines
    if (shell->building_type == ENTITY_HALL) {
        Rect building_rect = (Rect) {
            .x = building_cell.x, .y = building_cell.y,
            .w = entity_data.cell_size, .h = entity_data.cell_size
        };

        for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
            const Entity& entity = shell->match_state.entities[entity_index];
            if (entity.type == ENTITY_GOLDMINE) {
                Rect goldmine_block_rect = entity_goldmine_get_block_building_rect(entity.cell);
                if (building_rect.intersects(goldmine_block_rect)) {
                    return false;
                }
            }
        }
    }

    // Check individual squares in the building's rect to make sure they are valid
    for (int y = building_cell.y; y < building_cell.y + entity_data.cell_size; y++) {
        for (int x = building_cell.x; x < building_cell.x + entity_data.cell_size; x++) {
            if (!match_shell_is_building_place_cell_valid(shell, miner_cell, ivec2(x, y))) {
                return false;
            }
        }
    }

    return true;
}

bool match_shell_is_building_place_cell_valid(const MatchShell* shell, ivec2 miner_cell, ivec2 cell) {
    // Check that no entities are occupying the building rect
    Cell map_ground_cell = map_get_cell(shell->match_state.map, CELL_LAYER_GROUND, cell);
    if (map_ground_cell.type == CELL_UNIT || map_ground_cell.type == CELL_MINER || map_ground_cell.type == CELL_BUILDING) {
        // If the entity is not visible to the player, then we will ignore the fact that it's blocking the building
        uint8_t player_team = shell->match_state.players[network_get_player_id()].team;
        if (cell != miner_cell &&
                (entity_is_visible_to_player(shell->match_state, shell->match_state.entities.get_by_id(map_ground_cell.id), network_get_player_id()) ||
                (map_ground_cell.type == CELL_BUILDING && match_team_remembers_entity(shell->match_state, player_team, map_ground_cell.id)))) {
            return false;
        }
    } else if (map_ground_cell.type != CELL_EMPTY) {
        return false;
    }

    // Check that we're not building on a ramp
    if (map_is_tile_ramp(shell->match_state.map, cell)) {
        return false;
    }

    // Check that we're not building on top of a landmine
    Cell map_underground_cell = map_get_cell(shell->match_state.map, CELL_LAYER_UNDERGROUND, cell);
    if (map_underground_cell.type == CELL_BUILDING &&
            entity_is_visible_to_player(shell->match_state, shell->match_state.entities.get_by_id(map_underground_cell.id), network_get_player_id())) {
        return false;
    }

    // Check that we're not building on hidden fog
    if (match_get_fog(shell->match_state, shell->match_state.players[network_get_player_id()].team, cell) == FOG_HIDDEN) {
        return false;
    }

    return true;
}


// HOTKEY MENU

Rect match_shell_get_selection_list_item_rect(uint32_t selection_index) {
    ivec2 pos = SELECTION_LIST_TOP_LEFT + ivec2(((selection_index % 10) * 34) - 12, (selection_index / 10) * 34);
    return (Rect) { .x = pos.x, .y = pos.y, .w = 32, .h = 32 };
}

Rect match_shell_get_idle_miner_button_rect() {
    const SpriteInfo& minimap_frame_sprite_info = render_get_sprite_info(SPRITE_UI_MINIMAP);
    const SpriteInfo& control_group_frame_sprite_info = render_get_sprite_info(SPRITE_UI_CONTROL_GROUP);
    return (Rect) {
        .x = 4,
        .y = SCREEN_HEIGHT - minimap_frame_sprite_info.frame_height - control_group_frame_sprite_info.frame_height - 4,
        .w = control_group_frame_sprite_info.frame_width,
        .h = control_group_frame_sprite_info.frame_height
    };
}

bool match_shell_has_pressed_idle_miner_button() {
    if (input_is_hotkey_just_pressed(INPUT_HOTKEY_IDLE_MINER)) {
        return true;
    }
    if (match_shell_get_idle_miner_button_rect().has_point(input_get_mouse_position()) && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        return true;
    }
    return false;
}

// FIRE

bool match_shell_is_fire_on_screen(const MatchShell* shell) {
    // Return true if any fire cells intersect the screen
    for (uint32_t fire_index = 0; fire_index < shell->match_state.fires.size(); fire_index++) {
        const Fire& fire = shell->match_state.fires[fire_index];
        if (!match_shell_is_cell_rect_revealed(shell, fire.cell, 1)) {
            continue;
        }
        if (map_get_cell(shell->match_state.map, CELL_LAYER_GROUND, fire.cell).type != CELL_EMPTY) {
            continue;
        }
        Rect fire_rect = (Rect) {
            .x = (fire.cell.x * TILE_SIZE) - shell->camera_offset.x,
            .y = (fire.cell.y * TILE_SIZE) - shell->camera_offset.y,
            .w = TILE_SIZE,
            .h = TILE_SIZE
        };
        if (SOUND_LISTEN_RECT.intersects(fire_rect)) {
            return true;
        }
    }

    // Return true if any burning buildings intersect the screen
    for (uint32_t entity_index = 0; entity_index < shell->match_state.entities.size(); entity_index++) {
        const Entity& entity = shell->match_state.entities[entity_index];
        if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED) {
            continue;
        }
        if (!entity_is_building(entity.type) || !entity_check_flag(entity, ENTITY_FLAG_ON_FIRE)) {
            continue;
        }
        if (!match_shell_is_entity_visible(shell, entity)) {
            continue;
        }

        RenderSpriteParams params = match_shell_create_entity_render_params(shell, entity);
        const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_sprite(shell->match_state, entity));
        Rect render_rect = (Rect) {
            .x = params.position.x, .y = params.position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        };

        // Entity is on fire and on screen
        if (SOUND_LISTEN_RECT.intersects(render_rect)) {
            return true;
        }
    }

    return false;
}

// DEBUG

#ifdef GOLD_DEBUG

void match_shell_debug_handle_chat_message(MatchShell* shell, const std::string& message) {
    if (message == "/fog off") {
        shell->debug_fog_level = MATCH_SHELL_FOG_DISABLED;
    } else if (message == "/fog on") {
        shell->debug_fog_level = MATCH_SHELL_FOG_ENABLED;
    } else if (message == "/fog bot") {
        shell->debug_fog_level = MATCH_SHELL_FOG_BOT_VISION;
    } else if (message == "/gold") {
        shell->match_state.players[network_get_player_id()].gold += 5000;
    } else if (message == "/regions") {
        shell->debug_show_region_lines = !shell->debug_show_region_lines;
    }
}

bool match_shell_debug_has_next_checksums(MatchShell* shell) {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        // Checking for PLAYER_STATUS_READY, which is to say we are looking for active players but not bots
        if (network_get_player(player_id).status != NETWORK_PLAYER_STATUS_READY) {
            continue;
        }
        if (shell->checksums[player_id].empty()) {
            return false;
        }
    }

    return true;
}

bool match_shell_debug_are_next_checksums_out_of_sync(MatchShell* shell) {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (player_id == network_get_player_id() ||
                network_get_player(player_id).status != NETWORK_PLAYER_STATUS_READY) {
            continue;
        }
        if (shell->checksums[player_id].front() != shell->checksums[network_get_player_id()].front()) {
            log_error("DESYNC found on frame %u between player %u (checksum %u) and player %u (checksum %u)",
                shell->next_checksum_frame,
                player_id,
                shell->checksums[player_id].front(),
                network_get_player_id(),
                shell->checksums[network_get_player_id()].front());
            return true;
        }
    }

    return false;
}

#else

void match_shell_debug_handle_chat_message(MatchShell* /*shell*/, const std::string& /*message*/) {}
bool match_shell_debug_has_next_checksums(MatchShell* /*shell*/) { return false; }
bool match_shell_debug_are_next_checksums_out_of_sync(MatchShell* /*shell*/) { return false; }

#endif
