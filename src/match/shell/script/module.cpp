#include "core/animation.h"
#include "core/resource.h"
#include "core/sound.h"
#include "defines.h"
#include "match/shell/script/script.h"

#include "match/shell/shell.h"
#include "match/state/match.h"
#include "network/network.h"
#include "match/state/upgrade.h"
#include "util/bitflag.h"

// General
int script_log(lua_State* lua_state);
int script_play_sound(lua_State* lua_state);
int script_set_next_music_track(lua_State* lua_state);
int script_get_time(lua_State* lua_state);
int script_create_alert(lua_State* lua_state);

// Match over
int script_set_match_over_victory(lua_State* lua_state);
int script_set_match_over_defeat(lua_State* lua_state);

// Player
int script_is_player_defeated(lua_State* lua_state);
int script_get_player_gold(lua_State* lua_state);
int script_get_player_gold_mined_total(lua_State* lua_state);
int script_grant_player_upgrade(lua_State* lua_state);
int script_get_player_population(lua_State* lua_state);
int script_get_player_entity_count(lua_State* lua_state);

// Chat
int script_chat(lua_State* lua_state);
int script_chat_prefixed(lua_State* lua_state);
int script_hint(lua_State* lua_state);

// Camera
int script_fog_reveal(lua_State* lua_state);
int script_get_camera_centered_cell(lua_State* lua_state);
int script_begin_camera_pan(lua_State* lua_state);
int script_begin_camera_pan_and_shake(lua_State* lua_state);
int script_begin_camera_shake(lua_State* lua_state);
int script_hold_camera(lua_State* lua_state);
int script_release_camera(lua_State* lua_state);
int script_get_camera_mode(lua_State* lua_state);
int script_begin_camera_shake(lua_State* lua_state);

// Objectives
int script_add_objective(lua_State* lua_state);
int script_set_objective_variable_counter(lua_State* lua_state);
int script_complete_objective(lua_State* lua_state);
int script_is_objective_complete(lua_State* lua_state);
int script_are_objectives_complete(lua_State* lua_state);
int script_clear_objectives(lua_State* lua_state);
int script_set_global_objective_counter(lua_State* lua_state);
int script_set_global_objective_counter_variable_value(lua_State* lua_state);

// Entities
int script_is_entity_visible_to_player(lua_State* lua_state);
int script_highlight_entity(lua_State* lua_state);
int script_get_entity_gold_cost(lua_State* lua_state);
int script_get_building_which_trains(lua_State* lua_state);
int script_find_entity_spawn_cells(lua_State* lua_state);
int script_create_entity(lua_State* lua_state);
int script_remove_entity(lua_State* lua_state);
int script_edit_entity(lua_State* lua_state);
int script_get_hall_surrounding_goldmine(lua_State* lua_state);

// Bot
int script_bot_add_squad(lua_State* lua_state);
int script_bot_squad_exists(lua_State* lua_state);
int script_bot_get_squad_count(lua_State* lua_state);
int script_bot_get_squad_by_index(lua_State* lua_state);
int script_bot_get_squad_by_id(lua_State* lua_state);
int script_bot_set_squad_target_cell(lua_State* lua_state);
int script_bot_add_entities_to_squad(lua_State* lua_state);
int script_bot_get_entity_squad_id(lua_State* lua_state);
int script_bot_set_config_flag(lua_State* lua_state);
int script_bot_get_allowed_entities(lua_State* lua_state);
int script_bot_set_allowed_entities(lua_State* lua_state);
int script_bot_is_entity_reserved(lua_State* lua_state);
int script_bot_reserve_entity(lua_State* lua_state);
int script_bot_release_entity(lua_State* lua_state);

// Match input
int script_queue_match_input(lua_State* lua_state);

// Avalanche
int script_create_avalanche_column(lua_State* lua_state);
int script_explode_rigged_goldmine(lua_State* lua_state);

const luaL_reg GOLD_FUNCS[] = {
    // General
    { "log", script_log },
    { "play_sound", script_play_sound },
    { "set_next_music_track", script_set_next_music_track },
    { "get_time", script_get_time },
    { "create_alert", script_create_alert },

    // Match over
    { "set_match_over_victory", script_set_match_over_victory },
    { "set_match_over_defeat", script_set_match_over_defeat },

    // Player
    { "is_player_defeated", script_is_player_defeated },
    { "get_player_gold", script_get_player_gold },
    { "get_player_gold_mined_total", script_get_player_gold_mined_total },
    { "grant_player_upgrade", script_grant_player_upgrade },
    { "get_player_population", script_get_player_population },
    { "get_player_entity_count", script_get_player_entity_count },

    // Chat
    { "chat", script_chat },
    { "chat_prefixed", script_chat_prefixed },
    { "hint", script_hint },

    // Camera
    { "fog_reveal", script_fog_reveal },
    { "get_camera_centered_cell", script_get_camera_centered_cell },
    { "begin_camera_pan", script_begin_camera_pan },
    { "begin_camera_pan_and_shake", script_begin_camera_pan_and_shake },
    { "begin_camera_shake", script_begin_camera_shake },
    { "hold_camera", script_hold_camera },
    { "release_camera", script_release_camera },
    { "get_camera_mode", script_get_camera_mode },

    // Objectives
    { "add_objective", script_add_objective },
    { "set_objective_variable_counter", script_set_objective_variable_counter },
    { "complete_objective", script_complete_objective },
    { "is_objective_complete", script_is_objective_complete },
    { "are_objectives_complete", script_are_objectives_complete },
    { "clear_objectives", script_clear_objectives },
    { "set_global_objective_counter", script_set_global_objective_counter },
    { "set_global_objective_counter_variable_value", script_set_global_objective_counter_variable_value },

    // Entities
    { "is_entity_visible_to_player", script_is_entity_visible_to_player },
    { "highlight_entity", script_highlight_entity },
    { "get_entity_gold_cost", script_get_entity_gold_cost },
    { "get_building_which_trains", script_get_building_which_trains },
    { "find_entity_spawn_cells", script_find_entity_spawn_cells },
    { "create_entity", script_create_entity },
    { "remove_entity", script_remove_entity },
    { "edit_entity", script_edit_entity },
    { "get_hall_surrounding_goldmine", script_get_hall_surrounding_goldmine },

    // Bot
    { "bot_add_squad", script_bot_add_squad },
    { "bot_squad_exists", script_bot_squad_exists },
    { "bot_get_squad_count", script_bot_get_squad_count },
    { "bot_get_squad_by_index", script_bot_get_squad_by_index },
    { "bot_get_squad_by_id", script_bot_get_squad_by_id },
    { "bot_set_squad_target_cell", script_bot_set_squad_target_cell },
    { "bot_add_entities_to_squad", script_bot_add_entities_to_squad },
    { "bot_get_entity_squad_id", script_bot_get_entity_squad_id },
    { "bot_set_config_flag", script_bot_set_config_flag },
    { "bot_get_allowed_entities", script_bot_get_allowed_entities },
    { "bot_set_allowed_entities", script_bot_set_allowed_entities },
    { "bot_is_entity_reserved", script_bot_is_entity_reserved },
    { "bot_reserve_entity", script_bot_reserve_entity },
    { "bot_release_entity", script_bot_release_entity },

    // Match input
    { "queue_match_input", script_queue_match_input },

    // Avalanche
    { "create_avalanche_column", script_create_avalanche_column },
    { "explode_rigged_goldmine", script_explode_rigged_goldmine },

    { NULL, NULL }
};

// GENERAL

// Send a debug log. If debug logging is disabled, this function does nothing.
// @param ... any Values to print
int script_log(lua_State* lua_state) {
#if GOLD_LOG_LEVEL >= LOG_LEVEL_DEBUG
    int nargs = lua_gettop(lua_state);
    char buffer[4096];
    char* buffer_ptr = buffer;

    for (int arg_index = 1; arg_index <= nargs; arg_index++) {
        buffer_ptr += script_sprintf(buffer_ptr, lua_state, arg_index);
        if (arg_index < nargs) {
            buffer_ptr += sprintf(buffer_ptr, " ");
        }
    }

    log_debug("%s", buffer);
#endif

    return 0;
}

