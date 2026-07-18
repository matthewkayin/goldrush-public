#pragma once

#include "defines.h"
#include "match/state/match.h"
#include "match/bot/bot.h"
#include "shared/match_setting.h"

#define SCENARIO_SQUAD_MAX_ENTITIES SELECTION_LIMIT
#define SCENARIO_CONSTANT_NAME_BUFFER_LENGTH 32

struct ScenarioPlayer {
    uint8_t team;
    uint8_t recolor_id;
    uint16_t padding = 0;
    char name[MAX_USERNAME_LENGTH + 4];
    uint32_t starting_gold;
};

struct ScenarioEntity {
    EntityType type;
    uint8_t player_id;
    bool is_rigged;
    uint32_t gold_held;
    ivec2 cell;
};

struct ScenarioSquad {
    char name[MAX_USERNAME_LENGTH + 1];
    uint8_t player_id;
    BotSquadType type;
    ivec2 patrol_cell;
    uint32_t entity_count;
    uint32_t entities[SCENARIO_SQUAD_MAX_ENTITIES];
};

enum ScenarioConstantType {
    SCENARIO_CONSTANT_TYPE_ENTITY,
    SCENARIO_CONSTANT_TYPE_CELL,
    SCENARIO_CONSTANT_TYPE_ENTITY_LIST,
    SCENARIO_CONSTANT_TYPE_COUNT
};

struct ScenarioConstantEntityList {
    EntityId entity_ids[SELECTION_LIMIT];
    uint32_t entity_count;
};

struct ScenarioConstant {
    char name[SCENARIO_CONSTANT_NAME_BUFFER_LENGTH];
    ScenarioConstantType type;
    union {
        uint32_t entity_index;
        ivec2 cell;
        ScenarioConstantEntityList entity_list;
    };
};

struct Scenario {
    MapType map_type;
    RawMap* raw_map;
    int map_bake_lcg_seed;

    ivec2 player_spawn;
    ScenarioPlayer players[MAX_PLAYERS];

    bool player_allowed_entities[ENTITY_TYPE_COUNT];
    uint32_t player_allowed_upgrades;
    BotConfig bot_config[MAX_PLAYERS - 1];

    uint32_t entity_count;
    ScenarioEntity entities[MATCH_MAX_ENTITIES];

    std::vector<ScenarioSquad> squads;
    std::vector<ScenarioConstant> constants;
};

// Init
Scenario* scenario_init(MapType map_type, MapSize map_size);
void scenario_free(Scenario* scenario);

// Squad
ScenarioSquad scenario_squad_init();
bool scenario_squads_are_equal(const ScenarioSquad& a, const ScenarioSquad& b);

// Constant
const char* scenario_constant_type_str(ScenarioConstantType type);
ScenarioConstantType scenario_constant_type_from_str(const char* str);
void scenario_constant_set_type(ScenarioConstant& constant, ScenarioConstantType type);

// File
std::string scenario_get_script_path(const char* full_path);

bool scenario_save_file(const Scenario* scenario, const char* json_full_path);
Scenario* scenario_open_file(const char* path);

bool scenario_export(const Scenario* scenario, const char* full_path);
Scenario* scenario_import(const char* path);
bool scenario_export_all();
