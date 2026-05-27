#include "script.h"

#include "core/logger.h"
#include "core/filesystem.h"
#include "core/resource.h"
#include "match/shell/shell.h"
#include "match/state/entity_data.h"
#include "match/state/match.h"
#include "util/util.h"
#include "util/bitflag.h"
#include "match/state/upgrade.h"

#define SCRIPT_LUA_TCDATA 10

static const ScriptConstant GOLD_CONSTANTS[] = {
    { "CHAT_COLOR_WHITE", SCRIPT_CHAT_COLOR_WHITE },
    { "CHAT_COLOR_GOLD", SCRIPT_CHAT_COLOR_GOLD },
    { "CHAT_COLOR_BLUE", SCRIPT_CHAT_COLOR_GOLD },
    { "ALERT_COLOR_WHITE", SCRIPT_ALERT_COLOR_WHITE },
    { "ALERT_COLOR_GOLD", SCRIPT_ALERT_COLOR_GOLD },
    { "ALERT_COLOR_PLAYER", SCRIPT_ALERT_COLOR_PLAYER },
    { "SQUAD_ID_NULL", BOT_SQUAD_ID_NULL },
    { "CAMERA_MODE_FREE", CAMERA_MODE_FREE },
    { "CAMERA_MODE_MINIMAP_DRAG", CAMERA_MODE_MINIMAP_DRAG },
    { "CAMERA_MODE_PAN", CAMERA_MODE_PAN },
    { "CAMERA_MODE_HELD", CAMERA_MODE_HELD },
    { "PLAYER_NONE", PLAYER_NONE },
    { "ID_NULL", ID_NULL },
    { NULL, 0 }
};

static MatchShell* _shell = NULL;

bool script_init(MatchShell* shell, const Scenario* scenario, const char* script_path) {
    // Check for script existance
    {
        FILE* script_file = fopen(script_path, "r");
        if (script_file == NULL) {
            log_error("Could not open script file %s.", script_path);
            return false;
        }
        fclose(script_file);
    }

    // Init lua state
    shell->scenario_lua_state = luaL_newstate();
    luaL_openlibs(shell->scenario_lua_state);

    // Register scenario library
    luaL_register(shell->scenario_lua_state, MODULE_NAME, GOLD_FUNCS);

    // Set module path
    lua_getglobal(shell->scenario_lua_state, "package");
    lua_getfield(shell->scenario_lua_state, -1, "path");

    const char* current_package_path = lua_tostring(shell->scenario_lua_state, -1);
    std::string new_package_path =
        std::string(current_package_path) + ";" +
        filesystem_get_scenario_path() + "modules/?.lua" + ";" +
        filesystem_get_path_folder(script_path) + "?.lua";
    log_debug("Lua module path: %s", new_package_path.c_str());

    lua_pushstring(shell->scenario_lua_state, new_package_path.c_str());
    lua_setfield(shell->scenario_lua_state, -3, "path");
    lua_pop(shell->scenario_lua_state, 2);

    // Save a pointer to the shell state
    _shell = shell;

    // Register scenario constants
    script_register_scenario_constants(shell->scenario_lua_state);

    // Scenario file constants
    lua_getglobal(shell->scenario_lua_state, MODULE_NAME);
    lua_newtable(shell->scenario_lua_state);
    for (const ScenarioConstant& constant : scenario->constants) {
        switch (constant.type) {
            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                lua_pushinteger(shell->scenario_lua_state, constant.entity_index);
                break;
            }
            case SCENARIO_CONSTANT_TYPE_CELL: {
                lua_newtable(shell->scenario_lua_state);
                lua_pushinteger(shell->scenario_lua_state, constant.cell.x);
                lua_setfield(shell->scenario_lua_state, -2, "x");
                lua_pushinteger(shell->scenario_lua_state, constant.cell.y);
                lua_setfield(shell->scenario_lua_state, -2, "y");
                break;
            }
            case SCENARIO_CONSTANT_TYPE_ENTITY_LIST: {
                lua_createtable(shell->scenario_lua_state, constant.entity_list.entity_count, 0);
                for (uint32_t index = 0; index < constant.entity_list.entity_count; index++) {
                    lua_pushnumber(shell->scenario_lua_state, constant.entity_list.entity_ids[index]);
                    lua_rawseti(shell->scenario_lua_state, -2, index + 1);
                }
                break;
            }
            case SCENARIO_CONSTANT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }
        lua_setfield(shell->scenario_lua_state, -2, constant.name);
    }
    lua_setfield(shell->scenario_lua_state, -2, "constants");

    // End scenario module constants
    lua_pop(shell->scenario_lua_state, 1);

    int dofile_error = luaL_dofile(shell->scenario_lua_state, script_path);
    if (dofile_error) {
        log_error("Error loading script file %s. Code %u: %s", script_path, dofile_error, lua_tostring(shell->scenario_lua_state, -1));
        lua_close(shell->scenario_lua_state);
        return false;
    }

    // Check to make sure that scenario_update() exists
    lua_getglobal(shell->scenario_lua_state, "scenario_update");
    if (!lua_isfunction(shell->scenario_lua_state, -1)) {
        log_error("Script %s is missing scenario_update() function.", script_path);
        lua_pop(shell->scenario_lua_state, 1);
        lua_close(shell->scenario_lua_state);
        return false;
    }
    // Pop scenario_update() off the stack because we are not calling it
    lua_pop(shell->scenario_lua_state, 1);

    // Get the scenario_init() function
    lua_getglobal(shell->scenario_lua_state, "scenario_init");
    if (!lua_isfunction(shell->scenario_lua_state, -1)) {
        log_error("Script %s is missing scenario_init() function.", script_path);
        lua_pop(shell->scenario_lua_state, 1);
        lua_close(shell->scenario_lua_state);
        return false;
    }
    lua_pop(shell->scenario_lua_state, 1);

    // Set lua random seed
