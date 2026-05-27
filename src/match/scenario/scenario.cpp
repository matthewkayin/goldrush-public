#include "scenario.h"

#include "match/state/map_gen.h"
#include "util/util.h"
#include "util/bitflag.h"
#include "util/json.h"
#include "core/filesystem.h"
#include "match/state/upgrade.h"

static const uint32_t SCENARIO_FILE_SIGNATURE = 0x46597267;

// INIT

Scenario* scenario_init(MapType map_type, MapSize map_size) {
    Scenario* scenario = new Scenario();

    scenario->map_type = map_type;
    scenario->map_bake_lcg_seed = rand();

    int map_tile_size = map_get_tile_size(map_size);
    scenario->raw_map = raw_map_init(map_tile_size, map_tile_size);

    scenario->player_spawn = ivec2(0, 0);
    scenario->entity_count = 0;

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (player_id == 0) {
            sprintf(scenario->players[player_id].name, "Player");
        } else {
            sprintf(scenario->players[player_id].name, "Enemy %u", player_id);
        }
        scenario->players[player_id].starting_gold = 50;
        scenario->players[player_id].team = player_id;
        scenario->players[player_id].recolor_id = player_id;
    }

    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        scenario->player_allowed_entities[entity_type] = true;
    }

    for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
        scenario->player_allowed_upgrades |= 1U << upgrade_index;
    }

    // Init bot configs
    for (uint32_t bot_index = 0; bot_index < MAX_PLAYERS - 1; bot_index++) {
        scenario->bot_config[bot_index] = bot_config_init_from_difficulty(DIFFICULTY_HARD);
        scenario->bot_config[bot_index].allowed_upgrades = 0;
        scenario->bot_config[bot_index].flags = 0;
        scenario->bot_config[bot_index].target_base_count = 0;
        memset(scenario->bot_config[bot_index].is_entity_allowed, 0, sizeof(scenario->bot_config[bot_index].is_entity_allowed));
    }

    return scenario;
}

void scenario_free(Scenario* scenario) {
    if (scenario->raw_map != NULL) {
        free(scenario->raw_map);
    }
    delete scenario;
}

// SQUAD

ScenarioSquad scenario_squad_init() {
    ScenarioSquad squad;
    sprintf(squad.name, "New Squad");
    squad.player_id = 1;
    squad.type = BOT_SQUAD_TYPE_DEFEND;
    squad.patrol_cell = ivec2(-1, -1);
    squad.entity_count = 0;

    return squad;
}

bool scenario_squads_are_equal(const ScenarioSquad& a, const ScenarioSquad& b) {
    if (strcmp(a.name, b.name) != 0) {
        return false;
    }
    if (a.player_id != b.player_id ||
            a.type != b.type ||
            a.entity_count != b.entity_count ||
            a.patrol_cell != b.patrol_cell) {
        return false;
    }
    for (uint32_t index = 0; index < a.entity_count; index++) {
        if (a.entities[index] != b.entities[index]) {
            return false;
        }
    }
    return true;
}

// CONSTANT