// Plays a sound effect.
// @param sound number
int script_play_sound(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    int sound = (int)lua_tonumber(lua_state, 1);

    if (sound < 0 || sound >= SOUND_COUNT) {
        script_error(lua_state, "Invalid sound %i", sound);
    }

    sound_play((SoundName)sound);

    return 0;
}

// Sets the next music track
// @param track number
int script_set_next_music_track(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    uint32_t track = (uint32_t)lua_tonumber(lua_state, 1);
    if (track < RESOURCE_MUSIC_MATCH1 || track >= RESOURCE_MUSIC_MATCH1 + MATCH_SHELL_MUSIC_TRACK_COUNT) {
        script_error(lua_state, "Music track %u is not valid.", track);
    }

    MatchShell* shell = script_get_match_shell();
    shell->music_next_track = track;

    return 0;
}

// Returns the time in seconds since the scenario started.
// @return number
int script_get_time(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShell* shell = script_get_match_shell();
    lua_pushnumber(lua_state, (double)shell->match_timer / (double)UPDATES_PER_SECOND);

    return 1;
}

// Creates an alert on the minimap
// @param alert_color number
// @param cell ivec2
// @param cell_size number
int script_create_alert(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TTABLE, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 3);

    MatchShell* shell = script_get_match_shell();

    MinimapPixel pixel = MINIMAP_PIXEL_COUNT;
    uint32_t alert_color = (uint32_t)lua_tonumber(lua_state, 1);
    switch (alert_color) {
        case SCRIPT_ALERT_COLOR_WHITE:
            pixel = MINIMAP_PIXEL_WHITE;
            break;
        case SCRIPT_ALERT_COLOR_GOLD:
            pixel = MINIMAP_PIXEL_GOLD;
            break;
        case SCRIPT_ALERT_COLOR_PLAYER:
            pixel = (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + shell->match_state.players[network_get_player_id()].recolor_id);
            break;
        default:
            script_error(lua_state, "Unrecognized alert color %u", alert_color);
            break;
    }
    GOLD_ASSERT(pixel != MINIMAP_PIXEL_COUNT);

    ivec2 cell = script_lua_to_ivec2(lua_state, 2, "cell");
    int cell_size = (int)lua_tonumber(lua_state, 3);

    shell->alerts.push_back((Alert) {
        .pixel = pixel,
        .cell = cell,
        .cell_size = cell_size,
        .timer = ALERT_TOTAL_DURATION
    });

    return 0;
}

// MATCH OVER

// End the match in victory.
int script_set_match_over_victory(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShell* shell = script_get_match_shell();

    // This validation is to prevent the player from losing and then winning
    if (match_shell_is_in_leave_match_mode(shell)) {
        log_warn("script set_match_over_victory(): the match is already over.");
        return 0;
    }

    match_shell_set_match_over_victory(shell);

    return 0;
}

// End the match in defeat.
int script_set_match_over_defeat(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShell* shell = script_get_match_shell();

    // This validation is to prevent the player from winning and then losing
    if (match_shell_is_in_leave_match_mode(shell)) {
        log_warn("script set_match_over_defeat(): the match is already over.");
        return 0;
    }

    match_shell_set_match_over_defeat(shell);

    return 0;
}

// PLAYERS

// Checks if the player has been defeated.
// @param player_id number
// @return boolean
int script_is_player_defeated(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id);

    lua_pushboolean(lua_state, shell->match_state.players[player_id].mode != PLAYER_MODE_ACTIVE);

    return 1;
}

// Returns the specified player's gold count
// @param player_id number
// @return number
int script_get_player_gold(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id);

    lua_pushnumber(lua_state, shell->match_state.players[player_id].gold);

    return 1;
}

// Returns the total number of gold mined by the specified player this match
// @param player_id number
// @return number
int script_get_player_gold_mined_total(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id);

    lua_pushnumber(lua_state, shell->match_state.players[player_id].gold_mined_total);

    return 1;
}

// Grants the player the specified upgrade
// @param player_id number
// @param upgrade number
int script_grant_player_upgrade(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id);

    uint32_t upgrade = (uint32_t)lua_tonumber(lua_state, 2);

    // Validate upgrade type is a real upgrade
    uint32_t upgrade_index;
    for (upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
        uint32_t matching_upgrade = 1U << upgrade_index;
        if (matching_upgrade == upgrade) {
            break;
        }
    }
    if (upgrade_index == UPGRADE_COUNT) {
        script_error(lua_state, "Invalid upgrade type %u", upgrade);
    }

    match_grant_player_upgrade(shell->match_state, player_id, upgrade);

    return 0;
}

// Returns the player's population
// @param player_id number
// @return number
int script_get_player_population(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id);

    lua_pushnumber(lua_state, match_get_player_population(shell->match_state, player_id));

    return 1;
}

// CHAT

FontName script_chat_get_font_from_chat_color(int chat_color) {
    switch (chat_color) {
        case SCRIPT_CHAT_COLOR_WHITE:
            return FONT_HACK_WHITE;
        case SCRIPT_CHAT_COLOR_GOLD:
            return FONT_HACK_GOLD;
        case SCRIPT_CHAT_COLOR_BLUE:
            return FONT_HACK_PLAYER0;
        default:
            return FONT_COUNT;
    }
}

// Sends a chat message
// @param message string
int script_chat(lua_State* lua_state) {
    const int arg_types[] = { LUA_TSTRING };
    script_validate_arguments(lua_state, arg_types, 1);

    const char* chat_message = lua_tostring(lua_state, 1);

    MatchShell* shell = script_get_match_shell();
    match_shell_add_chat_message(shell, FONT_HACK_WHITE, "", chat_message, CHAT_MESSAGE_DURATION);
    sound_play(SOUND_UI_CLICK);

    return 0;
}

// Sends a chat message with a colored prefix
// @param prefix_color number
// @param prefix string
// @param message string
int script_chat_prefixed(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TSTRING, LUA_TSTRING };
    script_validate_arguments(lua_state, arg_types, 3);

    MatchShell* shell = script_get_match_shell();
    int chat_color = (int)lua_tonumber(lua_state, 1);
    const char* chat_prefix = lua_tostring(lua_state, 2);
    const char* chat_message = lua_tostring(lua_state, 3);

    FontName font = script_chat_get_font_from_chat_color(chat_color);
    if (font == FONT_COUNT) {
        script_error(lua_state, "Unregonized chat color %i", chat_color);
    }

    match_shell_add_chat_message(shell, font, chat_prefix, chat_message, CHAT_MESSAGE_DURATION);

    return 0;
}

// Sends a hint message.
// @param message string
int script_hint(lua_State* lua_state) {
    const int arg_types[] = { LUA_TSTRING };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();
    const char* message = lua_tostring(lua_state, 1);

    sound_play(SOUND_UI_CLICK);
    match_shell_add_chat_message(shell, FONT_HACK_PLAYER0, "Hint:", message, CHAT_MESSAGE_HINT_DURATION);

    return 0;
}

// CAMERA / FOG

// Reveals fog at the specified cell. If no player ID is specified, it will default to scenario.PLAYER_ID
// @param params { player_id: number|nil cell: ivec2, cell_size: number, sight: number, duration: number }
int script_fog_reveal(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();

    // Player ID
    lua_getfield(lua_state, 1, "player_id");
    uint8_t player_id = network_get_player_id();
    if (!lua_isnil(lua_state, -1)) {
        script_validate_type(lua_state, -1, "player_id", LUA_TNUMBER);
        player_id = (uint8_t)lua_tonumber(lua_state, -1);
        script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_ACTIVE);
    }
    lua_pop(lua_state, 1);

    // Team
    uint8_t team = shell->match_state.players[player_id].team;

    // Cell
    lua_getfield(lua_state, 1, "cell");
    ivec2 cell = script_lua_to_ivec2(lua_state, -1, "cell");
    lua_pop(lua_state, 1);

    // Cell size
    lua_getfield(lua_state, 1, "cell_size");
    script_validate_type(lua_state, -1, "cell_size", LUA_TNUMBER);
    int cell_size = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    // Sight
    lua_getfield(lua_state, 1, "sight");
    script_validate_type(lua_state, -1, "sight", LUA_TNUMBER);
    int sight = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    // Duration
    lua_getfield(lua_state, 1, "duration");
    script_validate_type(lua_state, -1, "duration", LUA_TNUMBER);
    double duration = lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    match_fog_update(shell->match_state, team, cell, cell_size, sight, false, CELL_LAYER_GROUND, true);
    shell->match_state.fog_reveals.push_back((FogReveal) {
        .team = team,
        .cell = cell,
        .cell_size = cell_size,
        .sight = sight,
        .timer = (uint32_t)(duration * (double)UPDATES_PER_SECOND)
    });

    return 0;
}

