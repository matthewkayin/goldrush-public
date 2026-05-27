#pragma once

#include "match/shell/shell.h"
#include "match/scenario/scenario.h"

#define MODULE_NAME "scenario"

enum ScriptChatColor {
    SCRIPT_CHAT_COLOR_WHITE,
    SCRIPT_CHAT_COLOR_GOLD,
    SCRIPT_CHAT_COLOR_BLUE
};

enum ScriptAlertColor {
    SCRIPT_ALERT_COLOR_WHITE,
    SCRIPT_ALERT_COLOR_GOLD,
    SCRIPT_ALERT_COLOR_PLAYER
};

struct ScriptConstant {
    const char* name;
    int value;
};

const uint32_t SCRIPT_VALIDATE_PLAYER_IS_BOT = 1U << 0U;
const uint32_t SCRIPT_VALIDATE_PLAYER_IS_ACTIVE = 1U << 1U;

extern const luaL_reg GOLD_FUNCS[];

// Shell facing API
bool script_init(MatchShell* state, const Scenario* scenario, const char* script_path);
void script_update(MatchShell* state);

// Script helpers
void script_register_scenario_constants(lua_State* lua_state);
void script_call(MatchShell* shell, const char* func_name);
const char* script_get_entity_type_str(EntityType type);
const char* script_get_entity_mode_str(EntityMode mode);
const char* script_get_target_type_str(TargetType type);
const char* script_get_global_objective_counter_type_str(GlobalObjectiveCounterType type);
MatchShell* script_get_match_shell();
const char* script_lua_type_str(int lua_type);
void script_error(lua_State* lua_state, const char* message, ...);
void script_validate_type(lua_State* lua_state, int stack_index, const char* name, int expected_type);
void script_validate_arguments(lua_State* lua_state, const int* arg_types, int arg_count);
void script_validate_entity_type(lua_State* lua_state, int entity_type);
void script_validate_upgrade(lua_State* lua_state, uint32_t upgrade);
uint32_t script_validate_entity_id(lua_State* lua_state, const MatchShell* shell, EntityId entity_id);
ivec2 script_lua_to_ivec2(lua_State* lua_state, int stack_index, const char* name);
void script_lua_push_ivec2(lua_State* lua_state, ivec2 cell);
void script_validate_player_id(lua_State* lua_state, const MatchShell* shell, uint8_t player_id, uint32_t options = 0);
int script_sprintf(char* str_ptr, lua_State* lua_state, int stack_index);