const char* scenario_constant_type_str(ScenarioConstantType type) {
    switch (type) {
        case SCENARIO_CONSTANT_TYPE_ENTITY:
            return "Entity";
        case SCENARIO_CONSTANT_TYPE_CELL:
            return "Cell";
        case SCENARIO_CONSTANT_TYPE_ENTITY_LIST:
            return "Entity List";
        case SCENARIO_CONSTANT_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

ScenarioConstantType scenario_constant_type_from_str(const char* str) {
    return (ScenarioConstantType)enum_from_str(str, (EnumToStrFn)scenario_constant_type_str, SCENARIO_CONSTANT_TYPE_COUNT);
}

void scenario_constant_set_type(ScenarioConstant& constant, ScenarioConstantType type) {
    constant.type = type;
    switch (type) {
        case SCENARIO_CONSTANT_TYPE_ENTITY: {
            constant.entity_index = INDEX_INVALID;
            break;
        }
        case SCENARIO_CONSTANT_TYPE_CELL: {
            constant.cell = ivec2(0, 0);
            break;
        }
        case SCENARIO_CONSTANT_TYPE_ENTITY_LIST: {
            constant.entity_list.entity_count = 0;
            break;
        }
        case SCENARIO_CONSTANT_TYPE_COUNT: {
            break;
        }
    }
}

// FILE

std::string scenario_get_script_path(const char* full_path) {
    std::string folder_path = filesystem_get_path_folder(full_path);
    log_debug("Folder path %s", folder_path.c_str());
    std::string short_path = std::string(full_path).substr(folder_path.length());
    log_debug("Short path %s", short_path.c_str());
    std::string short_path_without_extension = short_path.substr(0, short_path.find('.'));
    log_debug("Short path without extension %s", short_path_without_extension.c_str());
    std::string script_path = folder_path + short_path_without_extension + ".lua";
    log_debug("Script path %s", script_path.c_str());
    return script_path;
}

bool scenario_save_file(const Scenario* scenario, const char* json_full_path) {
    // Build scenario json
    Json* scenario_json = json_object();

    // Map type
    json_object_set_string(scenario_json, "map_type", match_setting_data(MATCH_SETTING_MAP_TYPE).values.at(scenario->map_type).c_str());

    // Make bake LCG seed
    json_object_set_number(scenario_json, "map_bake_lcg_seed", scenario->map_bake_lcg_seed);

    // Raw map
    Json* map_json = json_object();
        json_object_set_number(map_json, "width", scenario->raw_map->width);
        json_object_set_number(map_json, "height", scenario->raw_map->height);

        // Raw map data
        Json* map_data_json = json_array();
        for (int index = 0; index < scenario->raw_map->width * scenario->raw_map->height; index++) {
            json_array_push_number(map_data_json, scenario->raw_map->data[index]);
        }
        json_object_set(map_json, "data", map_data_json);
    json_object_set(scenario_json, "map", map_json);

    // Player spawn
    json_object_set(scenario_json, "player_spawn", json_from_ivec2(scenario->player_spawn));

    // Players
    Json* players_json = json_array();
    for (size_t index = 0; index < MAX_PLAYERS; index++) {
        // Base player properties
        Json* player_json = json_object();
        json_object_set_string(player_json, "name", scenario->players[index].name);
        json_object_set_number(player_json, "team", scenario->players[index].team);
        json_object_set_number(player_json, "recolor_id", scenario->players[index].recolor_id);
        json_object_set_number(player_json, "starting_gold", scenario->players[index].starting_gold);

        // Player config
        if (index != 0) {
            const BotConfig& bot_config = scenario->bot_config[index - 1];

            // Opener
            json_object_set(player_json, "opener", json_string(bot_config_opener_str(bot_config.opener)));

            // Unit comp
            json_object_set(player_json, "preferred_unit_comp", json_string(bot_config_unit_comp_str(bot_config.preferred_unit_comp)));

            // Bot flags
            Json* flags_json = json_array();
            for (uint32_t flag_index = 0; flag_index < BOT_CONFIG_FLAG_COUNT; flag_index++) {
                uint32_t flag = 1U << flag_index;
                if (!bitflag_check(bot_config.flags, flag)) {
                    continue;
                }

                json_array_push_string(flags_json, bot_config_flag_str(flag));
            }
            json_object_set(player_json, "flags", flags_json);

            // Bot config variables
            json_object_set_number(player_json, "target_base_count", bot_config.target_base_count);
            json_object_set_number(player_json, "macro_cycle_cooldown", bot_config.macro_cycle_cooldown);
        }

        // Allowed upgrades
        Json* upgrades_json = json_array();
        uint32_t allowed_upgrades = index == 0
            ? scenario->player_allowed_upgrades
            : scenario->bot_config[index - 1].allowed_upgrades;
        for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
            uint32_t upgrade = 1U << upgrade_index;
            if (!bitflag_check(allowed_upgrades, upgrade)) {
                continue;
            }

            json_array_push_string(upgrades_json, upgrade_get_data(upgrade).name);
        }
        json_object_set(player_json, "allowed_upgrades", upgrades_json);

        // Allowed entities
        Json* allowed_entities_json = json_array();
        const bool* allowed_entities = index == 0
            ? scenario->player_allowed_entities
            : scenario->bot_config[index - 1].is_entity_allowed;
        for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
            if (!allowed_entities[entity_type]) {
                continue;
            }

            json_array_push_string(allowed_entities_json, entity_get_data((EntityType)entity_type).name);
        }
        json_object_set(player_json, "allowed_entities", allowed_entities_json);

        json_array_push(players_json, player_json);
    }
    json_object_set(scenario_json, "players", players_json);

    // Entities
    Json* entities_json = json_array();
    for (uint32_t entity_index = 0; entity_index < scenario->entity_count; entity_index++) {
        const ScenarioEntity& entity = scenario->entities[entity_index];

        Json* entity_json = json_object();
        json_object_set_string(entity_json, "type", entity_get_data(entity.type).name);
        json_object_set_number(entity_json, "player_id", entity.player_id);
        json_object_set_boolean(entity_json, "is_rigged", entity.is_rigged);
        json_object_set_number(entity_json, "gold_held", entity.gold_held);
        json_object_set(entity_json, "cell", json_from_ivec2(entity.cell));

        json_array_push(entities_json, entity_json);
    }
    json_object_set(scenario_json, "entities", entities_json);

    // Squads
    Json* squads_json = json_array();
    for (const ScenarioSquad& squad : scenario->squads) {
        Json* squad_json = json_object();
        json_object_set_string(squad_json, "name", squad.name);
        json_object_set_number(squad_json, "player_id", squad.player_id);
        json_object_set_string(squad_json, "type", bot_squad_type_str(squad.type));

        if (squad.type == BOT_SQUAD_TYPE_PATROL) {
            json_object_set(squad_json, "patrol_cell", json_from_ivec2(squad.patrol_cell));
        }

        Json* squad_entities_json = json_array();
        for (uint32_t entity_index = 0; entity_index < squad.entity_count; entity_index++) {
            json_array_push_number(squad_entities_json, squad.entities[entity_index]);
        }
        json_object_set(squad_json, "entities", squad_entities_json);

        json_array_push(squads_json, squad_json);
    }
    json_object_set(scenario_json, "squads", squads_json);

    // Constants
    Json* constants_json = json_array();
    for (const ScenarioConstant& constant : scenario->constants) {
        Json* constant_json = json_object();
        json_object_set_string(constant_json, "name", constant.name);
        json_object_set_string(constant_json, "type", scenario_constant_type_str(constant.type));
        switch (constant.type) {
            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                json_object_set_number(constant_json, "entity_index", constant.entity_index);
                break;
            }
            case SCENARIO_CONSTANT_TYPE_CELL: {
                json_object_set(constant_json, "cell", json_from_ivec2(constant.cell));
                break;
            }
            case SCENARIO_CONSTANT_TYPE_ENTITY_LIST: {
                Json* entity_list_json = json_array();
                for (uint32_t index = 0; index < constant.entity_list.entity_count; index++) {
                    json_array_push_number(entity_list_json, constant.entity_list.entity_ids[index]);
                }
                json_object_set(constant_json, "entity_list", entity_list_json);
                break;
            }
            case SCENARIO_CONSTANT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }

        json_array_push(constants_json, constant_json);
    }
    json_object_set(scenario_json, "constants", constants_json);

    // Save json
    bool json_save_success = json_write(scenario_json, json_full_path);
    if (json_save_success) {
        log_info("Scenario %s saved successfully.", json_full_path);
    } else {
        log_error("Unable to save scenario json at path %s", json_full_path);
    }

    json_free(scenario_json);

    return json_save_success;
}