// Returns the cell the camera is currently centered on
// @return ivec2
int script_get_camera_centered_cell(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    const MatchShell* shell = script_get_match_shell();

    ivec2 camera_center = shell->camera_offset + ivec2(SCREEN_WIDTH / 2, ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2));
    ivec2 cell = camera_center / TILE_SIZE;

    lua_newtable(lua_state);

    lua_pushnumber(lua_state, cell.x);
    lua_setfield(lua_state, -2, "x");

    lua_pushnumber(lua_state, cell.y);
    lua_setfield(lua_state, -2, "y");

    return 1;
}

// Gradually pans the camera to center on the specified cell.
// @param cell ivec2 The cell to pan the camera to
// @param duration number The duration in seconds of the camera pan
int script_begin_camera_pan(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    ivec2 cell = script_lua_to_ivec2(lua_state, 1, "arg 1");
    double duration = lua_tonumber(lua_state, 2);

    MatchShell* shell = script_get_match_shell();
    match_shell_begin_camera_pan(shell, cell, duration, false);

    return 0;
}

// Pans the camera to the specified cell while shaking for the specified duration
// @param cell ivec2 The cell to pan the camera to
// @param duration number The duration in seconds of the camera pan
int script_begin_camera_pan_and_shake(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    ivec2 cell = script_lua_to_ivec2(lua_state, 1, "arg 1");
    double duration = lua_tonumber(lua_state, 2);

    MatchShell* shell = script_get_match_shell();
    match_shell_begin_camera_pan(shell, cell, duration, true);

    return 0;
}

// Shakes the camera for the specified duration
// @param duration number
int script_begin_camera_shake(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    double duration = lua_tonumber(lua_state, 1);

    MatchShell* shell = script_get_match_shell();
    ivec2 camera_center = shell->camera_offset + ivec2(SCREEN_WIDTH / 2, ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2));
    ivec2 camera_cell = camera_center / TILE_SIZE;
    match_shell_begin_camera_pan(shell, camera_cell, duration, true);

    return 0;
}

// Removes camera movement from the player and holds the camera in place
int script_hold_camera(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShell* shell = script_get_match_shell();
    shell->camera_mode = CAMERA_MODE_HELD;

    return 0;
}

// Returns camera movement to the player
int script_release_camera(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShell* shell = script_get_match_shell();
    shell->camera_mode = CAMERA_MODE_FREE;

    return 0;
}

// Returns the current camera mode.
// @return number
int script_get_camera_mode(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShell* shell = script_get_match_shell();
    lua_pushnumber(lua_state, shell->camera_mode);

    return 1;
}

// OBJECTIVES

// Adds an objective to the objectives list.
//
// If entity_type is provided, then counter_target is also required and
// the objective counter will be based on the player's entities of that type.
//
// If counter_target is provided but entity_type is not, then the objective counter
// will be a variable counter that must be manually updated.
//
// Returns the index of the created objective.
// @param params { description: string, entity_type: number|nil, counter_target: number|nil }
// @return number
int script_add_objective(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    // Initialize objective
    Objective objective = (Objective) {
        .description = std::string(),
        .is_complete = false,
        .counter_type = OBJECTIVE_COUNTER_TYPE_NONE,
        .counter_value = 0,
        .counter_target = 0
    };

    // Get description
    lua_getfield(lua_state, 1, "description");
    script_validate_type(lua_state, -1, "description", LUA_TSTRING);
    objective.description = std::string(lua_tostring(lua_state, -1));
    lua_pop(lua_state, 1);

    // Get entity type (optional)
    lua_getfield(lua_state, 1, "entity_type");
    if (!lua_isnil(lua_state, -1)) {
        script_validate_type(lua_state, -1, "entity_type", LUA_TNUMBER);
        int entity_type = (int)lua_tonumber(lua_state, -1);
        script_validate_entity_type(lua_state, entity_type);

        objective.counter_type = OBJECTIVE_COUNTER_TYPE_ENTITY;
        objective.counter_value = (uint32_t)entity_type;
    }
    lua_pop(lua_state, 1);

    // Get counter target (optional, but required if counter_type_entity)
    lua_getfield(lua_state, 1, "counter_target");
    if (objective.counter_type == OBJECTIVE_COUNTER_TYPE_ENTITY && lua_isnil(lua_state, -1)) {
        script_error(lua_state, "Objective field counter_target is required when entity_type is defined.");
    }
    if (!lua_isnil(lua_state, -1)) {
        script_validate_type(lua_state, -1, "counter_target", LUA_TNUMBER);
        objective.counter_target = (uint32_t)lua_tonumber(lua_state, -1);

        if (objective.counter_type == OBJECTIVE_COUNTER_TYPE_NONE) {
            objective.counter_type = OBJECTIVE_COUNTER_TYPE_VARIABLE;
        }
    }
    lua_pop(lua_state, 1);

    MatchShell* shell = script_get_match_shell();

    shell->scenario_objectives.push_back(objective);
    lua_pushnumber(lua_state, (int)shell->scenario_objectives.size() - 1);

    return 1;
}

// Sets the specified objectives variable counter
// @param objective_index number
// @param counter_value number
int script_set_objective_variable_counter(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    uint32_t objective_index = (uint32_t)lua_tonumber(lua_state, 1);
    uint32_t counter_value = (uint32_t)lua_tonumber(lua_state, 2);

    MatchShell* shell = script_get_match_shell();

    if (objective_index >= shell->scenario_objectives.size()) {
        script_error(lua_state, "Cannot set counter value on out-of-bounds objective %u.", objective_index);
    }

    if (shell->scenario_objectives[objective_index].counter_type != OBJECTIVE_COUNTER_TYPE_VARIABLE) {
        script_error(lua_state, "Cannot set counter value of objective %u. Its counter is not of type variable.", objective_index);
    }

    shell->scenario_objectives[objective_index].counter_value = counter_value;

    return 0;
}

// Marks the specified objective as complete.
// @param objective_index number
int script_complete_objective(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();
    uint32_t objective_index = lua_tonumber(lua_state, 1);
    if (objective_index >= shell->scenario_objectives.size()) {
        script_error(lua_state, "Objective index %u out of bounds.", objective_index);
    }

    shell->scenario_objectives[objective_index].is_complete = true;

    return 0;
}

// Returns true if the specified objective is complete.
// @param objective_index number
// @return boolean
int script_is_objective_complete(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();
    uint32_t objective_index = lua_tonumber(lua_state, 1);
    if (objective_index >= shell->scenario_objectives.size()) {
        script_error(lua_state, "Objective index %u out of bounds.", objective_index);
    }

    lua_pushboolean(lua_state, shell->scenario_objectives[objective_index].is_complete);

    return 1;
}

// Returns true if all objectives are complete.
// @return boolean
int script_are_objectives_complete(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    bool result = true;

    const MatchShell* shell = script_get_match_shell();
    if (shell->scenario_objectives.empty()) {
        result = false;
    }
    for (uint32_t index = 0; index < shell->scenario_objectives.size(); index++) {
        if (!shell->scenario_objectives[index].is_complete) {
            result = false;
            break;
        }
    }

    lua_pushboolean(lua_state, result);

    return 1;
}

// Clears the objectives list.
int script_clear_objectives(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShell* shell = script_get_match_shell();
    shell->scenario_objectives.clear();

    return 0;
}

