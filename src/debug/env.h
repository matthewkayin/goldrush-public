#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "match/bot/config.h"

struct Env {
    int rand_seed;
    bool desync_debug;
    bool use_resource_pack;
    BotUnitComp test_mode_unit_comp;
};
const Env& env_get();

#endif

void env_init();
void env_get_rand_seed_override(int* lcg_seed);