Scenario* scenario_open_file(const char* path) {
    // Load json file
    Json* scenario_json = json_read(path);
    if (scenario_json == NULL) {
        log_error("Unable to open scenario json at path %s", path);
        return NULL;
    }

    // Map type
    const std::string map_type_string = json_object_get_string(scenario_json, "map_type");
    MapType map_type = MAP_TYPE_COUNT;
    for (uint32_t map_type_index = 0; map_type_index < MAP_TYPE_COUNT; map_type_index++) {
        if (match_setting_data(MATCH_SETTING_MAP_TYPE).values.at(map_type_index) == map_type_string) {
            map_type = (MapType)map_type_index;
            break;
        }
    }
    GOLD_ASSERT(map_type != MAP_TYPE_COUNT);

    // Map bake lcg seed
    Json* map_bake_lcg_seed_json = json_object_get(scenario_json, "map_bake_lcg_seed");
    const int map_bake_lcg_seed =
        map_bake_lcg_seed_json != NULL && map_bake_lcg_seed_json->type != JSON_TYPE_NUMBER
            ? (int)map_bake_lcg_seed_json->number.value
            : rand();

    // Map
    Json* map_json = json_object_get(scenario_json, "map");
    Json* noise_json = json_object_get(scenario_json, "noise");
    // Assert that we have at least the map or the noise, but not both
    GOLD_ASSERT((map_json != NULL && noise_json == NULL) || (map_json == NULL || noise_json != NULL));

    // Map size
    int map_tile_size = 0;
    if (map_json != NULL) {
        map_tile_size = (int)json_object_get_number(map_json, "width");
    }
    if (noise_json != NULL) {
        map_tile_size = (int)json_object_get_number(noise_json, "width");
    }
    MapSize map_size = map_get_map_size_from_tile_size(map_tile_size);
    GOLD_ASSERT(map_size != MAP_SIZE_COUNT);

    // Init scenario
    Scenario* scenario = scenario_init(map_type, map_size);
    scenario->map_type = map_type;
    scenario->map_bake_lcg_seed = map_bake_lcg_seed;

    // Copy map data
    if (map_json != NULL) {
        Json* map_data_json = json_object_get(map_json, "data");
        for (int index = 0; index < map_tile_size * map_tile_size; index++) {
            scenario->raw_map->data[index] = (uint8_t)json_array_get_number(map_data_json, index);
        }
    }

    // Noise-based map data copy
    if (noise_json != NULL) {
        Json* noise_map_json = json_object_get(noise_json, "map");
        for (int index = 0; index < map_tile_size * map_tile_size; index++) {
            scenario->raw_map->data[index] = (uint8_t)json_array_get_number(noise_map_json, index);
        }

        // Decorations
        Json* decorations_json = json_object_get(scenario_json, "decorations");
        for (size_t index = 0; index < decorations_json->array.length; index++) {
            Json* decoration_json = json_array_get(decorations_json, index);
            ivec2 cell = json_to_ivec2(json_object_get(decoration_json, "cell"));
            int decoration_map_index = cell.x + (cell.y * map_tile_size);
            scenario->raw_map->data[decoration_map_index] =
                scenario->raw_map->data[decoration_map_index] == MAP_VALUE_LOWGROUND
                    ? MAP_VALUE_DECORATION_LOWGROUND
                    : MAP_VALUE_DECORATION_HIGHGROUND;
        }
    }

    // Player spawn
    scenario->player_spawn = json_to_ivec2(json_object_get(scenario_json, "player_spawn"));

    // Players
    Json* players_json = json_object_get(scenario_json, "players");
    for (size_t index = 0; index < MAX_PLAYERS; index++) {
        // Base player properties
        Json* player_json = json_array_get(players_json, index);
        strncpy(scenario->players[index].name, json_object_get_string(player_json, "name"), MAX_USERNAME_LENGTH);
        scenario->players[index].starting_gold = (uint32_t)json_object_get_number(player_json, "starting_gold");

        // Team
        Json* player_team_json = json_object_get(player_json, "team");
        if (player_team_json != NULL) {
            scenario->players[index].team = (uint8_t)player_team_json->number.value;
        } else {
            log_warn("Scenario player index %u has no team. Using index as team.", index);
            scenario->players[index].team = (uint8_t)index;
        }

        // Recolor ID
        Json* player_recolor_id_json = json_object_get(player_json, "recolor_id");
        if (player_recolor_id_json != NULL) {
            scenario->players[index].recolor_id = (uint8_t)player_recolor_id_json->number.value;
        } else {
            log_warn("Scenario player index %u has no recolor_id. Using index as recolor_id.", index);
            scenario->players[index].recolor_id = (uint8_t)index;
        }

        // Player config
        if (index != 0) {
            scenario->bot_config[index - 1] = bot_config_init();
            BotConfig& bot_config = scenario->bot_config[index - 1];

            // Opener
            Json* opener_json = json_object_get(player_json, "opener");
            if (opener_json != NULL) {
                bot_config.opener = bot_config_opener_from_str(opener_json->string.value);
            }

            // Unit comp
            Json* preferred_unit_comp_json = json_object_get(player_json, "preferred_unit_comp");
            if (preferred_unit_comp_json != NULL) {
                bot_config.preferred_unit_comp = bot_config_unit_comp_from_str(preferred_unit_comp_json->string.value);
            }

            // Bot flags
            Json* flags_json = json_object_get(player_json, "flags");
            for (size_t flag_index = 0; flag_index < flags_json->array.length; flag_index++) {
                const char* flag_str = json_array_get_string(flags_json, flag_index);
                uint32_t flag = bot_config_flag_from_str(flag_str);
                if (flag == BOT_CONFIG_FLAG_COUNT) {
                    log_warn("Bot config flag %s for player %u not recognized.", flag_str, index);
                    continue;
                }
                bot_config.flags |= flag;
            }

            // Bot config variables
            bot_config.target_base_count = (uint32_t)json_object_get_number(player_json, "target_base_count");
            bot_config.macro_cycle_cooldown = (uint32_t)json_object_get_number(player_json, "macro_cycle_cooldown");
        }

        // Allowed upgrades
        uint32_t* allowed_upgrades = index == 0
            ? &scenario->player_allowed_upgrades
            : &scenario->bot_config[index - 1].allowed_upgrades;
        *allowed_upgrades = 0;
        Json* upgrades_json = json_object_get(player_json, "allowed_upgrades");
        for (size_t upgrades_array_index = 0; upgrades_array_index < upgrades_json->array.length; upgrades_array_index++) {
            std::string upgrade_str = std::string(json_array_get_string(upgrades_json, upgrades_array_index));
            uint32_t upgrade_index;
            for (upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
                uint32_t upgrade = 1U << upgrade_index;
                if (strcmp(upgrade_str.c_str(), upgrade_get_data(upgrade).name) == 0) {
                    break;
                }
            }

            if (upgrade_index < UPGRADE_COUNT) {
                uint32_t upgrade = 1U << upgrade_index;
                *allowed_upgrades |= upgrade;
            } else {
                log_warn("Allowed upgrade %s for player %u not recognized.", upgrade_str.c_str(), index);
                continue;
            }
        }

        // Allowed entities
        bool* allowed_entities = index == 0
            ? scenario->player_allowed_entities
            : scenario->bot_config[index - 1].is_entity_allowed;
        memset(allowed_entities, 0, ENTITY_TYPE_COUNT * sizeof(bool));
        Json* allowed_entities_json = json_object_get(player_json, "allowed_entities");
        for (size_t allowed_entities_index = 0; allowed_entities_index < allowed_entities_json->array.length; allowed_entities_index++) {
            const char* entity_str = json_array_get_string(allowed_entities_json, allowed_entities_index);
            EntityType entity_type = entity_type_from_str(entity_str);
            if (entity_type < ENTITY_TYPE_COUNT) {
                allowed_entities[entity_type] = true;
            } else {
                log_warn("Allowed entity %s for player %u not recognized.", entity_str, index);
                continue;
            }
        }
    } // End for each player

    // Entities
    Json* entities_json = json_object_get(scenario_json, "entities");
    for (size_t entity_index = 0; entity_index < entities_json->array.length; entity_index++) {
        Json* entity_json = json_array_get(entities_json, entity_index);

        const char* entity_str = json_object_get_string(entity_json, "type");
        EntityType entity_type = entity_type_from_str(entity_str);
        if (entity_type == ENTITY_TYPE_COUNT) {
            log_warn("Entity type %s for entity index %u not recognized.", entity_str, entity_index);
            continue;
        }

        ScenarioEntity entity;
        entity.type = entity_type;
        entity.player_id = (uint8_t)json_object_get_number(entity_json, "player_id");
        entity.gold_held = (uint32_t)json_object_get_number(entity_json, "gold_held");
        entity.cell = json_to_ivec2(json_object_get(entity_json, "cell"));

        Json* entity_is_rigged_json = json_object_get(entity_json, "is_rigged");
        entity.is_rigged =
            entity_is_rigged_json != NULL &&
            entity_is_rigged_json->type == JSON_TYPE_BOOLEAN &&
            entity_is_rigged_json->boolean.value;

        scenario->entities[scenario->entity_count] = entity;
        scenario->entity_count++;
    }

    // Squads
    Json* squads_json = json_object_get(scenario_json, "squads");
    for (size_t squad_index = 0; squad_index < squads_json->array.length; squad_index++) {
        Json* squad_json = json_array_get(squads_json, squad_index);

        const char* squad_type_str = json_object_get_string(squad_json, "type");
        BotSquadType squad_type = bot_squad_type_from_str(squad_type_str);
        if (squad_type == BOT_SQUAD_TYPE_COUNT) {
            log_warn("Squad type %s for squad %u not recognized.", squad_type_str, squad_index);
            continue;
        }

        ScenarioSquad squad;
        strncpy(squad.name, json_object_get_string(squad_json, "name"), MAX_USERNAME_LENGTH);
        squad.type = squad_type;
        squad.player_id = (uint8_t)json_object_get_number(squad_json, "player_id");

        Json* squad_patrol_cell_json = json_object_get(squad_json, "patrol_cell");
        if (squad_patrol_cell_json != NULL) {
            squad.patrol_cell = json_to_ivec2(squad_patrol_cell_json);
        }

        Json* squad_entities_json = json_object_get(squad_json, "entities");
        squad.entity_count = 0;
        for (size_t squad_entities_index = 0; squad_entities_index < squad_entities_json->array.length; squad_entities_index++) {
            uint32_t entity_index = (uint32_t)json_array_get_number(squad_entities_json, squad_entities_index);
            if (entity_index >= scenario->entity_count) {
                log_warn("Entity index %u for squad %u (%s) is out of range.", entity_index, squad_index, squad.name);
                continue;
            }
            squad.entities[squad.entity_count] = entity_index;
            squad.entity_count++;
        }

        scenario->squads.push_back(squad);
    }

    // Constants
    Json* constants_json = json_object_get(scenario_json, "constants");
    for (size_t constant_index = 0; constant_index < constants_json->array.length; constant_index++) {
        Json* constant_json = json_array_get(constants_json, constant_index);

        ScenarioConstant constant;
        strncpy(constant.name, json_object_get_string(constant_json, "name"), SCENARIO_CONSTANT_NAME_BUFFER_LENGTH);

        const char* constant_type_str = json_object_get_string(constant_json, "type");
        constant.type = scenario_constant_type_from_str(constant_type_str);
        if (constant.type == SCENARIO_CONSTANT_TYPE_COUNT) {
            log_warn("Constant type %s for constant %u:%s not recognized.", constant_type_str, constant_index, constant.name);
            continue;
        }

        switch (constant.type) {
            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                constant.entity_index = (uint32_t)json_object_get_number(constant_json, "entity_index");
                break;
            }
            case SCENARIO_CONSTANT_TYPE_CELL: {
                constant.cell = json_to_ivec2(json_object_get(constant_json, "cell"));
                break;
            }
            case SCENARIO_CONSTANT_TYPE_ENTITY_LIST: {
                Json* entity_list_json = json_object_get(constant_json, "entity_list");
                GOLD_ASSERT(entity_list_json->array.length <= SELECTION_LIMIT);
                constant.entity_list.entity_count = entity_list_json->array.length;
                for (uint32_t index = 0; index < entity_list_json->array.length; index++) {
                    constant.entity_list.entity_ids[index] = (EntityId)entity_list_json->array.values[index]->number.value;
                }
                break;
            }
            case SCENARIO_CONSTANT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }

        scenario->constants.push_back(constant);
    }

    json_free(scenario_json);
    log_info("Loaded scenario %s.", path);

    return scenario;
}