// Sets the global objective counter
// @param params table
int script_set_global_objective_counter(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    // Counter type
    lua_getfield(lua_state, 1, "type");
    script_validate_type(lua_state, -1, "type", LUA_TNUMBER);
    uint32_t counter_type = (uint32_t)lua_tonumber(lua_state, -1);
    if (counter_type >= GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT) {
        script_error(lua_state, "Global objective counter type %u is invalid.", counter_type);
    }
    lua_pop(lua_state, 1);

    GlobalObjectiveCounter counter;
    counter.type = (GlobalObjectiveCounterType)counter_type;
    switch (counter.type) {
        case GLOBAL_OBJECTIVE_COUNTER_GOLD: {
            // Get max value
            lua_getfield(lua_state, 1, "max_value");
            script_validate_type(lua_state, -1, "max_value", LUA_TNUMBER);
            uint32_t max_value = (uint32_t)lua_tonumber(lua_state, -1);
            lua_pop(lua_state, 1);

            memset(&counter.gold, 0, sizeof(counter.gold));
            counter.gold.max_value = max_value;
            break;
        }
        case GLOBAL_OBJECTIVE_COUNTER_COUNTDOWN: {
            // Get header text
            lua_getfield(lua_state, 1, "header_text");
            script_validate_type(lua_state, -1, "header_text", LUA_TSTRING);
            strncpy(counter.variable.header_text, lua_tostring(lua_state, -1), GLOBAL_OBJECTIVE_COUNTER_HEADER_TEXT_BUFFER_SIZE);

            // Get countdown end time
            lua_getfield(lua_state, 1, "end_time_seconds");
            script_validate_type(lua_state, -1, "end_time_seconds", LUA_TNUMBER);
            uint32_t end_time_seconds = (uint32_t)lua_tonumber(lua_state, -1);
            lua_pop(lua_state, 1);

            counter.countdown.end_frame = end_time_seconds * UPDATES_PER_SECOND;
            break;
        }
        case GLOBAL_OBJECTIVE_COUNTER_VARIABLE: {
            // Get header text
            lua_getfield(lua_state, 1, "header_text");
            script_validate_type(lua_state, -1, "header_text", LUA_TSTRING);
            strncpy(counter.variable.header_text, lua_tostring(lua_state, -1), GLOBAL_OBJECTIVE_COUNTER_HEADER_TEXT_BUFFER_SIZE);
            lua_pop(lua_state, 1);

            lua_getfield(lua_state, 1, "initial_value");
            if (lua_isnumber(lua_state, -1)) {
                counter.variable.value = (uint32_t)lua_tonumber(lua_state, -1);
            }
            lua_pop(lua_state, 1);
            break;
        }
        case GLOBAL_OBJECTIVE_COUNTER_OFF:
            break;
        case GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT:
            GOLD_ASSERT(false);
    }

    MatchShell* shell = script_get_match_shell();
    shell->scenario_global_objective_counter = counter;

    return 0;
}

// Updates the global objective counter. Should only be called on variable counter.
// @param value number
int script_set_global_objective_counter_variable_value(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();

    if (shell->scenario_global_objective_counter.type != GLOBAL_OBJECTIVE_COUNTER_VARIABLE) {
        script_error(lua_state, "Tried to update variable global objective counter, but the objective counter type is not variable.");
    }

    // Counter value
    uint32_t value = lua_tonumber(lua_state, 1);

    shell->scenario_global_objective_counter.variable.value = value;

    return 0;
}

// ENTITIES

// Returns true if the specified entity is visible to the player.
// @param entity_id number
// @return boolean
int script_is_entity_visible_to_player(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();
    EntityId entity_id = lua_tonumber(lua_state, 1);
    uint32_t entity_index = script_validate_entity_id(lua_state, shell, entity_id);

    bool result = entity_is_visible_to_player(shell->match_state, shell->match_state.entities[entity_index], network_get_player_id());
    lua_pushboolean(lua_state, result);

    return 1;
}

// Highlights the specified entity.
// @param entity_id number
int script_highlight_entity(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();
    EntityId entity_id = lua_tonumber(lua_state, 1);
    script_validate_entity_id(lua_state, shell, entity_id);

    shell->entity_highlights.push_back((EntityHighlight) {
        .animation = animation_create(ANIMATION_UI_HIGHLIGHT_ENTITY),
        .entity_id = entity_id
    });

    // Do a fog reveal on the highlighted entity
    const Entity& entity = shell->match_state.entities.get_by_id(entity_id);
    FogReveal fog_reveal = (FogReveal) {
        .team = shell->match_state.players[network_get_player_id()].team,
        .cell = entity.cell,
        .cell_size = entity_get_data(entity.type).cell_size,
        .sight = 3,
        .timer = 160U // this is the duration of animation_ui_highlight_entity
    };

    match_fog_update(shell->match_state, fog_reveal.team, fog_reveal.cell, fog_reveal.cell_size, fog_reveal.sight, false, CELL_LAYER_GROUND, true);
    shell->match_state.fog_reveals.push_back(fog_reveal);

    return 0;
}

// Returns the number of entities controlled by the player of a given type.
// @param player_id number
// @param entity_type number
// @return number
int script_get_player_entity_count(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    int entity_type = (int)lua_tonumber(lua_state, 2);
    script_validate_entity_type(lua_state, entity_type);

    const MatchShell* shell = script_get_match_shell();

    uint32_t result = match_shell_get_player_entity_count(shell, player_id, (EntityType)entity_type);
    lua_pushnumber(lua_state, result);

    return 1;
}

void script_populate_table_with_entity_data(lua_State* lua_state, const Entity& entity, const Map& map) {
    // Type
    lua_pushnumber(lua_state, entity.type);
    lua_setfield(lua_state, -2, "type");

    // Mode
    lua_pushnumber(lua_state, entity.mode);
    lua_setfield(lua_state, -2, "mode");

    // Player
    lua_pushnumber(lua_state, entity.player_id);
    lua_setfield(lua_state, -2, "player_id");

    // Cell
    script_lua_push_ivec2(lua_state, entity.cell);
    lua_setfield(lua_state, -2, "cell");

    // Target
    lua_newtable(lua_state);
        // Type
        lua_pushnumber(lua_state, entity.target.type);
        lua_setfield(lua_state, -2, "type");

        // Id
        lua_pushnumber(lua_state, entity.target.id);
        lua_setfield(lua_state, -2, "id");

        // Cell
        script_lua_push_ivec2(lua_state, entity.cell);
        lua_setfield(lua_state, -2, "cell");
    lua_setfield(lua_state, -2, "target");

    // Health
    lua_pushnumber(lua_state, entity.health);
    lua_setfield(lua_state, -2, "health");

    // Gold held
    lua_pushnumber(lua_state, entity.gold_held);
    lua_setfield(lua_state, -2, "gold_held");

    // Garrisoned units
    lua_createtable(lua_state, entity.garrisoned_units.size(), 0);
    for (uint32_t garrisoned_units_index = 0; garrisoned_units_index < entity.garrisoned_units.size(); garrisoned_units_index++) {
        lua_pushnumber(lua_state, entity.garrisoned_units[garrisoned_units_index]);
        lua_rawseti(lua_state, -2, garrisoned_units_index + 1);
    }
    lua_setfield(lua_state, -2, "garrisoned_units");

    // Garrison ID
    if (entity.garrison_id != ID_NULL) {
        lua_pushnumber(lua_state, entity.garrison_id);
    } else {
        // We have to explicitly push nil here, so that if users re-use the table,
        // we still set the garrison_id as nil instead of recycling old values
        lua_pushnil(lua_state);
    }
    lua_setfield(lua_state, -2, "garrison_id");

    // Elevation
    lua_pushnumber(lua_state, entity_get_elevation(entity, map));
    lua_setfield(lua_state, -2, "elevation");
}

// Returns the gold cost of the specified entity type
// @param entity_type number
// @return number
int script_get_entity_gold_cost(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    int entity_type = (uint32_t)lua_tonumber(lua_state, 1);
    script_validate_entity_type(lua_state, entity_type);

    lua_pushnumber(lua_state, entity_get_data((EntityType)entity_type).gold_cost);

    return 1;
}

// Returns the building type which trains the specified entity
// @param entity_type number
// @return number
int script_get_building_which_trains(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    int entity_type = (uint32_t)lua_tonumber(lua_state, 1);
    script_validate_entity_type(lua_state, entity_type);

    EntityType building_type = bot_get_building_which_trains((EntityType)entity_type);
    lua_pushnumber(lua_state, building_type);

    return 1;
}