#ifdef GOLD_DEBUG
    int stack_size_before = lua_gettop(shell->scenario_lua_state);
#endif
    lua_getglobal(shell->scenario_lua_state, "math");
    lua_getfield(shell->scenario_lua_state, -1, "randomseed");
    lua_remove(shell->scenario_lua_state, -2);
    lua_pushnumber(shell->scenario_lua_state, shell->match_state.lcg_seed);
    lua_call(shell->scenario_lua_state, 1, 0);
#ifdef GOLD_DEBUG
    int stack_size_after = lua_gettop(shell->scenario_lua_state);
    GOLD_ASSERT(stack_size_after == stack_size_before);
#endif

    // Call scenario_init()
    script_call(shell, "scenario_init");

    return true;
}

void script_update(MatchShell* shell) {
    ZoneScoped;
    script_call(shell, "scenario_update");
}

void script_register_scenario_constants(lua_State* lua_state) {
    // Push scenario table onto the stack
    lua_getglobal(lua_state, MODULE_NAME);

    // The scenario table should always exist when this function is called
    GOLD_ASSERT(!lua_isnil(lua_state, -1));

    // Script constants
    size_t const_index = 0;
    while (GOLD_CONSTANTS[const_index].name != NULL) {
        lua_pushinteger(lua_state, GOLD_CONSTANTS[const_index].value);
        lua_setfield(lua_state, -2, GOLD_CONSTANTS[const_index].name);

        const_index++;
    }

    // Player ID constant
    lua_pushinteger(lua_state, 0);
    lua_setfield(lua_state, -2, "PLAYER_ID");

    // Entity constants
    lua_createtable(lua_state, 0, ENTITY_TYPE_COUNT);
    for (int entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        lua_pushinteger(lua_state, entity_type);
        lua_setfield(lua_state, -2, script_get_entity_type_str((EntityType)entity_type));
    }
    lua_setfield(lua_state, -2, "entity_type");

    // Entity mode
    lua_createtable(lua_state, 0, MODE_COUNT);
    for (int entity_mode = 0; entity_mode < MODE_COUNT; entity_mode++) {
        lua_pushinteger(lua_state, entity_mode);
        lua_setfield(lua_state, -2, script_get_entity_mode_str((EntityMode)entity_mode));
    }
    lua_setfield(lua_state, -2, "entity_mode");

    // Target type
    lua_createtable(lua_state, 0, TARGET_TYPE_COUNT);
    for (int target_type = 0; target_type < TARGET_TYPE_COUNT; target_type++) {
        lua_pushinteger(lua_state, target_type);
        lua_setfield(lua_state, -2, script_get_target_type_str((TargetType)target_type));
    }
    lua_setfield(lua_state, -2, "target_type");

    // Upgrades
    lua_createtable(lua_state, 0, UPGRADE_COUNT);
    for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
        uint32_t upgrade = 1U << upgrade_index;

        char const_name[64];
        strcpy_to_upper(const_name, upgrade_get_data(upgrade).name);

        lua_pushnumber(lua_state, upgrade);
        lua_setfield(lua_state, -2, const_name);
    }
    lua_setfield(lua_state, -2, "upgrade");

    // Sound constants
    lua_createtable(lua_state, 0, SOUND_COUNT);
    for (int sound_name = 0; sound_name < SOUND_COUNT; sound_name++) {
        char const_name[64];
        strcpy_to_upper(const_name, sound_get_name((SoundName)sound_name));

        lua_pushinteger(lua_state, sound_name);
        lua_setfield(lua_state, -2, const_name);
    }
    lua_setfield(lua_state, -2, "sound");

    // Music track constants
    lua_createtable(lua_state, 0, MATCH_SHELL_MUSIC_TRACK_COUNT);
    for (uint32_t track = 0; track < MATCH_SHELL_MUSIC_TRACK_COUNT; track++) {
        char const_name[64];
        sprintf(const_name, "MATCH%u", track + 1);

        lua_pushinteger(lua_state, RESOURCE_MUSIC_MATCH1 + track);
        lua_setfield(lua_state, -2, const_name);
    }
    lua_setfield(lua_state, -2, "music");

    // Bot squad type constants
    lua_createtable(lua_state, 0, BOT_SQUAD_TYPE_COUNT);
    for (int squad_type = 0; squad_type < BOT_SQUAD_TYPE_COUNT; squad_type++) {
        char const_name[64];
        strcpy_to_upper(const_name, bot_squad_type_str((BotSquadType)squad_type));

        lua_pushinteger(lua_state, squad_type);
        lua_setfield(lua_state, -2, const_name);
    }
    lua_setfield(lua_state, -2, "bot_squad_type");

    // Bot config constants
    lua_createtable(lua_state, 0, BOT_CONFIG_FLAG_COUNT);
    for (uint32_t flag_index = 0; flag_index < BOT_CONFIG_FLAG_COUNT; flag_index++) {
        uint32_t flag_value = 1U << flag_index;

        char const_name[64];
        strcpy_to_upper(const_name, bot_config_flag_str(flag_value));

        lua_pushinteger(lua_state, flag_value);
        lua_setfield(lua_state, -2, const_name);
    }
    lua_setfield(lua_state, -2, "bot_config_flag");

    // Match input constants
    lua_createtable(lua_state, 0, MATCH_INPUT_TYPE_COUNT);
    for (uint32_t match_input_type = 0; match_input_type < MATCH_INPUT_TYPE_COUNT; match_input_type++) {
        lua_pushinteger(lua_state, match_input_type);
        lua_setfield(lua_state, -2, match_input_type_str((MatchInputType)match_input_type));
    }
    lua_setfield(lua_state, -2, "match_input_type");

    // Objective counter type constants
    lua_createtable(lua_state, 0, GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT);
    for (uint32_t counter_type = 0; counter_type < GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT; counter_type++) {
        lua_pushnumber(lua_state, counter_type);
        lua_setfield(lua_state, -2, script_get_global_objective_counter_type_str((GlobalObjectiveCounterType)counter_type));
    }
    lua_setfield(lua_state, -2, "global_objective_counter_type");

    // Pops scenario table off the stack
    lua_pop(lua_state, 1);
}