bool scenario_export(const Scenario* scenario, const char* full_path) {
    FILE* file = fopen(full_path, "wb");
    if (!file) {
        log_error("Scenario export - failed to open %s for writing.", full_path);
        return false;
    }

    // Signature
    fwrite(&SCENARIO_FILE_SIGNATURE, 1, sizeof(SCENARIO_FILE_SIGNATURE), file);

    // Map type
    uint32_t map_type = scenario->map_type;
    fwrite(&map_type, 1, sizeof(map_type), file);

    // Raw map
    raw_map_fwrite(scenario->raw_map, file);

    // Bake lcg seed
    fwrite(&scenario->map_bake_lcg_seed, 1, sizeof(scenario->map_bake_lcg_seed), file);

    // Player spawn
    fwrite(&scenario->player_spawn, 1, sizeof(scenario->player_spawn), file);

    // Players
    fwrite(scenario->players, 1, sizeof(scenario->players), file);

    // Allowed entities
    fwrite(scenario->player_allowed_entities, 1, sizeof(scenario->player_allowed_entities), file);

    // Allowed upgrades
    fwrite(&scenario->player_allowed_upgrades, 1, sizeof(scenario->player_allowed_upgrades), file);

    // Bot config
    fwrite(scenario->bot_config, 1, sizeof(scenario->bot_config), file);

    // Entity count
    fwrite(&scenario->entity_count, 1, sizeof(scenario->entity_count), file);

    // Entities
    for (uint32_t entity_index = 0; entity_index < scenario->entity_count; entity_index++) {
        fwrite(&scenario->entities[entity_index], 1, sizeof(scenario->entities[entity_index]), file);
    }

    // Squads
    uint32_t squad_count = scenario->squads.size();
    fwrite(&squad_count, 1, sizeof(squad_count), file);
    for (uint32_t squad_index = 0; squad_index < squad_count; squad_index++) {
        fwrite(&scenario->squads[squad_index], 1, sizeof(scenario->squads[squad_index]), file);
    }

    // Constants
    uint32_t constant_count = scenario->constants.size();
    fwrite(&constant_count, 1, sizeof(constant_count), file);
    for (uint32_t constant_index = 0; constant_index < constant_count; constant_index++) {
        fwrite(&scenario->constants[constant_index], 1, sizeof(scenario->constants[constant_index]), file);
    }

    fclose(file);
    log_info("Exported scenario %s.", full_path);
    return true;
}