// Finds an empty cell for the specified entities to spawn in. The manhattan distance of the cell to the provided spawn_cell will be less than 16
//
// Accepts a table of entity types and returns a parallel table where each entry in the result is either an ivec2 or nil if no spawn location could be found
// @param spawn_cell ivec2
// @param entity_types table
// @return table
int script_find_entity_spawn_cells(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE, LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 2);

    // Spawn cell
    ivec2 spawn_cell = script_lua_to_ivec2(lua_state, 1, "spawn_cell");

    // Entity type
    std::vector<EntityType> entity_types;
    lua_pushnil(lua_state);
    while (lua_next(lua_state, 2)) {
        script_validate_type(lua_state, -1, "entity_type", LUA_TNUMBER);
        int entity_type = (int)lua_tonumber(lua_state, -1);
        script_validate_entity_type(lua_state, entity_type);
        entity_types.push_back((EntityType)entity_type);
        lua_pop(lua_state, 1);
    }
    lua_pop(lua_state, 1);

    if (entity_types.empty()) {
        script_error(lua_state, "No entity_types were provided.");
    }

    const MatchShell* shell = script_get_match_shell();

    std::vector<ivec2> frontier;
    std::vector<bool> explored = std::vector<bool>(shell->match_state.map.width * shell->match_state.map.height, false);
    std::vector<bool> blocked = std::vector<bool>(shell->match_state.map.width * shell->match_state.map.height, false);

    std::vector<ivec2> results = std::vector<ivec2>(entity_types.size(), ivec2(-1, -1));
    size_t result_index = 0;
    const EntityData& first_entity_data = entity_get_data(entity_types[result_index]);
    CellLayer entity_cell_layer = first_entity_data.cell_layer;
    int entity_cell_size = first_entity_data.cell_size;

    frontier.push_back(spawn_cell);

    while (!frontier.empty()) {
        uint32_t nearest_index = 0;
        for (uint32_t index = 1; index < frontier.size(); index++) {
            if (ivec2::manhattan_distance(frontier[index], spawn_cell) <
                    ivec2::manhattan_distance(frontier[nearest_index], spawn_cell)) {
                nearest_index = index;
            }
        }
        ivec2 next = frontier[nearest_index];
        frontier[nearest_index] = frontier.back();
        frontier.pop_back();

        explored[next.x + (next.y * shell->match_state.map.width)] = true;

        // Check map cell occupy
        bool next_is_in_bounds = map_is_cell_rect_in_bounds(shell->match_state.map, next, entity_cell_size);
        bool next_is_blocked = map_is_cell_rect_occupied(shell->match_state.map, entity_cell_layer, next, entity_cell_size);
        // And also check temporary blocks
        if (next_is_in_bounds && !next_is_blocked) {
            for (int y = next.y; y < next.y + entity_cell_size; y++) {
                for (int x = next.x; x < next.x + entity_cell_size; x++) {
                    if (blocked[x + (y * shell->match_state.map.width)]) {
                        GOLD_ASSERT(map_is_cell_in_bounds(shell->match_state.map, ivec2(x, y)));
                        next_is_blocked = true;
                    }
                }
            }
        }

        if (next_is_in_bounds && !next_is_blocked) {
            // Temporary block the cell
            for (int y = next.y; y < next.y + entity_cell_size; y++) {
                for (int x = next.x; x < next.x + entity_cell_size; x++) {
                    GOLD_ASSERT(map_is_cell_in_bounds(shell->match_state.map, ivec2(x, y)));
                    blocked[x + (y * shell->match_state.map.width)] = true;
                }
            }

            // Store result
            results[result_index] = next;
            result_index++;

            // End the loop if that was the last entity type
            if (result_index == entity_types.size()) {
                break;
            }

            // Otherwise, set the cell layer and size
            const EntityData& entity_data = entity_get_data(entity_types[result_index]);
            entity_cell_layer = entity_data.cell_layer;
            entity_cell_size = entity_data.cell_size;
        }

        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = next + DIRECTION_IVEC2[direction];
            if (!map_is_cell_in_bounds(shell->match_state.map, child)) {
                continue;
            }
            if (explored[child.x + (child.y * shell->match_state.map.width)]) {
                continue;
            }
            if (ivec2::manhattan_distance(child, spawn_cell) > 16) {
                continue;
            }

            bool is_in_frontier = false;
            for (ivec2 cell : frontier) {
                if (cell == child) {
                    is_in_frontier = true;
                    break;
                }
            }
            if (!is_in_frontier) {
                frontier.push_back(child);
            }
        }
    }

    log_debug("script_find_entity_spawn_cells - result count %u", results.size());

    // Create the result table
    uint32_t number_of_nil_results = 0;
    lua_createtable(lua_state, (int)results.size(), 0);
    for (int index = 0; index < (int)results.size(); index++) {
        if (results[index].x == -1) {
            lua_pushnil(lua_state);
            number_of_nil_results++;
        } else {
            script_lua_push_ivec2(lua_state, results[index]);
        }

        lua_rawseti(lua_state, -2, index + 1);
    }

    if (number_of_nil_results != 0) {
        log_warn("script_find_entity_spawn_cells - %u results were nil. spawn_cell <%i, %i>", number_of_nil_results, spawn_cell.x, spawn_cell.y);
    }

    return 1;
}

// Creates a new entity. Returns the ID of the newly created entity
// @param entity_type number
// @param cell ivec2
// @param player_id number
// @return number
int script_create_entity(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TTABLE, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 3);

    MatchShell* shell = script_get_match_shell();

    int entity_type = (int)lua_tonumber(lua_state, 1);
    script_validate_entity_type(lua_state, entity_type);

    ivec2 cell = script_lua_to_ivec2(lua_state, 2, "cell");

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 3);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_ACTIVE);

    EntityId entity_id = entity_create(shell->match_state, (EntityType)entity_type, cell, player_id);
    lua_pushnumber(lua_state, entity_id);

    return 1;
}

// Removes an entity
// @param entity_id number
int script_remove_entity(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 1);
    uint32_t entity_index = script_validate_entity_id(lua_state, shell, entity_id);

    const Entity& entity = shell->match_state.entities[entity_index];
    const EntityData& entity_data = entity_get_data(entity.type);

    match_fog_update(shell->match_state, shell->match_state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_data.has_detection, entity_data.cell_layer, false);
    map_set_cell_rect(shell->match_state.map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) { .type = CELL_EMPTY, .id = ID_NULL });

    log_info("Script removing entity %s ID %u player id %u", entity_get_data(entity.type).name, entity_id, entity.player_id);
    shell->match_state.entities.remove_at(entity_index);

    return 0;
}

// Sets values on an entity based on the passed in props table
// @param entity_id number
// @param props table
int script_edit_entity(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 2);

    MatchShell* shell = script_get_match_shell();

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 1);
    uint32_t entity_index = script_validate_entity_id(lua_state, shell, entity_id);

    Entity& entity = shell->match_state.entities[entity_index];

    // Energy
    lua_getfield(lua_state, 2, "energy");
    if (lua_isnumber(lua_state, -1)) {
        entity.energy = (uint32_t)lua_tonumber(lua_state, -1);
    }
    lua_pop(lua_state, 1);

    return 0;
}

// Returns the ID of the hall which surrounds the specified goldmine
// @param goldmine_id number
// @return number | nil
int script_get_hall_surrounding_goldmine(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShell* shell = script_get_match_shell();

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 1);
    uint32_t entity_index = script_validate_entity_id(lua_state, shell, entity_id);

    const Entity& goldmine = shell->match_state.entities[entity_index];

    EntityId hall_surrounding_goldmine_id = match_find_entity(shell->match_state, [&goldmine](const Entity& hall, EntityId /*hall_id*/) {
        return hall.type == ENTITY_HALL &&
            entity_is_selectable(hall) &&
            bot_does_entity_surround_goldmine(hall, goldmine.cell);
    });

    if (hall_surrounding_goldmine_id == ID_NULL) {
        lua_pushnil(lua_state);
    } else {
        lua_pushnumber(lua_state, hall_surrounding_goldmine_id);
    }

    return 1;
}

// BOT