void script_call(MatchShell* shell, const char* func_name) {
#ifdef GOLD_DEBUG
    int stack_size_before = lua_gettop(shell->scenario_lua_state);
#endif

    lua_getglobal(shell->scenario_lua_state, "debug");
    lua_getfield(shell->scenario_lua_state, -1, "traceback");
    lua_remove(shell->scenario_lua_state, -2);
    lua_getglobal(shell->scenario_lua_state, func_name);
    if (lua_pcall(shell->scenario_lua_state, 0, 0, -2)) {
        const char* error_str = lua_tostring(shell->scenario_lua_state, -1);

        // Lua is not giving us the short_src so we will
        // manually look through the string to find the tail end
        // (past the path separators)

        size_t index = 0;
        size_t path_sep_index = 0;
        while (error_str[index] != ':') {
            if (error_str[index] == GOLD_PATH_SEPARATOR) {
                path_sep_index = index;
            }
            index++;
        }
        if (path_sep_index != 0) {
            error_str += path_sep_index + 1;
        }

        log_error("%s", error_str);
        match_shell_leave_match(shell, MATCH_SHELL_MODE_LEAVE_MATCH);
        return;
    }
    // Remove traceback from stack
    lua_pop(shell->scenario_lua_state, 1);

#ifdef GOLD_DEBUG
    int stack_size_after = lua_gettop(shell->scenario_lua_state);
    GOLD_ASSERT(stack_size_before == stack_size_after);
#endif
}