Scenario* scenario_import(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        log_error("Scenario import - could not open file %s for reading.", path);
        return NULL;
    }

    // Signature
    uint32_t signature;
    fread(&signature, 1, sizeof(signature), file);
    if (signature != SCENARIO_FILE_SIGNATURE) {
        log_error("Scenario import - signature does not match.");
        fclose(file);
        return NULL;
    }

    Scenario* scenario = new Scenario();

    // Map type
    fread(&scenario->map_type, 1, sizeof(scenario->map_type), file);

    // Raw map
    scenario->raw_map = raw_map_fread(file);

    // Bake seed
    fread(&scenario->map_bake_lcg_seed, 1, sizeof(scenario->map_bake_lcg_seed), file);

    // Player spawn
    fread(&scenario->player_spawn, 1, sizeof(scenario->player_spawn), file);

    // Players
    fread(scenario->players, 1, sizeof(scenario->players), file);

    // Allowed entities
    fread(scenario->player_allowed_entities, 1, sizeof(scenario->player_allowed_entities), file);

    // Allowed upgrades
    fread(&scenario->player_allowed_upgrades, 1, sizeof(scenario->player_allowed_upgrades), file);

    // Bot config
    fread(scenario->bot_config, 1, sizeof(scenario->bot_config), file);

    // Entities
    fread(&scenario->entity_count, 1, sizeof(scenario->entity_count), file);
    for (uint32_t entity_index = 0; entity_index < scenario->entity_count; entity_index++) {
        fread(&scenario->entities[entity_index], 1, sizeof(scenario->entities[entity_index]), file);
    }

    // Squads
    uint32_t squad_count;
    fread(&squad_count, 1, sizeof(squad_count), file);
    for (uint32_t squad_index = 0; squad_index < squad_count; squad_index++) {
        ScenarioSquad squad;
        fread(&squad, 1, sizeof(squad), file);
        scenario->squads.push_back(squad);
    }

    // Constants
    uint32_t constant_count;
    fread(&constant_count, 1, sizeof(constant_count), file);
    for (uint32_t constant_index = 0; constant_index < constant_count; constant_index++) {
        ScenarioConstant constant;
        fread(&constant, 1, sizeof(constant), file);
        scenario->constants.push_back(constant);
    }

    fclose(file);
    log_info("Imported scenario %s.", path);

    return scenario;
}

void scenario_export_all() {
    const uint32_t scenario_count = 12;
    for (uint32_t index = 0; index < scenario_count; index++) {
        char scenario_path[256];
        sprintf(scenario_path, "../scenario/scenario%u/scenario%u.json", index + 1, index + 1);
        Scenario* scenario = scenario_open_file(scenario_path);
        sprintf(scenario_path, "../scenario/scenario%u/scenario%u.scn", index + 1, index + 1);
        scenario_export(scenario, scenario_path);
        scenario_free(scenario);
    }

    log_info("Finished.");
}