// Spawns an enemy squad. The entities table should be an array of entity types.
// Returns the squad ID of the created squad, or SQUAD_ID_NULL if no squad was created.
// @param params { player_id: number, type: number, target_cell: ivec2, entity_list: table }
// @return number
int script_bot_add_squad(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();

    // Player
    lua_getfield(lua_state, 1, "player_id");
    script_validate_type(lua_state, -1, "player_id", LUA_TNUMBER);
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    // Type
    lua_getfield(lua_state, 1, "type");
    uint32_t squad_type = (uint32_t)lua_tonumber(lua_state, -1);
    if (squad_type >= BOT_SQUAD_TYPE_COUNT) {
        script_error(lua_state, "Squad type %u is invalid.", squad_type);
    }
    lua_pop(lua_state, 1);

    // Target cell
    lua_getfield(lua_state, 1, "target_cell");
    script_validate_type(lua_state, -1, "target_cell", LUA_TTABLE);
    ivec2 target_cell = script_lua_to_ivec2(lua_state, -1, "target_cell");
    lua_pop(lua_state, 1);

    // Entities
    lua_getfield(lua_state, 1, "entity_list");
    script_validate_type(lua_state, -1, "entity_list", LUA_TTABLE);

    EntityList entity_list;
    lua_pushnil(lua_state);
    while (lua_next(lua_state, -2)) {
        script_validate_type(lua_state, -1, "entity_list", LUA_TNUMBER);
        EntityId entity_id = (EntityId)lua_tonumber(lua_state, -1);
        uint32_t entity_index = shell->match_state.entities.get_index_of(entity_id);
        if (entity_index == INDEX_INVALID || shell->match_state.entities[entity_index].health == 0) {
            log_warn("Script bot_add_squad: provided entity_id %u is does not refer to a living entity.", entity_id);
            lua_pop(lua_state, 1);
            continue;
        }

        if (entity_list.is_full()) {
            log_warn("bot_add_squad: entity_list is full.");
            lua_pop(lua_state, 1);
            break;
        }

        entity_list.push_back(entity_id);
        lua_pop(lua_state, 1);
    }
    lua_pop(lua_state, 1);

    // Create the squad
    int squad_id = BOT_SQUAD_ID_NULL;
    if (!entity_list.empty()) {
        squad_id = bot_add_squad(shell->bots[player_id], {
            .type = (BotSquadType)squad_type,
            .target_cell = target_cell,
            .entity_list = entity_list
        });
    }

    if (squad_id == BOT_SQUAD_ID_NULL) {
        log_warn("script_bot_add_squad() did not create a squad.");
    }

    lua_pushnumber(lua_state, squad_id);

    return 1;
}

// Checks if the squad exists.
// @param player_id number
// @param squad_id number
// @return boolean
int script_bot_squad_exists(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    const MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    int squad_id = (int)lua_tonumber(lua_state, 2);
    if (squad_id == BOT_SQUAD_ID_NULL) {
        script_error(lua_state, "Squad ID NULL is not valid.");
    }

    bool result = false;
    for (uint32_t squad_index = 0; squad_index < shell->bots[player_id].squads.size(); squad_index++) {
        const BotSquad& squad = shell->bots[player_id].squads[squad_index];
        if (squad.id == squad_id) {
            result = true;
            break;
        }
    }

    lua_pushboolean(lua_state, result);

    return 1;
}

// Returns the number of squads controlled by the specified bot
// @param player_id number
// @return number
int script_bot_get_squad_count(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShell* shell = script_get_match_shell();

    // Player
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_ACTIVE | SCRIPT_VALIDATE_PLAYER_IS_BOT);

    lua_pushnumber(lua_state, shell->bots[player_id].squads.size());

    return 1;
}

void script_push_squad_info(lua_State* lua_state, const BotSquad& squad) {
    lua_newtable(lua_state);

    // Squad ID
    lua_pushnumber(lua_state, squad.id);
    lua_setfield(lua_state, -2, "id");

    // Type
    lua_pushnumber(lua_state, squad.type);
    lua_setfield(lua_state, -2, "type");

    // Target cell
    script_lua_push_ivec2(lua_state, squad.target_cell);
    lua_setfield(lua_state, -2, "target_cell");

    // Patrol cell
    if (squad.type == BOT_SQUAD_TYPE_PATROL) {
        script_lua_push_ivec2(lua_state, squad.patrol_cell);
        lua_setfield(lua_state, -2, "patrol_cell");
    }

    // Entity list
    lua_createtable(lua_state, squad.entity_list.size(), 0);
    for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_list.size(); squad_entity_index++) {
        lua_pushnumber(lua_state, squad.entity_list[squad_entity_index]);
        lua_rawseti(lua_state, -2, squad_entity_index + 1);
    }
    lua_setfield(lua_state, -2, "entity_list");
}

// Returns a table of information about the squad matching the provided index
// @param player_id number
// @param index number
// @return table
int script_bot_get_squad_by_index(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    const MatchShell* shell = script_get_match_shell();

    // Player ID
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    // Squad index
    uint32_t squad_index = (uint32_t)lua_tonumber(lua_state, 2);
    if (squad_index >= shell->bots[player_id].squads.size()) {
        script_error(lua_state, "Squad index %u is not valid.", squad_index);
    }

    const BotSquad& squad = shell->bots[player_id].squads[squad_index];
    script_push_squad_info(lua_state, squad);

    return 1;
}

// Returns a table of information about the squad matching the provided ID
// @param player_id number
// @param squad_id number
// @return table
int script_bot_get_squad_by_id(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    const MatchShell* shell = script_get_match_shell();

    // Player ID
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    // Squad ID
    int squad_id = (int)lua_tonumber(lua_state, 2);
    if (squad_id == BOT_SQUAD_ID_NULL) {
        script_error(lua_state, "Squad ID NULL is not valid.");
    }

    // Validate squad ID
    uint32_t squad_index;
    for (squad_index = 0; squad_index < shell->bots[player_id].squads.size(); squad_index++) {
        const BotSquad& squad = shell->bots[player_id].squads[squad_index];
        if (squad.id == squad_id) {
            break;
        }
    }
    if (squad_index == shell->bots[player_id].squads.size()) {
        script_error(lua_state, "Bot %u squad %u does not exist.", player_id, squad_id);
    }

    // Populate squad table
    const BotSquad& squad = shell->bots[player_id].squads[squad_index];
    script_push_squad_info(lua_state, squad);

    return 1;
}

// Sets the target cell of the specified squad
// @param player_id number
// @param squad_id number
// @param target_cell ivec2
int script_bot_set_squad_target_cell(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER, LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 3);

    MatchShell* shell = script_get_match_shell();

    // Player ID
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    // Squad ID
    int squad_id = (int)lua_tonumber(lua_state, 2);
    if (squad_id == BOT_SQUAD_ID_NULL) {
        script_error(lua_state, "Squad ID NULL is not valid.");
    }

    // Validate squad ID
    uint32_t squad_index;
    for (squad_index = 0; squad_index < shell->bots[player_id].squads.size(); squad_index++) {
        const BotSquad& squad = shell->bots[player_id].squads[squad_index];
        if (squad.id == squad_id) {
            break;
        }
    }
    if (squad_index == shell->bots[player_id].squads.size()) {
        script_error(lua_state, "Bot %u squad %u does not exist.", player_id, squad_id);
    }

    // Target cell
    ivec2 target_cell = script_lua_to_ivec2(lua_state, 3, "arg 3");

    BotSquad& squad = shell->bots[player_id].squads[squad_index];
    squad.target_cell = target_cell;

    return 0;
}