const char* script_get_entity_type_str(EntityType type) {
    switch (type) {
        case ENTITY_GOLDMINE:
            return "GOLDMINE";
        case ENTITY_CRATE:
            return "CRATE";
        case ENTITY_SWITCH:
            return "SWITCH";
        case ENTITY_MINER:
            return "MINER";
        case ENTITY_COWBOY:
            return "COWBOY";
        case ENTITY_BANDIT:
            return "BANDIT";
        case ENTITY_WAGON:
            return "WAGON";
        case ENTITY_JOCKEY:
            return "JOCKEY";
        case ENTITY_SAPPER:
            return "SAPPER";
        case ENTITY_PYRO:
            return "PYRO";
        case ENTITY_SOLDIER:
            return "SOLDIER";
        case ENTITY_CANNON:
            return "CANNON";
        case ENTITY_DETECTIVE:
            return "DETECTIVE";
        case ENTITY_BALLOON:
            return "BALLOON";
        case ENTITY_HALL:
            return "HALL";
        case ENTITY_HOUSE:
            return "HOUSE";
        case ENTITY_SALOON:
            return "SALOON";
        case ENTITY_BUNKER:
            return "BUNKER";
        case ENTITY_WORKSHOP:
            return "WORKSHOP";
        case ENTITY_SMITH:
            return "SMITH";
        case ENTITY_COOP:
            return "COOP";
        case ENTITY_BARRACKS:
            return "BARRACKS";
        case ENTITY_SHERIFFS:
            return "SHERIFFS";
        case ENTITY_LANDMINE:
            return "LANDMINE";
        case ENTITY_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* script_get_entity_mode_str(EntityMode mode) {
    switch (mode) {
        case MODE_UNIT_IDLE:
            return "UNIT_IDLE";
        case MODE_UNIT_BLOCKED:
            return "UNIT_BLOCKED";
        case MODE_UNIT_MOVE:
            return "UNIT_MOVE";
        case MODE_UNIT_MOVE_FINISHED:
            return "UNIT_MOVE_FINISHED";
        case MODE_UNIT_BUILD:
            return "UNIT_BUILD";
        case MODE_UNIT_BUILD_ASSIST:
            return "UNIT_BUILD_ASSIST";
        case MODE_UNIT_REPAIR:
            return "UNIT_REPAIR";
        case MODE_UNIT_ATTACK_WINDUP:
            return "UNIT_ATTACK_WINDUP";
        case MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP:
            return "UNIT_SOLDIER_RANGED_ATTACK_WINDUP";
        case MODE_UNIT_SOLDIER_CHARGE:
            return "UNIT_SOLDIER_CHARGE";
        case MODE_UNIT_IN_MINE:
            return "UNIT_IN_MINE";
        case MODE_UNIT_PYRO_THROW:
            return "UNIT_PYRO_THROW";
        case MODE_UNIT_DEATH:
            return "UNIT_DEATH";
        case MODE_UNIT_DEATH_FADE:
            return "UNIT_DEATH_FADE";
        case MODE_UNIT_BALLOON_DEATH_START:
            return "UNIT_BALLOON_DEATH_START";
        case MODE_UNIT_BALLOON_DEATH:
            return "UNIT_BALLOON_DEATH";
        case MODE_BUILDING_IN_PROGRESS:
            return "BUILDING_IN_PROGRESS";
        case MODE_BUILDING_FINISHED:
            return "BUILDING_FINISHED";
        case MODE_BUILDING_DESTROYED:
            return "BUILDING_DESTROYED";
        case MODE_MINE_ARM:
            return "MINE_ARM";
        case MODE_MINE_PRIME:
            return "MINE_PRIME";
        case MODE_GOLDMINE:
            return "GOLDMINE";
        case MODE_GOLDMINE_COLLAPSED:
            return "GOLDMINE_COLLAPSED";
        case MODE_GOLDMINE_RIGGED:
            return "GOLDMINE_RIGGED";
        case MODE_SWITCH_UP:
            return "SWITCH_UP";
        case MODE_SWITCH_DOWN:
            return "SWITCH_DOWN";
        case MODE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* script_get_target_type_str(TargetType type) {
    switch (type) {
        case TARGET_NONE:
            return "NONE";
        case TARGET_CELL:
            return "CELL";
        case TARGET_ENTITY:
            return "ENTITY";
        case TARGET_ATTACK_CELL:
            return "ATTACK_CELL";
        case TARGET_ATTACK_ENTITY:
            return "ATTACK_ENTITY";
        case TARGET_REPAIR:
            return "REPAIR";
        case TARGET_UNLOAD:
            return "UNLOAD";
        case TARGET_MOLOTOV:
            return "MOLOTOV";
        case TARGET_BUILD:
            return "BUILD";
        case TARGET_BUILD_ASSIST:
            return "BUILD_ASSIST";
        case TARGET_PATROL:
            return "PATROL";
        case TARGET_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* script_get_global_objective_counter_type_str(GlobalObjectiveCounterType type) {
    switch (type) {
        case GLOBAL_OBJECTIVE_COUNTER_OFF:
            return "OFF";
        case GLOBAL_OBJECTIVE_COUNTER_GOLD:
            return "GOLD";
        case GLOBAL_OBJECTIVE_COUNTER_COUNTDOWN:
            return "COUNTDOWN";
        case GLOBAL_OBJECTIVE_COUNTER_VARIABLE:
            return "VARIABLE";
        case GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

MatchShell* script_get_match_shell() {
    return _shell;
}

const char* script_lua_type_str(int lua_type) {
    switch (lua_type) {
        case LUA_TNIL:
            return "<nil>";
        case LUA_TNUMBER:
            return "<number>";
        case LUA_TBOOLEAN:
            return "<boolean>";
        case LUA_TSTRING:
            return "<string>";
        case LUA_TTABLE:
            return "<table>";
        case LUA_TFUNCTION:
            return "<function>";
        case LUA_TUSERDATA:
            return "<userdata>";
        case LUA_TTHREAD:
            return "<thread>";
        case LUA_TLIGHTUSERDATA:
            return "<lightuserdata>";
        case SCRIPT_LUA_TCDATA:
            return "<cdata>";
        default:
            log_debug("unknown type has number %i", lua_type);
            return "<type-unknown>";
    }
}

void script_error(lua_State* lua_state, const char* message, ...) {
    const size_t ERROR_BUFFER_LENGTH = 1024;
    char error_str[ERROR_BUFFER_LENGTH];
    char* error_str_ptr = error_str;

    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    vsnprintf(error_str_ptr, ERROR_BUFFER_LENGTH, message, arg_ptr);
    va_end(arg_ptr);

    lua_pushstring(lua_state, error_str);
    lua_error(lua_state);
}

void script_validate_type(lua_State* lua_state, int stack_index, const char* name, int expected_type) {
    int received_type = lua_type(lua_state, stack_index);
    if (received_type != expected_type) {
        script_error(lua_state, "Invalid type for %s. Received %s. Expected %s.",
            name,
            script_lua_type_str(received_type),
            script_lua_type_str(expected_type));
    }
}

void script_validate_arguments(lua_State* lua_state, const int* arg_types, int arg_count) {
    int arg_number = lua_gettop(lua_state);
    if (arg_number != arg_count) {
        script_error(lua_state, "Invalid arg count. Received %u. Expected %u.", arg_number, arg_count);
    }

    for (int arg_index = 1; arg_index <= arg_count; arg_index++) {
        char field_name[16];
        sprintf(field_name, "arg %u", arg_index);
        script_validate_type(lua_state, arg_index, field_name, arg_types[arg_index - 1]);
    }
}

void script_validate_entity_type(lua_State* lua_state, int entity_type) {
    if (entity_type < 0 || entity_type >= ENTITY_TYPE_COUNT) {
        script_error(lua_state, "Entity type %i not recognized.", entity_type);
    }
}

void script_validate_upgrade(lua_State* lua_state, uint32_t upgrade) {
    uint32_t upgrade_index;
    for (upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
        uint32_t upgrade_flag = 1U << upgrade_index;
        if (upgrade == upgrade_flag) {
            break;
        }
    }

    if (upgrade_index == UPGRADE_COUNT) {
        script_error(lua_state, "Upgrade type %u not recognized.", upgrade);
    }
}

uint32_t script_validate_entity_id(lua_State* lua_state, const MatchShell* shell, EntityId entity_id) {
    uint32_t entity_index = shell->match_state.entities.get_index_of(entity_id);
    if (entity_index == INDEX_INVALID) {
        script_error(lua_state, "Entity ID %u does not exist.", entity_id);
    }

    return entity_index;
}

ivec2 script_lua_to_ivec2(lua_State* lua_state, int stack_index, const char* name) {
    ivec2 value;

    int received_type = lua_type(lua_state, stack_index);
    if (received_type != LUA_TTABLE && received_type != SCRIPT_LUA_TCDATA) {
        script_error(lua_state, "Invalid type for %s. Received %s. Expected ivec2.", name, script_lua_type_str(received_type));
    }

    lua_getfield(lua_state, stack_index, "x");
    if (lua_type(lua_state, -1) != LUA_TNUMBER) {
        script_error(lua_state, "Invalid ivec2 %s: x is not a number.", name);
    }
    value.x = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    lua_getfield(lua_state, stack_index, "y");
    if (lua_type(lua_state, -1) != LUA_TNUMBER) {
        script_error(lua_state, "Invalid ivec2 %s: y is not a number.", name);
    }
    value.y = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    return value;
}

void script_lua_push_ivec2(lua_State* lua_state, ivec2 cell) {
    lua_createtable(lua_state, 0, 2);

    lua_pushnumber(lua_state, cell.x);
    lua_setfield(lua_state, -2, "x");

    lua_pushnumber(lua_state, cell.y);
    lua_setfield(lua_state, -2, "y");
}

void script_validate_player_id(lua_State* lua_state, const MatchShell* shell, uint8_t player_id, uint32_t options) {
    if (player_id >= MAX_PLAYERS) {
        script_error(lua_state, "player_id %u is out of range.", player_id);
    }
    if (bitflag_check(options, SCRIPT_VALIDATE_PLAYER_IS_BOT) && player_id == 0) {
        script_error(lua_state, "player_id is 0, but it must be a bot.");
    }
    if (bitflag_check(options, SCRIPT_VALIDATE_PLAYER_IS_ACTIVE) && shell->match_state.players[player_id].mode != PLAYER_MODE_ACTIVE) {
        script_error(lua_state, "player %u is inactive.", player_id);
    }
}

int script_sprintf(char* str_ptr, lua_State* lua_state, int stack_index) {
    int arg_type = lua_type(lua_state, stack_index);
    switch (arg_type) {
        case LUA_TNIL: {
            return sprintf(str_ptr, "nil");
        }
        case LUA_TNUMBER: {
            double value = lua_tonumber(lua_state, stack_index);
            return sprintf(str_ptr, "%f", value);
        }
        case LUA_TBOOLEAN: {
            bool value = lua_toboolean(lua_state, stack_index);
            return sprintf(str_ptr, "%s", value ? "true" : "false");
        }
        case LUA_TSTRING: {
            const char* value = lua_tostring(lua_state, stack_index);
            return sprintf(str_ptr, "%s", value);
        }
        case LUA_TTABLE: {
            size_t offset = 0;
            offset += sprintf(str_ptr + offset, "{ ");
            lua_pushnil(lua_state);
            while (lua_next(lua_state, stack_index) != 0) {
                int key_type = lua_type(lua_state, -2);
                if (key_type == LUA_TSTRING) {
                    offset += sprintf(str_ptr + offset, "%s = ", lua_tostring(lua_state, -2));
                }
                offset += script_sprintf(str_ptr + offset, lua_state, lua_gettop(lua_state));
                offset += sprintf(str_ptr + offset, ", ");
                lua_pop(lua_state, 1);
            }
            offset += sprintf(str_ptr + offset, "}");
            return offset;
        }
        default: {
            return sprintf(str_ptr, "%s", script_lua_type_str(arg_type));
        }
    }
}
