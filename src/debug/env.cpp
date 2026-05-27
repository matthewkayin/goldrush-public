#include "env.h"
#include "match/bot/config.h"

#ifdef GOLD_DEBUG

#include "core/logger.h"
#include "util/json.h"

static Env env;

const Env& env_get() {
    return env;
}

bool env_json_validate_type(Json* env_json, const char* key, JsonType type) {
    Json* value_json = json_object_get(env_json, key);
    if (value_json == NULL) {
        log_warn("Env var %s not found.", key);
        return false;
    }
    if (value_json->type != type) {
        log_warn("Env var %s not found.", key);
        return false;
    }
    return true;
}

void env_json_read_int(Json* env_json, const char* key, int* value) {
    if (env_json_validate_type(env_json, key, JSON_TYPE_NUMBER)) {
        *value = (int)json_object_get_number(env_json, key);
    }
}

void env_json_read_boolean(Json* env_json, const char* key, bool* value) {
    if (env_json_validate_type(env_json, key, JSON_TYPE_BOOLEAN)) {
        *value = json_object_get_boolean(env_json, key);
    }
}

bool env_json_read_string(Json* env_json, const char* key, std::string* value) {
    if (env_json_validate_type(env_json, key, JSON_TYPE_STRING)) {
        *value = std::string(json_object_get_string(env_json, key));
        return true;
    }
    return false;
}

void env_init() {
    // Set env to defaults
    env.rand_seed = 0;
    env.desync_debug = false;
    env.use_resource_pack = false;
    env.test_mode_unit_comp = BOT_UNIT_COMP_NONE;

    // Load env file
    Json* env_json = json_read("../env.json");
    if (env_json != NULL) {
        env_json_read_int(env_json, "rand_seed", &env.rand_seed);
        env_json_read_boolean(env_json, "desync_debug", &env.desync_debug);
        env_json_read_boolean(env_json, "use_resource_pack", &env.use_resource_pack);

        std::string test_mode_unit_comp_str;
        if (env_json_read_string(env_json, "test_mode_unit_comp", &test_mode_unit_comp_str)) {
            env.test_mode_unit_comp = bot_config_unit_comp_from_str(test_mode_unit_comp_str.c_str());
        }

        json_free(env_json);
        log_info("Loaded env.");
    } else {
        log_info("No env json found. Creating with defaults.");
    }

    // Write env
    env_json = json_object();
    json_object_set_number(env_json, "rand_seed", env.rand_seed);
    json_object_set_boolean(env_json, "desync_debug", env.desync_debug);
    json_object_set_boolean(env_json, "use_resource_pack", env.use_resource_pack);
    json_object_set_string(env_json, "test_mode_unit_comp", bot_config_unit_comp_str(env.test_mode_unit_comp));
    bool success = json_write(env_json, "../env.json");
    json_free(env_json);
    if (!success) {
        log_error("Failed to write env json.");
    }
    log_info("Wrote env json.");
}

void env_get_rand_seed_override(int* lcg_seed) {
    if (env.rand_seed == 0) {
        return;
    }
    *lcg_seed = env.rand_seed;
}

#else

void env_init() {}
void env_get_rand_seed_override(int* /*lcg_seed*/) {}

#endif