// Add entities to an existing squad
// @param player_id number
// @param squad_id number
// @param entity_list table
int script_bot_add_entities_to_squad(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER, LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 3);

    MatchShell* shell = script_get_match_shell();

    // Player
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_ACTIVE | SCRIPT_VALIDATE_PLAYER_IS_BOT);

    // Squad ID
    int squad_id = (int)lua_tonumber(lua_state, 2);
    uint32_t squad_index;
    for (squad_index = 0; squad_index < shell->bots[player_id].squads.size(); squad_index++) {
        const BotSquad& squad = shell->bots[player_id].squads[squad_index];
        if (squad.id == squad_id) {
            break;
        }
    }

    if (squad_index == shell->bots[player_id].squads.size()) {
        script_error(lua_state, "Bot %u squad with ID %i does not exist.", player_id, squad_id);
    }

    BotSquad& squad = shell->bots[player_id].squads[squad_index];
    lua_pushnil(lua_state);
    while (lua_next(lua_state, 3)) {
        script_validate_type(lua_state, -1, "entity_list", LUA_TNUMBER);
        EntityId entity_id = (EntityId)lua_tonumber(lua_state, -1);
        uint32_t entity_index = shell->match_state.entities.get_index_of(entity_id);
        if (entity_index == INDEX_INVALID || shell->match_state.entities[entity_index].health == 0) {
            log_warn("Script bot_add_entities_to_squad: provided entity_id %u does not refer to a living unit.", entity_id);
            lua_pop(lua_state, 1);
            continue;
        }

        if (squad.entity_list.is_full()) {
            log_warn("Script bot_add_entities_to_squad: squad entity_list is full.");
            lua_pop(lua_state, 1);
            break;
        }

        bot_reserve_entity(shell->bots[player_id], entity_id);
        squad.entity_list.push_back(entity_id);
        lua_pop(lua_state, 1);
    }
    lua_pop(lua_state, 1);

    return 0;
}

// Gets the Squad ID of the squad that the entity is a part of
// Returns nil if the entity is not part of a squad
// @param entity_id number
// @return number | nil
int script_bot_get_entity_squad_id(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShell* shell = script_get_match_shell();

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 1);
    uint32_t entity_index = script_validate_entity_id(lua_state, shell, entity_id);

    uint8_t player_id = shell->match_state.entities[entity_index].player_id;
    const Bot& bot = shell->bots[player_id];

    uint32_t squad_index;
    for (squad_index = 0; squad_index < bot.squads.size(); squad_index++) {
        const BotSquad& squad = bot.squads[squad_index];

        bool squad_has_entity = false;
        for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_list.size(); squad_entity_index++) {
            if (squad.entity_list[squad_entity_index] == entity_id) {
                squad_has_entity = true;
                break;
            }
        }

        if (squad_has_entity) {
            break;
        }
    }

    if (squad_index < bot.squads.size()) {
        lua_pushnumber(lua_state, bot.squads[squad_index].id);
    } else {
        lua_pushnil(lua_state);
    }

    return 1;
}

// Sets a bot config flag to the specified value
// @param player_id number
// @param flag number
// @param value boolean
int script_bot_set_config_flag(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER, LUA_TBOOLEAN };
    script_validate_arguments(lua_state, arg_types, 3);

    MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    uint32_t flag = (uint32_t)lua_tonumber(lua_state, 2);
    uint32_t flag_index;
    for (flag_index = 0; flag_index < BOT_CONFIG_FLAG_COUNT; flag_index++) {
        uint32_t flag_value = 1U << flag_index;
        if (flag == flag_value) {
            break;
        }
    }
    if (flag_index == BOT_CONFIG_FLAG_COUNT) {
        script_error(lua_state, "%u is not a valid bot config flag.", flag);
    }

    bool value = lua_toboolean(lua_state, 3);

    bitflag_set(&shell->bots[player_id].config.flags, flag, value);

    return 0;
}

// Returns a list of entities that are allowed to be produced by this bot
// @param player_id number
int script_bot_get_allowed_entities(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    uint32_t allowed_entities_count = 0;
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (shell->bots[player_id].config.is_entity_allowed[entity_type]) {
            allowed_entities_count++;
        }
    }

    lua_createtable(lua_state, allowed_entities_count, 0);
    int allowed_entities_index = 1;
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (shell->bots[player_id].config.is_entity_allowed[entity_type]) {
            lua_pushnumber(lua_state, entity_type);
            lua_rawseti(lua_state, -2, allowed_entities_index);
            allowed_entities_index++;
        }
    }

    return 1;
}

// Sets the list of entities allowed by this bot
// @param player_id number
// @param entity_types table - A list of allowed entities types
int script_bot_set_allowed_entities(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 2);

    MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    memset(shell->bots[player_id].config.is_entity_allowed, 0, sizeof(shell->bots[player_id].config.is_entity_allowed));

    lua_pushnil(lua_state);
    while (lua_next(lua_state, 2) != 0) {
        int allowed_entity = (int)lua_tonumber(lua_state, -1);
        script_validate_entity_type(lua_state, allowed_entity);

        shell->bots[player_id].config.is_entity_allowed[allowed_entity] = true;
        lua_pop(lua_state, 1);
    }

    return 0;
}

// Returns true if the specified entity is reserved by the bot
// @param player_id number
// @param entity_id number
int script_bot_is_entity_reserved(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    const MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 2);
    script_validate_entity_id(lua_state, shell, entity_id);

    lua_pushboolean(lua_state, bot_is_entity_reserved(shell->bots[player_id], entity_id));

    return 1;
}

// Tells the specified bot to reserve the specified entity
// @param player_id number
// @param entity_id number
int script_bot_reserve_entity(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 2);
    uint32_t entity_index = script_validate_entity_id(lua_state, shell, entity_id);
    if (shell->match_state.entities[entity_index].player_id != player_id) {
        script_error(lua_state, "Bot tried to reserve an entity (%u) that it didn't own.", entity_id);
    }

    bot_reserve_entity(shell->bots[player_id], entity_id);

    return 0;
}

// Tells the specified bot o release the specified entity
// @param player_id number
// @param entity_id number
int script_bot_release_entity(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    MatchShell* shell = script_get_match_shell();

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 2);
    uint32_t entity_index = script_validate_entity_id(lua_state, shell, entity_id);

    if (shell->match_state.entities[entity_index].player_id != player_id) {
        script_error(lua_state, "Bot tried to reserve an entity (%u) that it didn't own.", entity_id);
    }

    bot_release_entity(shell->bots[player_id], entity_id);

    return 0;
}

// MATCH INPUT

void script_queue_match_input_get_entity_ids(lua_State* lua_state, uint8_t* entity_count, EntityId entity_ids[SELECTION_LIMIT]) {
    *entity_count = 0;

    lua_getfield(lua_state, 1, "entity_id");
    if (!lua_isnil(lua_state, -1)) {
        script_validate_type(lua_state, -1, "entity_id", LUA_TNUMBER);
        *entity_count = 1;
        entity_ids[0] = (EntityId)lua_tonumber(lua_state, -1);
    }
    lua_pop(lua_state, 1);

    if (*entity_count != 0) {
        return;
    }

    lua_getfield(lua_state, 1, "entity_ids");
    if (!lua_isnil(lua_state, -1)) {
        script_validate_type(lua_state, -1, "entity_ids", LUA_TTABLE);
        lua_pushnil(lua_state);
        while (lua_next(lua_state, -2) != 0) {
            script_validate_type(lua_state, -1, "entity_ids.element", LUA_TNUMBER);
            if (*entity_count == SELECTION_LIMIT) {
                script_error(lua_state, "Too many entities provided for match input.");
            }

            entity_ids[*entity_count] = (EntityId)lua_tonumber(lua_state, -1);
            (*entity_count)++;
            lua_pop(lua_state, 1);
        }
        lua_pop(lua_state, 1);
    }
    lua_pop(lua_state, 1);

    if (*entity_count == 0) {
        script_error(lua_state, "No entities provided for match input.");
    }
}

// Queues a match input
//
// MOVE { target_cell: ivec2|nil, target_id: number|nil }
// BUILD { building_type: number, building_cell: ivec2 }
// @param params { player_id: number, type: number, entity_id: number|nil, entity_ids: table|nil }
int script_queue_match_input(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();

    // Input type
    lua_getfield(lua_state, 1, "type");
    uint8_t input_type = (uint8_t)lua_tonumber(lua_state, -1);
    if (input_type >= MATCH_INPUT_TYPE_COUNT) {
        script_error(lua_state, "Invalid match input type %u", input_type);
    }
    lua_pop(lua_state, 1);

    // Player ID
    lua_getfield(lua_state, 1, "player_id");
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, -1);
    script_validate_player_id(lua_state, shell, player_id, SCRIPT_VALIDATE_PLAYER_IS_ACTIVE);
    lua_pop(lua_state, 1);

    MatchInput input;
    input.type = input_type;

    switch (input.type) {
        case MATCH_INPUT_NONE:
            break;
        case MATCH_INPUT_MOVE_CELL:
        case MATCH_INPUT_MOVE_ATTACK_CELL:
        case MATCH_INPUT_MOVE_UNLOAD:
        case MATCH_INPUT_MOVE_MOLOTOV: {
            input.move.target_id = ID_NULL;

            // Target cell
            lua_getfield(lua_state, 1, "target_cell");
            input.move.target_cell = script_lua_to_ivec2(lua_state, -1, "target_cell");
            lua_pop(lua_state, 1);

            // Shift command
            lua_getfield(lua_state, 1, "shift_command");
            if (!lua_isnil(lua_state, -1)) {
                script_validate_type(lua_state, -1, "shift_command", LUA_TBOOLEAN);
                input.move.shift_command = (uint8_t)lua_toboolean(lua_state, -1);
            } else {
                input.move.shift_command = 0;
            }
            lua_pop(lua_state, 1);

            script_queue_match_input_get_entity_ids(lua_state, &input.move.entity_count, input.move.entity_ids);
            break;
        }
        case MATCH_INPUT_MOVE_ENTITY:
        case MATCH_INPUT_MOVE_ATTACK_ENTITY:
        case MATCH_INPUT_MOVE_REPAIR: {
            input.move.target_cell = ivec2(-1, -1);

            // Target ID
            lua_getfield(lua_state, 1, "target_id");
            script_validate_type(lua_state, -1, "target_id", LUA_TNUMBER);
            input.move.target_id = (EntityId)lua_tonumber(lua_state, -1);
            lua_pop(lua_state, 1);

            // Shift command
            lua_getfield(lua_state, 1, "shift_command");
            if (!lua_isnil(lua_state, -1)) {
                script_validate_type(lua_state, -1, "shift_command", LUA_TBOOLEAN);
                input.move.shift_command = (uint8_t)lua_toboolean(lua_state, -1);
            } else {
                input.move.shift_command = 0;
            }
            lua_pop(lua_state, 1);

            script_queue_match_input_get_entity_ids(lua_state, &input.move.entity_count, input.move.entity_ids);
            break;
        }
        case MATCH_INPUT_STOP:
        case MATCH_INPUT_DEFEND: {
            script_queue_match_input_get_entity_ids(lua_state, &input.stop.entity_count, input.stop.entity_ids);

            break;
        }
        case MATCH_INPUT_BUILD: {
            // Building type
            lua_getfield(lua_state, 1, "building_type");
            int building_type = (int)lua_tonumber(lua_state, -1);
            script_validate_entity_type(lua_state, building_type);
            input.build.building_type = (EntityType)building_type;
            lua_pop(lua_state, 1);

            // Cell
            lua_getfield(lua_state, 1, "building_cell");
            script_validate_type(lua_state, -1, "building_cell", LUA_TTABLE);
            input.build.target_cell = script_lua_to_ivec2(lua_state, -1, "building_cell");
            lua_pop(lua_state, 1);

            // Shift command
            lua_getfield(lua_state, 1, "shift_command");
            if (!lua_isnil(lua_state, -1)) {
                script_validate_type(lua_state, -1, "shift_command", LUA_TBOOLEAN);
                input.move.shift_command = (uint8_t)lua_toboolean(lua_state, -1);
            } else {
                input.move.shift_command = 0;
            }
            lua_pop(lua_state, 1);

            script_queue_match_input_get_entity_ids(lua_state, &input.build.entity_count, input.build.entity_ids);
            break;
        }
        case MATCH_INPUT_SINGLE_UNLOAD: {
            // Entity ID
            lua_getfield(lua_state, 1, "entity_id");
            EntityId entity_id = (EntityId)lua_tonumber(lua_state, -1);
            script_validate_entity_id(lua_state, shell, entity_id);
            input.single_unload.entity_id = entity_id;
            lua_pop(lua_state, 1);
            break;
        }
        case MATCH_INPUT_BUILDING_ENQUEUE: {
            const uint8_t BUILDING_QUEUE_ITEM_TYPE_INVALID = UINT8_MAX;
            input.building_enqueue.item_type = BUILDING_QUEUE_ITEM_TYPE_INVALID;

            // Entity type
            lua_getfield(lua_state, 1, "entity_type");
            if (!lua_isnil(lua_state, -1)) {
                input.building_enqueue.item_type = BUILDING_QUEUE_ITEM_UNIT;
                script_validate_type(lua_state, -1, "entity_type", LUA_TNUMBER);
                input.building_enqueue.item_subtype = lua_tonumber(lua_state, -1);
                script_validate_entity_type(lua_state, input.building_enqueue.item_subtype);
            }
            lua_pop(lua_state, 1);

            // Upgrade
            lua_getfield(lua_state, 1, "upgrade");
            if (!lua_isnil(lua_state, -1)) {
                input.building_enqueue.item_type = BUILDING_QUEUE_ITEM_UPGRADE;
                script_validate_type(lua_state, -1, "upgrade", LUA_TNUMBER);
                input.building_enqueue.item_subtype = lua_tonumber(lua_state, -1);
                script_validate_upgrade(lua_state, input.building_enqueue.item_subtype);
            }
            lua_pop(lua_state, 1);

            if (input.building_enqueue.item_type == BUILDING_QUEUE_ITEM_TYPE_INVALID) {
                script_error(lua_state, "No item type provided for building_enqueue. Provide either entity_type or upgrade.");
            }

            script_queue_match_input_get_entity_ids(lua_state, &input.building_enqueue.building_count, input.building_enqueue.building_ids);
            break;
        }
        case MATCH_INPUT_RALLY: {
            // Rally point
            lua_getfield(lua_state, 1, "rally_cell");
            ivec2 rally_cell = script_lua_to_ivec2(lua_state, -1, "rally_cell");
            input.rally.rally_point = (rally_cell * TILE_SIZE) + ivec2(TILE_SIZE / 2, TILE_SIZE / 2);
            lua_pop(lua_state, 1);

            script_queue_match_input_get_entity_ids(lua_state, &input.rally.building_count, input.rally.building_ids);
            break;
        }
        default: {
            script_error(lua_state, "Match input type %s is not yet supported.", match_input_type_str((MatchInputType)input_type));
        }
    }

    if (player_id == network_get_player_id()) {
        shell->input_queue.push_back(input);
    } else {
        shell->inputs[player_id].push({ input });
    }

    return 0;
}

// Creates an avalanche at the specified position
// @param position ivec2
int script_create_avalanche_column(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();

    ivec2 position = script_lua_to_ivec2(lua_state, 1, "arg 1");

    shell->scenario_avalanche_columns.push_back((AvalancheColumn) {
        .animation = animation_create(ANIMATION_PARTICLE_AVALANCHE),
        .position = position,
        .destination = position + ivec2(0, SCREEN_HEIGHT)
    });

    return 0;
}

// Explodes a rigged goldmine
// @param goldmine_id number
int script_explode_rigged_goldmine(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShell* shell = script_get_match_shell();

    // Goldmine
    EntityId goldmine_id = (EntityId)lua_tonumber(lua_state, 1);
    script_validate_entity_id(lua_state, shell, goldmine_id);
    Entity& goldmine = shell->match_state.entities.get_by_id(goldmine_id);

    // Explosion particles
    const uint32_t EXPLOSION_COUNT = 4U;
    const ivec2 EXPLOSION_OFFSETS[EXPLOSION_COUNT] = {
        ivec2(5, 43),
        ivec2(17, 43),
        ivec2(26, 42),
        ivec2(39, 44)
    };
    const ivec2 BASE_POSITION = goldmine.cell * TILE_SIZE;
    for (uint32_t index = 0; index < EXPLOSION_COUNT; index++) {
        shell->match_state.particles.push_back((Particle) {
            .layer = PARTICLE_LAYER_GROUND,
            .sprite = SPRITE_PARTICLE_EXPLOSION,
            .animation = animation_create(ANIMATION_PARTICLE_EXPLOSION),
            .vframe = 0,
            .position = BASE_POSITION + EXPLOSION_OFFSETS[index],
        });
    }

    // Explosion sound
    match_event_play_sound(shell->match_state, SOUND_GOLD_MINE_COLLAPSE, goldmine.position.to_ivec2());

    // Update goldmine mode
    goldmine.mode = MODE_GOLDMINE;

    return 0;
}
